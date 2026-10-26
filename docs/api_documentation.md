# Web API hiện hành

Đối chiếu source 2026-09-18; mô tả code, không xác nhận firmware đang nạp. HMI ESP32-S3 phục vụ Web; Main Recovery là server riêng. Application schema17 (pump-flow Q1/Q9), HeaterWire4; payload chuẩn ở `protocol/hmi_protocol.h`, `protocol/heater_wire.h`.

Đường dẫn source bên dưới tính từ gốc dự án. Route chính ở `hmi/src/web/routes/`; kiểm tra handler khi cần toàn bộ trường/giới hạn. Không giả định tất cả route cùng cơ chế xác thực; giữ xử lý auth hiện có, đặc biệt heater/OTA/diagnostic.

## Offset nhiệt pha

GET/POST `/cal/thermal` dùng WebJobs. GET trả `ok`, `offset_c`; POST nhận số `offset_c` và `expected_offset_c` trong0–60°C, chỉ lưu khi Main nghỉ. Giá trị dự kiến khác Main trả conflict; timeout cần đọc lại, không tự retry. [Giao thức và kiểm chứng](thermal_offset_calibration.md).

## Ba cơ chế bất đồng bộ

| Cơ chế | Gửi / theo dõi | Thành công |
|---|---|---|
| Web job | HTTP202 `{"job":id}` → GET /api/hmi/job?id=id | Poll trả kết quả HTTP/body của công việc |
| Heater ticket | POST /api/heater → HTTP202 `{"ticket":id}` → GET /api/heater | online, ticket khớp, result=0 |
| Diagnostic Main | Request ID hoặc ticket theo từng route | ACK/kết quả Main tương ứng, không chỉ HTTP202 |

Job có4 slot. Job chưa hoàn tất không bị tái dùng; sau hoàn tất, slot có thể tái dùng khi đã đọc hoặc qua30 giây. Job không tìm thấy trả404/result unknown; không tự gửi lại mutation. HTTP409 có thể là bận/từ chối, không mặc định retry. Nguồn: `hmi/src/web/web_jobs.cpp`.

## Dashboard, profiles và calibration

| Method | Route | Hành vi |
|---|---|---|
| GET | /, /dashboard-v2, /network, /history | Trang Web/compatibility/redirect |
| GET | /api/live, /dash/live | Telemetry; kiểm tra online/compatible/freshness trước dùng số liệu |
| GET | /dash/profiles | Catalog compact |
| GET | /dash/set_profile?idx=N | Chọn profile, làm thay đổi máy |
| GET | /dash/start, /dash/stop, /dash/flush | Lệnh pha/dừng/xả; GET ở đây không phải chỉ đọc |
| GET | /dash/set_yield, /dash/set_manual | Đăng ký compatibility nhưng trả unavailable |
| GET | /status | Status compatibility qua job |
| GET/POST | /profiles | Đọc/ghi profile qua job/bridge |
| POST | /profile/save, /profile/delete | Lưu/xóa qua job |
| GET/POST | /cal/pressure, /cal/flow | Calibration qua job/bridge |

GET /profiles khi job hoàn tất trả `{ok,version,profiles:[...]}`, profile canonical GaggiMate. Save dùng `{index,profile}`; delete dùng `{index}`. Index custom bắt đầu0, khác index catalog có Manual/OFF; append theo quy ước bridge. POST /profiles có hợp đồng batch riêng; đọc `hmi/src/web/profile_web_bridge.cpp` trước sử dụng. Không gửi schema step legacy. Main kiểm tra state/validation và lưu; Save không tự activate. Codec từ chối valve=0.

Pressure POST: `{cmd:"zero"}` hoặc `{cmd:"p_at",bar:number}`. Pump Flow POST nhận `{q1_ml_s,q9_ml_s}` với `Q1 > Q9 > 0`; GET trả hai điểm cùng diagnostic hiện tại. Main nội suy đường full-power giữa 1 và 9 bar; target Flow trong profile là Pump Flow. Nguồn: `hmi/src/web/routes/dashboard_routes.cpp`, `hmi/src/web/calibration_web_bridge.cpp`.

## Heater

GET /heater là trang có auth; GET/POST /api/heater dùng auth cùng handler.

POST JSON:
- `op=0`: Off; `op=1`: Configure; `op=2`: Enable; `op=3`: Test; `op=4`: ClearFault.
- Configure cần đủ numeric: target, ceiling, kp, ki, kd, maxPower, waterMaxPower, idleMinutes, heaterW, inletC, ffScale; boolean autoSleep, ffEnabled; integer sleepDelaySeconds, stableSeconds.
- Test cần testPower numeric và testSeconds integer1..60. Main kiểm tra công suất và state.
- Route kiểm tra JSON/OTA/link, Main kiểm tra cấu hình và guards. HTTP400/409 là lỗi ở bước nhận/gửi; HTTP202 chỉ có ticket. Result Main:0 Ok,1 Invalid,2 Busy,3 Unsafe.

