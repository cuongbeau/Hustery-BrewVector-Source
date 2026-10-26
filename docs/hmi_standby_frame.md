# Mốc 7 — khung màn chờ HMI (2026-09-06)

Người dùng đã duyệt khung rồi bổ sung History và Flush. Phạm vi lần này là
màn chờ và nối thao tác với chức năng hiện có; chưa gỡ TFT hoặc thiết kế lại
chi tiết màn pha, kết quả, History, calibration, cài đặt và Web.

## Khung đang triển khai

HMI LovyanGFX 320x240, giữ màu/font/nhãn tiếng Anh đang dùng:

1. Trạng thái Main và đèn kết nối cân/Wi-Fi.
2. Nhiệt độ lớn lấy telemetry Main; mất Main hoặc NaN hiển thị `--.-`.
   Đây là nhiệt độ đo được, không phải target heater hay điều kiện cho phép pha.
3. Vùng profile có thể chạm để chọn. Tên dài dùng font nhỏ hơn; wire hiện
   giới hạn tên23 byte. Chỉ cập nhật từ Main, không giả lập đã chọn thành công.
4. Start brew rộng toàn hàng; Manual và Flush ở hàng riêng.
5. Gram cân trực tiếp, History và Settings. Mất cân hiện `Scale offline`;
   không giữ trạng thái online cũ khi telemetry Main ngừng đến.

`hmi/src/ui/ready_layout.h` dùng chung hình chữ nhật cho vẽ và hit-test.
Chạm vùng profile/History/Settings hoặc khoảng trống không được chạy bơm.
Không chuyển màn chạy trước khi có telemetry Main. Khi Main offline, khác
schema, không Ready hoặc còn pending command, khóa thao tác vận hành; vẫn
truy cập History/Settings. Profile OFF hoặc catalog chưa khớp khóa Start.

Manual tra flag từ catalog Main rồi gửi SelectProfile, không hardcode index
và không tự Start. Main báo chọn xong thì hiện MANUAL ACTIVE. HMI tự yêu cầu
catalog khi mới mở hoặc tên profile từ Main không khớp catalog; retry tối đa
một lần/giây khi cần. Chưa hiển thị duty dự kiến lúc chờ vì wire chỉ có duty
đang xuất, không có giá trị Manual lưu trong Main; tránh trình bày 0% như setpoint.

Flush gửi StartFlush argument0, dùng mặc định Main hiện5 s. Chưa có trang
cấu hình duration mới. Màn Flush có countdown, Stop và progress theo duration
Main gửi, không hardcode progress5 s. Flush hiện không đi qua auto-tare, không
được Main phát ControlTrace như shot, nên không thành bản ghi pha trong History.

## Những quyết định còn phải thực hiện ở mốc sau

- Người dùng bỏ nhấn giữ encoder để dừng pha; dừng bằng HMI/nút zin máy.
  Encoder chỉ chỉnh duty khi pha Manual. Recovery giữ5 giây khi chờ là đề
  xuất đã ghi trong trao đổi; chưa sửa state_machine/encoder trong mốc này.
- HMI calibration sẽ có +/- công suất thay encoder; chưa sửa wizard lần này.
- History HMI xem nhanh danh sách/tổng kết/biểu đồ; khi không có cân hợp lệ,
  yield phải là `—`. So sánh nhiều shot/phân tích/CSV/quản lý trên Web.
  Đây là hướng thiết kế tiếp theo, không khẳng định màn History cũ đã đổi đủ.
- Chi tiết màn pha/History/Cài đặt cần thảo luận với người dùng trước code.
  TFT vẫn trong firmware Main. Heater SSR, tune áp/ULKA, cân để lại.

## Kiểm chứng

- `tests/hmi_ready/check.ps1` PASS: C++11 static_assert hit-test các nút,
  navigation không thành lệnh bơm, ngoài màn/khe giữa nút không kích hoạt,
  disabled action và vùng Stop Flush đúng giới hạn.
- `pio run -d hmi` SUCCESS: RAM96836 B (29.6%), flash1545397 B (23.6%).
  Bin1545760 B, SHA256:
  `7771C9D0EA267A3BEA8BA1C67E5ED02E9F4CF8A4A41E60F3E875618EAC76260E`.
- Output `hmi/.pio/build/esp32-s3-devkitc-1/firmware.bin`, schema13.
  Main2150 và firmware cân không sửa, không build/nạp lại.
- Chưa upload HMI, chưa xác nhận hình ảnh/vùng chạm trên màn thật. Không có
  browser runtime của màn LovyanGFX này; kiểm chứng build/hit-test không thay
  cho kiểm tra khả năng đọc và thao tác trên phần cứng.

Sau nạp: mở trang chính, kiểm tra dữ liệu live; chạm profile/History/Settings
không chạy bơm. Manual chỉ chọn chế độ rồi Start riêng. Flush đếm ngược và
tự dừng hoặc Stop; không thêm History. Ngắt Main thì nút vận hành khóa;
ngắt cân thì gram chuyển offline, vẫn cho phép pha theo policy đã chốt.
