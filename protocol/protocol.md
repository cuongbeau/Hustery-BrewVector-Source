# Main Controller ↔ HMI Protocol

## Hysteresis lỗi MAX31855 — 2026-09-17

Vòng điều khiển vẫn giữ 20 kết quả đọc ở nhịp 250 ms. Từ 4 lỗi cảnh báo;
14 lỗi tạm dừng SSR; chỉ tự chạy lại khi còn tối đa 8 lỗi; 18 lỗi mới latch
Sensor. Các mốc tuổi mẫu tốt >1,5 s tạm dừng, >5 s khóa, quá nhiệt và Main
stalled giữ nguyên. HeaterSensorStatus 0x3A version 8 thêm
controlWarnErrors/controlPauseErrors/controlResumeErrors và densityPaused;
Main và HMI phải cùng bản để hiện đủ telemetry. Chưa build/nạp.

## Tạm dừng nhiệt 1,5s và khóa 5s — 2026-09-08

Source mới: >1,5s tạm ngừng heater/I; >5s mới latchStale, mẫu tốt trước
hard fault tự tiếp tục có guards, không tích phân bù. Mục 10/20 ở đây đã
được thay bởi hysteresis 4/14/8/18 ở trên. Web/HMI có trạng thái tạm dừng riêng. Kiểm tra PASS; chưa
build/link/nạp bản hai mức. Bản1s trước đã build/nạp/chạy thực nghiệm.
Optional0x3A v4 nối uint16 pauseGoodAgeMs và uint8 paused; GET/CSV dùng sensorPauseGoodAgeMs/sensorPaused. maxGoodAgeMs=5000; schema15/HeaterWire2 không đổi.
Hysteresis ở mục đầu thay các ghi chú trước đó.

## Chính sách bỏ mẫu lỗi ADC — 2026-09-08

Optional0x3A HeaterSensorStatus version3 nối uint8 controlSamples/controlErrors/
controlCapacity/controlTripErrors, uint16 samplePeriodMs/maxGoodAgeMs vào cuối
layoutv2. Header packed, matching sizeof/version bắt buộc. HeaterWire2 và
application schema15 không đổi; khác diagnostic version chỉ unavailable
diagnostics, không chặn control/OTA. Source chưa build/nạp; mục v1/v2 bên dưới là lịch sử.

## Application schema 15 — Heater V2 (2026-09-08)

Framing version1 giữ nguyên. `HeaterRequest`0x38 / `HeaterStatus`0x39,
capability bit3 `SupportsHeater`, packed payload version2 tại `heater_wire.h`.
Config bổ sung autoSleep, sleepDelaySeconds, stableSeconds; status thêm ready.
Command StartBrew khi heaterOFF chỉ Preheat; khi chưaReady từ chối pha.
Main/HMI phải cùng schema15 để điều khiển. Firmware transfer không yêu cầu
application schema khớp; nâng cấp HMI trước rồi Main (guard HMIv1 có thể chặn
self-OTA nếu đã nâng Main lên15 trước). Xem `docs/heater_pid.md` mục V2.

## Vật lý

- UART 3.3 V, full duplex, 230400 baud, 8N1.
- HMI TX GPIO43 → Main RX GPIO39 (VN).
- HMI RX GPIO44 ← Main TX GPIO1 (chân in `TX`).
- Hai bo nối chung GND.

GPIO39 là input-only và phù hợp làm UART RX. GPIO1 được UART1 chiếm dụng sau
khi khởi động, nên log Serial runtime trên cổng USB-UART của Main sẽ không còn
được đưa ra chân TX. HMI RX là đầu vào trở kháng cao nên không cản bootloader.
Nên đặt điện trở nối tiếp 470 Ω đến 1 kΩ trên mỗi đường TX/RX.

## Frame

```text
MAGIC | VERSION | TYPE | LENGTH | SEQUENCE | PAYLOAD | CRC16-CCITT
```

- `MAGIC`: `0xA55A`.
- `VERSION`: phiên bản giao thức, bắt đầu từ `1`.
- `SEQUENCE`: chống lặp command và ghép ACK/NACK.
- `CRC16`: tính trên header và payload.

## Nhịp dữ liệu dự kiến

- Telemetry khi brew: 20–25 Hz.
- Control trace khi brew/hot water: 25 Hz, chỉ gửi khi UART TX còn chỗ; sample
  chẩn đoán có thể bị bỏ nhưng không bao giờ làm Main chờ.
- Telemetry khi idle: 2–5 Hz.
- Heartbeat: 2 Hz.
- State/event: gửi ngay khi thay đổi.
- Scale sample: 20–50 Hz tùy cân, luôn kèm timestamp.

## Nguyên tắc an toàn

- HMI chỉ gửi request; ESP chính quyết định và trả ACK/NACK.
- ESP chính là nguồn trạng thái chuẩn duy nhất.
- Command cũ hoặc trùng sequence không được thực thi lại.
- Main đánh dấu weight từ HMI stale nếu không có sample mới trong 750 ms; khi
  mất cân thì dùng predictive weight theo flow model.
