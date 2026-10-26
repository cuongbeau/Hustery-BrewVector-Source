# Giggauino System Architecture

Trạng thái chuẩn hóa: 2026-08-21, protocol schema 10.

## Nguyên tắc cốt lõi

Main ESP32 là bộ điều khiển thời gian thực và phải pha được khi HMI mất kết
nối. HMI ESP32-S3 là lớp giao diện, kết nối và lưu trữ dài hạn. HMI chỉ gửi
yêu cầu; Main kiểm tra trạng thái rồi mới tác động relay, pump hoặc dữ liệu
điều khiển.

```text
BLE scale ──> HMI ESP32-S3 ── ScaleSample UART ──> Main ESP32
                 │                   ▲                 │
                 ├─ touch UI         ├─ telemetry      ├─ sensors/pump/relay
                 ├─ Wi-Fi + Web      ├─ trace/ACK      ├─ profile + safety
                 ├─ TF History       └─ commands/FW    ├─ fallback TFT
                 └─ audio/BLE                         └─ Recovery AP/OTA
```

## Quyền sở hữu

| Miền | Main ESP32 | HMI ESP32-S3 |
|---|---|---|
| Pump, relay, valve, sensor | Chủ sở hữu duy nhất | Chỉ hiển thị/gửi request |
| State machine và safety | Chủ sở hữu duy nhất | Phản chiếu state |
| Thuật toán pressure/flow | Thực thi | Không can thiệp vòng điều khiển |
| Profile đang chạy | Bản authoritative | Editor và đồng bộ qua UART |
| Pressure/flow calibration | Lưu và áp dụng | Wizard/editor qua UART |
| TFT trên + encoder | Fallback vận hành | Không sở hữu |
| Touch UI và đồ thị | Không sở hữu | Chủ sở hữu |
| Wi-Fi và Web đầy đủ | Không sở hữu | Chủ sở hữu |
| Lịch sử/log shot dài hạn | Không ghi | TF card là authoritative |
| BLE scale | Nhận sample chuẩn hóa qua UART | Pair, driver, reconnect và chuyển mẫu |
| OTA Main | Nhận image qua UART; Recovery OTA dự phòng | Firmware Center điều phối |
| OTA HMI | Không sở hữu | Web OTA trực tiếp |
| Emergency Recovery | AP/web tối giản, bình thường tắt | Không phụ thuộc |

## Cấu trúc Main đang chạy

- `src/hardware`: driver và I/O phần cứng.
- `src/control`: profile executor và thuật toán điều khiển.
- `src/model`: mô hình flow, nhiệt và dự đoán cân.
- `src/app`: orchestration, state machine và persistence điều khiển.
- `src/communication`: giao thức UART với HMI và firmware receiver.
- `src/ui/tft`: TFT fallback trên Main.
- `src/recovery`: AP/web bảo trì tối giản khi HMI hỏng.
- `interfaces` và `protocol`: kiểu dữ liệu và wire contract dùng chung.

Main không còn compile web dashboard cũ, shot logger cũ hoặc shot analyzer cũ.
Các source đó được giữ trong `_archive` để tham khảo, không nằm trong firmware.

### Communication trên Main

- `src/communication/hmi_link.cpp`: UART transport, framing, heartbeat,
  telemetry và diagnostics; không xử lý nghiệp vụ request.
- `src/communication/handlers/hmi_command_handler.cpp`: command và chống gửi
  lặp/deferred stop.
- `src/communication/handlers/hmi_profile_handler.cpp`: list, download,
  upload, checksum và xóa profile.
- `src/communication/handlers/hmi_calibration_handler.cpp`: pressure/flow
  calibration request/reply.
- `src/communication/handlers/hmi_firmware_receiver.cpp`: nhận image Main,
  CRC/SHA-256, ghi OTA partition và reboot.
- `src/hardware/scale_service.cpp`: nhận sample cân BLE do HMI chuyển qua UART;
  khi không còn sample hợp lệ, control dùng predictive weight.
- `src/model/predictive_gate_learning.cpp`: dùng first-drip do cân xác nhận để
  học EMA water/time theo profile. Shot không có cân chỉ đọc prior đã học;
  dữ liệu được ghi `/predictive_gate.json` ở pha IDLE, không ghi flash trong
  control loop hoặc shot-stop path.

State của từng transfer nằm trong module sở hữu nó. Transport chỉ xác nhận một
frame hợp lệ rồi dispatch theo `MessageType`; control layer không phụ thuộc web
hoặc UI của HMI.

## Cấu trúc HMI đang chạy

- `hmi/src/communication`: UART link và contract trao đổi với Main.
- `hmi/src/ui/screen_manager.cpp`: điều hướng, touch dispatch và state UI dùng chung.
- `hmi/src/ui/screens/profile_screens.cpp`: danh sách, editor và preview profile.
- `hmi/src/ui/screens/live_screens.cpp`: Ready, Flush, Brew và Result.
- `hmi/src/ui/screens/settings_screens.cpp`: Wi-Fi, scale, calibration, display,
  storage, system và diagnostics.
- `hmi/src/ui/screens/history_screens.cpp`: lịch sử, chi tiết và so sánh shot.
- `hmi/src/network/ota_service.cpp`: chỉ quản lý Wi-Fi STA/AP, scan,
  reconnect và mDNS; không chứa HTML hay HTTP route.
- `hmi/src/web/hmi_web_server.cpp`: sở hữu HTTP server và lắp các nhóm route.
- `hmi/src/web/routes/dashboard_routes.cpp`: dashboard, live data, profile,
  calibration, Wi-Fi form và HMI settings API.
