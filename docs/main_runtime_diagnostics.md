# Chẩn đoán Main và lỗi heater

Trong Web → PID nhiệt → Chẩn đoán & lịch sử lỗi, có thể xem phiên bản Main, lý do khởi động, boot ID, uptime, RAM và sự kiện heater khóa lỗi gần nhất.

## Đọc trạng thái

- `GET /api/main/diagnostics`: build, resetReason, bootId/bootCount, uptime, heap, chế độ ADC và lastFault.
- `GET /api/heater`: trạng thái PID, nhiệt thô/lọc, công suất, bộ đếm cảm biến, pause và trạng thái xác nhận quá nhiệt.
- Chỉ xem dữ liệu là hiện hành khi `online=true`; chẩn đoán Main quá hạn sau 3 giây không nhận frame mới.
- `lastFault.bootId` khác `bootId` hiện tại nghĩa là lỗi thuộc lần khởi động trước. `atMs` tính từ lúc khởi động của sự kiện, không phải giờ lịch.
- `faultSaved=true` xác nhận sự kiện đã được lưu NVS. ClearFault chỉ xóa khóa heater, không xóa bản ghi chẩn đoán.

Mã lỗi heater: 1 Sensor, 2 Stale, 3 Overtemp, 4 MainStalled. Mã lỗi OC/SCG/SCV là quan sát từ chip cảm biến, chưa đủ kết luận nguyên nhân phần cứng.

## Lưu trữ và giao thức

Main giữ hai slot RTC có checksum và sequence. Khi khóa lỗi, task heater chụp nhiệt thô/lọc, tuổi mẫu, khung cảm biến, mật độ lỗi và cờ chạy nước vào RAM/RTC. Không cấp phát hoặc ghi flash trong đường xử lý mẫu/SSR.

Vòng Main chỉ ghi sự kiện đang chờ vào NVS khi máy IDLE, heater OFF/Fault và đầu ra bằng 0, ngoài phiên OTA. Khởi động ghi boot count một lần; ghi thất bại được thử lại cách ít nhất 5 giây. NVS dùng namespace `bv-diag`, không thay heater config/calibration.

RTC không được bảo đảm qua mất điện/brownout; sự kiện chưa lưu có thể mất. Chỉ giữ lỗi khóa gần nhất và tổng số sự kiện, không phải nhật ký đầy đủ hoặc stack trace.

Frame tùy chọn `0x3B`, version 1, 92 byte; application schema 17 và HeaterWire 4 không đổi. Main gửi khoảng 1 Hz nếu UART còn chỗ. HMI cũ không hiển thị phần mở rộng; HMI mới với Main cũ báo chưa có chẩn đoán.

## Biểu đồ và CSV

Biểu đồ PID bắt đầu phiên mới khi boot ID đổi hoặc uptime giảm, tránh vẽ mẫu sau reboot ra ngoài trục. CSV vẫn giữ các phiên trước trong RAM trình duyệt, tối đa 7200 dòng, và thêm cột boot ID. Reload/đóng tab làm mất dữ liệu chưa tải.

Trang ghi ADC cập nhật trạng thái nền mà không vô hiệu hóa toàn bộ nút mỗi lần poll. Các thao tác ghi/tải vẫn được tuần tự hóa với yêu cầu đang chạy.

## Watchdog và kiểm chứng

Main block một tick bằng `vTaskDelay(1)` ở cuối loop, cả trong OTA, để idle task CPU 1 có thời gian chạy. Giữ watchdog 5 giây và bảo vệ actuator; ADC/pump/heater vẫn có task riêng.

Bản Main/HMI hiện hành đã build và nạp trên máy dự án ngày 05/11/2026. Đã xác minh API, chẩn đoán qua UART, dữ liệu retained fault, biểu đồ Web và OTA Main 100% không recovery. Điều này không xác nhận mọi tình huống tải hay nguyên nhân lỗi cảm biến đã được xử lý.

Kiểm tra liên quan: `tests/heater/diagnostic_record.cpp`, `main_diagnostics_web_test.cjs`, `chart_session_test.cjs`, `transport_test.cjs`. Xem thêm [bộ lọc và bảo vệ nhiệt](heater_temperature_filter.md).
