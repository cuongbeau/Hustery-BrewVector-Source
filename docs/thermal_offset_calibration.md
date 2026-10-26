# Hiệu chỉnh nhiệt pha bằng offset — 2026-09-17

## Hành vi

`brewTempEstC = blockTempC - offsetC`, với blockTempC từ bộ lọc SensorReader hiện có.
Bỏ toàn bộ khoản trừ theo flow, đường nước lạnh và tải nước tích lũy. `pathTempEstC`
là alias của brewTempEstC để giữ telemetry cũ; `thermalLoadG` luôn bằng 0.
Nhiệt pha vẫn là ước tính, không phải nhiệt nước đo tại puck.

Offset mặc định 21°C khi chưa lưu; chỉnh từ 0 đến 60°C. Tăng offset làm nhiệt pha
hiển thị thấp hơn. Offset không thay đổi nhiệt cảm biến của heater, setpoint thân,
PID, FF hoặc điều kiện Ready. Đồ thị/CSV mới dùng ước tính mới; không sửa History cũ.

## Thao tác

- Touch HMI: Settings → Calibration → Brew temperature; chỉnh +/- 0,1°C, Save.
- Web: Calib → Brew temperature offset; nhập giá trị, Save offset. Reload đọc lại Main.
- Main chỉ lưu khi IDLE/SHOT_END, van máy không hoạt động; bị khóa trong OTA và
  các phép chẩn đoán đang chiếm quyền. UI khóa lưu khi Main offline/khác schema.
- Đo nhiệt nước trong điều kiện pha ổn định để hiệu chỉnh: offset ≈ nhiệt thân
  được dùng trên giao diện trừ nhiệt nước đo thực tế tại vị trí cần kiểm soát.
  Không suy offset từ setpoint máy zin hay dùng đồ thị phẳng làm bằng chứng nhiệt đúng.

Giá trị dùng chung giữa Web/touch, lưu trên Main bằng Preferences namespace
`thermal-cal`, key `offset-c`. Chỉ áp dụng RAM sau khi `putFloat` thành công;
đọc dữ liệu không hợp lệ giữ mặc định. Offset dùng atomic float để SensorReader
đọc an toàn khi Main loop lưu. Không ghi đè calibration áp/flow hay NVS heater.

## API và giao thức

GET `/cal/thermal`: kết quả `{ "ok": true, "offset_c": 21.0 }`.

POST `/cal/thermal`: `{ "offset_c": 22.5, "expected_offset_c": 21.0 }`.
Trường expected phải là giá trị đã đọc từ Main. Nếu giá trị hiện hành khác,
Main trả Conflict và không ghi; giao diện yêu cầu Reload. JSON thiếu/sai kiểu,
NaN, vô cực hoặc ngoài 0–60 bị từ chối. Dùng WebJobs HTTP202 + polling như Calib cũ.

UART thêm `SetThermalOffset=5`, `CalibrationStatus::Conflict=5`, và
`CalibrationReplyPayload.thermalOffsetC`. SetThermalOffset gửi value1=giá trị mới,
value2=giá trị dự kiến đang lưu. Application schema tăng **15 → 16**;
HeaterWire giữ **4**. Cần nạp Main/HMI cùng schema; tắt heater trước khi cập nhật
hai bên để không mắc kẹt lệnh heater khi khác schema. Chưa thực hiện cập nhật thiết bị.

Touch theo dõi đúng requestId/operation, hết 1,8 s yêu cầu Reload; Web cũng không
tự gửi lại lệnh khi timeout. Khi mở màn hoặc Reload mới đọc lại giá trị, không
lấy 21°C trong bản nháp làm bằng chứng Main đã lưu.

## Kiểm chứng

`tests/thermal/check.ps1`: cú pháp các translation unit Main/HMI liên quan PASS;
Web test cho load, bounds/zero, lưu, single-flight, conflict, timeout/reload PASS;
root gzip khớp source và JavaScript trang ghép parse PASS. Không build/link firmware,
không nạp, chưa kiểm tra panel thật hoặc lưu/khởi động lại trên thiết bị.

Source gốc trước chỉnh nằm trong `_archive/thermal_offset_20260917/`.