- `hmi/src/web/routes/history_routes.cpp`: lịch sử và tải CSV từ TF.
- `hmi/src/web/routes/firmware_routes.cpp`: OTA HMI và truyền firmware Main.
- `hmi/src/web/profile_web_bridge.*` và `calibration_web_bridge.*`: chuyển đổi
  web request sang giao thức UART với Main.
- `hmi/src/storage`: lưu lịch sử shot trên TF.
- `hmi/src/scale/scale_manager.cpp`: chuẩn hóa driver BLE/thương mại/simulator
  và phát `ScaleSample` về Main, không chứa quyết định auto-yield.
- `hmi/src/update`: truyền firmware Main qua UART.

Các module màn hình dùng chung `ScreenManager` để giữ nguyên state hiện tại,
nhưng phần render đã tách theo miền chức năng. Việc sửa một nhóm màn hình không
còn yêu cầu mở hoặc biên dịch lại một file giao diện monolith gần 1.700 dòng.

## Dữ liệu persistent trên Main

Main chỉ giữ dữ liệu cần cho vận hành độc lập:

- `/pressure_cal.json`
- `/flow_cal.json`
- `/profiles.json`
- dữ liệu last-shot/adaptive mà thuật toán đang dùng
- không có bảng hold-duty/adaptive pressure state persistent

`/sys_config.json` và `/shot_latest.csv` từ firmware cũ có thể vẫn tồn tại trên
SPIFFS sau OTA nhưng firmware mới không đọc hoặc ghi chúng. Không cần format
SPIFFS vì profile và calibration đang dùng chung partition.

## Luồng khởi động Main

1. Khởi tạo ScaleService để nhận sample cân từ HMI qua UART.
2. Khởi tạo UART HMI.
3. Khởi tạo TFT fallback.
4. Nạp calibration và profile authoritative; rebuild runtime profile.
5. Đăng ký Recovery Portal nhưng giữ AP ở trạng thái tắt.
6. Khởi tạo hardware và control orchestrator.
7. Chạy vòng điều khiển; UART telemetry và Recovery lifecycle chạy độc lập.

## Ràng buộc vòng điều khiển

- Main orchestration/profile tick chạy mỗi 10 ms. ADS acquisition và
  `DimmedPumpCore` chạy ở hai task FreeRTOS độc lập, mỗi task dùng
  `xTaskDelayUntil(..., 30 ms)`; Web/HMI không quyết định cadence feedback.
- `DimmedPumpCore` là owner duy nhất của PressureController + PSM. Profile chỉ
  gửi `POWER/PRESSURE/FLOW`, target, limit và valve state.
- Không chạy shot analytics, user logging hoặc phát hiện cảnh báo chỉ để in
  Serial trong tick này.
- Không tạo JSON, không truy cập WebServer và không ghi SPIFFS trong lúc pump
  đang được điều khiển.
- Dữ liệu persistent ngoài pressure controller được gom ghi sau khi đã vào
  `SHOT_END/IDLE` ổn định 3 giây.
- HMI telemetry và Recovery lifecycle nằm ngoài tick 10 ms; Recovery HTTP chỉ
  được phục vụ khi AP thực sự đang bật.
- Khi Recovery dừng, Main chuyển Wi-Fi về `WIFI_OFF`; không giữ STA/channel cho
  ESP-NOW vì transport này đã bị loại khỏi runtime.

## Độ bền liên kết UART

- Hai đầu gửi heartbeat mỗi 500 ms và coi link mất sau 3,5 giây không có frame
  hợp lệ. Khoảng này chịu được một HTTP request chậm mà không báo mất HMI giả.
- Frame dở quá 120 ms bị hủy để parser không mắc kẹt trên `payloadLength` lỗi
  cho tới khi khởi động lại.
- Khi CRC sai, parser dịch một byte và tìm lại `MAGIC`, không bỏ cả vùng dữ liệu
  theo độ dài chưa đáng tin cậy.
- Main đánh dấu trong telemetry cả trạng thái chiều HMI -> Main. HMI Diagnostics
  và `/api/live` hiển thị revision, tuổi frame, CRC/framing error, stale reset
  và reconnect count cho cả việc phát hiện lỗi một chiều.

## Recovery khi mất HMI

- TFT cảnh báo sau 3 giây mất link tương thích.
- Sau 60 giây, nếu máy đang IDLE/SHOT_END và valve không hoạt động, Main bật AP
  `Giggauino-Recovery-xxxx`.
- Recovery chỉ có status, OTA Main và reboot.
- Khi HMI tương thích trở lại ổn định 10 giây, Recovery tự tắt nếu được bật tự
  động.
- Control loop vẫn thuộc Main. HMI mất không làm máy mất khả năng pha bằng nút
  zin, encoder và TFT.

## Luật dependency

1. `hardware`, `control`, `model` không include HMI web/UI.
2. HMI không được đặt pump/relay trực tiếp; mọi command phải qua request/reply.
3. Recovery không được tải profile, calibration hay chạy analytics.
4. Main không lưu lịch sử shot phục vụ người dùng; HMI/TF đảm nhiệm việc đó.
5. Thay đổi wire payload phải tăng `SCHEMA_REVISION` khi không tương thích.
6. Mọi lệnh từ HMI phải được Main kiểm tra state, giới hạn và điều kiện safety.

## Phần phát triển tiếp theo

- Tune pressure tracking bằng Control Trace schema 10 đã lưu trên TF.
- Kiểm chứng driver Acaia/Timemore bằng phần cứng thật trước khi tuyên bố tương
  thích đầy đủ.
- Tiếp tục tinh chỉnh hai giao diện độc lập với control layer.
- Mở rộng phân tích History trên Web khi cần; không khôi phục logger
  text/SPIFFS trong Main.