- Profile cần target weight không được Start nếu scale/HMI không hợp lệ.
- Mất heartbeat HMI không được làm thay đổi output pump/heater của Main.

## Manual PSM diagnostic — additive command, schema 11

`Command::ResetStoppedPsm=9`, `argument=0`, dùng nguyên CommandRequest/Reply.
Main chỉ nhận ở IDLE, không OTA/remote start pending, pump/valve/sense OFF.
Request được owner kiểm tra lại và thực hiện trong tối đa 250 ms hoặc từ chối.
Accepted là đã thực hiện, không phải chỉ enqueue. Không thay payload/CSV.
Main cũ trả InvalidCommand; HMI phải hiển thị unsupported. Không tự retry khi
timeout/mất ACK. Reset thủ công này không được coi là cách sửa lỗi ổn định.

## ScaleSample — schema revision 11

HMI chuẩn hóa mọi driver cân thành `ScaleSamplePayload` và gửi về Main. Payload
gồm `sourceTimestampMs`, `sampleSequence`, `weightG`, `sampleAgeMs`, `source`,
flags `connected/stable/battery-valid/simulated` và phần trăm pin tùy chọn.

Thứ tự chọn nguồn trên Main:

1. Mẫu HMI hợp lệ và chưa quá 750 ms (`BLE CUSTOM`, `BLE COMMERCIAL` hoặc
   `SIMULATOR`).
2. Predictive weight khi không còn cân vật lý.

Việc lặp lại một packet cũ không thể giữ cân online vì Main tái tạo thời điểm
mẫu gốc từ `sampleAgeMs`. Simulator chỉ là công cụ chẩn đoán, có xác thực web
và không được bật khi Brew, Flush hoặc Hot Water đang chạy.

## ControlTrace + ShotEnd — schema revision 11

`ControlTracePayload` là snapshot nhị phân fixed-size gồm trạng thái thực tế
(áp raw/filtered, flow, cân thực, model yield độc lập, nhiệt), mục tiêu
profile/controller, duty/CPS, base/P/I/damping, calibration flow, các giới hạn
và cờ saturation. Main chỉ gửi frame này qua
UART không-blocking. HMI đệm, chuyển sang CSV và ghi TF; vì thế filesystem,
formatting CSV hoặc thẻ chậm không nằm trong đường điều khiển pump.

Schema17 đổi calibration Flow sang hai điểm full-power Q1/Q9 tại 1/9 bar và
đặt tên rõ hai kênh trace là Pump Flow/Puck Flow. Target Flow của profile và
đường Flow chính là Pump Flow; Puck Flow chỉ dùng cho phân tích.

Mỗi lần Main bắt đầu Brew/Hot Water, nó tạo một `shotSessionId` khác 0 và giữ
nguyên mã này trong toàn bộ trace của phiên. HMI dùng mã phiên làm nguồn sự
thật để gom sample vào đúng một file; không suy đoán shot mới từ timestamp hoặc
độ trễ UART. Khi shot kết thúc, Main gửi `ShotEndPayload` trước telemetry
`DONE`; payload chứa nguồn dừng, nguyên nhân thoát profile, step, thời gian và
weight cuối. HMI ghi chúng thành footer `# stop_*` rồi flush/đóng file. Timeout
2 giây chỉ dùng để đóng file dang dở khi đường UART thực sự mất.

Schema 11 nối thêm snapshot timing PSM khoảng 1 Hz vào cuối
`ControlTracePayload`. Snapshot gồm raw/accepted/rejected ZC, interval,
accumulator output pattern, gate run và pump-task jitter; nó được lặp trong
trace 25 Hz và de-duplicate bằng `diagSequence`. Vị trí các field biểu đồ cũ
không thay đổi.
### Optional HeaterSensorStatus 0x3A (2026-09-08)

`heater_sensor_wire.h`, version1, Main -> HMI mỗi500ms best effort, fresh2s.
Đây là diagnostics optional: giữ application schema15 và HeaterWire2,
không thay layout các message cũ. Peer cũ bỏ qua message mới; HMI mới với
Main cũ hiện chẩn đoán unavailable.
### HeaterSensorStatus 0x3A version2 — thống kê 5s

Source 2026-09-08 thêm windows/counts/streaks vào heater_sensor_wire.h.
HeaterSensorWire::VERSION=2, application schema15 và HeaterWire2 không đổi.
Peer chỉ chấp nhận đúng sizeof/version; cặp khác diagnostic version sẽ
không có diagnostic fresh nhưng không chặn control/OTA; các mô tả version1 dưới là lịch sử.

Version8 hiện tại giữ các trường cũ và nối ngưỡng warn/pause/resume/trip cùng
cờ densityPaused. Peer vẫn chỉ chấp nhận đúng `sizeof` và `VERSION`.