Giới hạn cấu hình chuẩn ở `src/control/heater_core.h::validConfig`: target30..130°C, ceiling ít nhất target+5 và tối đa150°C; kp/kd0..100, ki0..10; hai trần >0..100%; idleMinutes1..240; sleepDelaySeconds0..3600; stableSeconds0..300; heaterW100..4000; inletC0..60; ffScale0..1. Không coi miền hợp lệ phần mềm là thông số đã hiệu chuẩn cho máy.

GET trả nhóm:
- Trạng thái: online, ticket, result, mode, fault, ready, valid, relay, uptime, sampleAge, idleRemaining, idleExpired.
- PID/config: raw, filtered, power, p/i/d và các trường Configure, activeMaxPower.
- Diagnostic: sensorDiagnosticsFresh, sensorTotal/Errors/Issues, frame và timestamp, cửa sổ5 s/peak/streak; sensorControlSamples/Errors/Capacity/TripErrors và ngưỡng tuổi mẫu.
- FF: thermalFeedforward, inletFlow, feedforwardKff, flowQualified, inletValveOpen, pidPeriodMs, inletPressure, inletDuty.

Chỉ dùng diagnostic khi sensorDiagnosticsFresh và online. valid là tính hợp lệ lần đọc mới; sampleAge là tuổi mẫu tốt, không phải tuổi mọi lần đọc. Pause1,5 s/hard latch5 s độc lập với vòng20 mẫu (warn4, pause14, resume8, trip18). flowQualified là điều kiện model, không phải flowmeter xác nhận. Nguồn field đầy đủ: `hmi/src/web/routes/heater_routes.cpp`.

## Chẩn đoán

| Route | Request / đọc kết quả |
|---|---|
| GET /api/hmi/diagnostics | Runtime heap/loop/UART/BLE/TF counters, có auth; maxima tích lũy không phải thời gian trung bình |
| GET /diagnostics/pump | Trang reset PSM |
| POST /api/pump/reset-stopped | Không cần body;202 ticket/state pending |
| GET /api/pump/reset-stopped | none/pending/applied/rejected/unsupported/unknown; chỉ applied xác nhận owner đã reset |
| GET /diagnostics/adc | Trang probe |
| POST /api/pressure/adc-probe | JSON request_id,run_id,op; op0 Status/1 Start/2 Cancel |
| GET /api/pressure/adc-probe?id=... | Report theo request ID |
| GET /diagnostics/pressure | Trang capture |
| POST /api/pressure/capture | JSON request_id,capture_id,offset,op; op0 Status/1 Arm/2 Read/3 Cancel/4 Clear |
| GET /api/pressure/capture?id=... | Reply theo request ID |

Probe/capture cần capability tương ứng; không bỏ pump hold hoặc tải bulk khi state bị chặn. Reset PSM chỉ được thực hiện thủ công khi bơm đã dừng.

## History, settings và OTA

| Method | Route | Ghi chú |
|---|---|---|
| GET | /traces/list | Job đọc TF; poll lấy danh sách |
| GET | /traces/download?id=N | CSV attachment |
| GET | /traces/download?id=N&offset=B | Đọc tối đa16.384 byte từ offset; HTTP200, header X-Trace-Total;416 nếu offset sai |
| GET | /traces/latest.csv | CSV gần nhất |
| GET | /sys/wifi_scan | Scan SSID |
| POST | /sys/wifi_save, /wifi | Lưu/kết nối mạng; xem tên trường theo handler |
| GET/POST | /hmi/settings | Cấu hình HMI |
| POST | /hmi/test-sound, /api/scale/simulator | Có tác động; không gọi như health check |
| GET | /firmware | Firmware Center |
| GET/POST | /update | Trang/upload OTA HMI |
| POST | /main-update | Upload image Main vào TF |
| GET | /main-update/status | Trạng thái updater |
| POST | /main-update/start, /main-update/cancel | Cài đặt/hủy Main |

Chunk CSV là byte, có thể cắt giữa ký tự UTF-8; ghép byte hoặc dùng decoder streaming. Header/metadata trace theo file thực, xem [CSV guide](../.agents/tune_csv_guide.md). Không dùng schema UART để suy mọi CSV cùng nghĩa.

OTA dùng handler multipart và guard riêng, không phải Web job chung. Giữ interlock heater/pump/valve và đúng target image; không đưa thứ tự nâng cấp cố định cho mọi cặp firmware.

## Main Recovery

Khi Recovery AP bật: GET / và /status; GET/POST /update để OTA Main; POST /reboot nếu an toàn. Nguồn `src/recovery/recovery_portal.cpp`. Không có dashboard/profile editor/history.

Bản trước dọn: [hồ sơ gốc](../_archive/knowledge_cleanup_phase3_20260911/api_documentation.md).

