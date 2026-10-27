# Trình chỉnh profile theo khối

Bản Web HMI ngày 27/10/2026 dùng hàng khối phase và vùng chỉnh đổi màu theo chế độ. Áp suất: cyan; lưu lượng: vàng; công suất: tím; nghỉ: xám. Bản hiện hành có bốn dòng Bơm, Diễn biến, Chuyển bước và Giới hạn. Chạm số để sửa ngay tại chỗ; không có bộ cộng/trừ hay bảng chỉnh nổi. Đồ thị toàn bài nằm dưới, tóm tắt bằng câu chữ thu gọn.

## Tích hợp thiết bị

- Dùng JSON canonical `phases/pump/transition/targets` đang chạy trên Main. Không lưu schema `brewvector.profile.draft-1` của bản HTML thử.
- Danh sách tải qua `/profiles`, chi tiết tải lười qua `/profile/detail?index=…`. Lưu qua `/profile/save`, giữ cơ chế HTTP 202/job polling và chỉ gửi một POST. Bản nháp được giữ khi lưu lỗi; rời tab vẫn hỏi khi có thay đổi chưa lưu.
- Thêm, nhân bản, đổi thứ tự, xóa phase qua menu; kéo grip đổi thứ tự trên desktop. Menu là cách tương đương cho touch/bàn phím.
- Điều kiện kết thúc là các ngưỡng riêng; không có bộ chọn VÀ/HOẶC. Runtime kết thúc khi đạt một điều kiện. Gram tính toàn shot, nước bơm tính riêng trong phase. Chạm chip để sửa đại lượng, so sánh và ngưỡng ngay trên dòng; thêm/xóa cũng thực hiện tại chỗ.
- Ramp chỉ áp dụng cho điều khiển áp suất/lưu lượng. Công suất số nguyên 0–100% được runtime đặt tức thời; 0% hiển thị Nghỉ. Nghỉ không thay đổi lệnh van.
- Đã bỏ khung Thông tin bài và các ô chọn trục ramp, loại bước, nhiệt độ bước, fallback mất cân. Ghi chú chuyển vào menu tên bài. Các trường ẩn vẫn được giữ trong JSON gốc; mở/sửa số/lưu không tự reset bài cũ. Loại Pro/Standard, nhiệt độ bài/bước, adaptive, trục gram/ml và fallback riêng đã lưu tiếp tục có hiệu lực. Có thể xem toàn bộ qua JSON.
- Bài mới mặc định Pro, nhiệt độ theo máy, ramp theo giây và mốc đo đầu bước. Fallback mặc định theo thời gian phase. Khi sửa thời gian, fallback bằng thời gian cũ sẽ đi theo thời gian mới; fallback riêng được giữ nếu còn hợp lệ, hoặc hạ xuống thời gian phase để đáp ứng validator.
- Trần toàn shot hiện cố định 60 giây trên Main; thanh dừng hiển thị đúng giới hạn này. Hoàn tất bước cuối kết thúc shot. Điều kiện gram trong phase không được biến thành dừng toàn shot.
- Standard có cân hợp lệ và mục tiêu gram: không kết thúc theo thời gian phase khi chưa đạt điều kiện; UI ghi rõ thời gian dự phòng. Mất cân dùng fallback theo runtime hiện có.

## Đồ thị

Một vùng vẽ, chung trục thời gian. Trục đứng theo phase đang chọn; mỗi màu dùng đơn vị riêng (bar/ml/s/%). Không nối đường giữa các chế độ. Giới hạn bổ sung là nhãn, không vẽ thành đường áp suất/flow khác. Mốc adaptive và ramp theo gram/ml chỉ minh họa bằng nét đứt và dấu hỏi; không giả lập cân hay cảm biến. Ramp thời gian dài hơn phase chỉ vẽ phần thực hiện trong phase, không ép ramp kết thúc sớm. Tổng thời gian dài hơn 60 s có vạch trần dừng.

## Source và build

- `hmi/web/profiles.js`: thư viện, API, bản nháp và save guard.
- `hmi/web/profile-editor-model.js`: projection và ngữ nghĩa UI.
- `hmi/web/profile-editor-view.js`: shell, hàng khối, JSON, ghi chú và tóm tắt.
- `hmi/web/profile-inline-editor.js`: bốn dòng thông số, phiên chỉnh tại chỗ và commit/cancel.
- `hmi/web/profile-blocks.css`, `hmi/web/profile-inline.css`: kiểu dáng theo bản đã duyệt.
- `hmi/web/profiles-preview.js`: mô hình đường mục tiêu và SVG xem trước.
- `scripts/package_profiles.py` ghép các module/CSS vào header; `scripts/package_hmi_web.py` đóng gói trang gzip và ETag.

Chỉ cần build HMI cho thay đổi giao diện này:

```powershell
pio run -d hmi -e esp32-s3-devkitc-1
```

Binary: `hmi/.pio/build/esp32-s3-devkitc-1/firmware.bin`. Nạp vào mục ESP32-S3 HMI; sau reboot tải lại Web để nhận ETag mới. Chưa xác minh thao tác trên thiết bị thật trong lượt này.

Web HMI hiện truy cập trực tiếp: PID, System, chẩn đoán và OTA không yêu cầu đăng nhập. Đã gỡ Basic Auth trên API lẫn header/hộp nhập mật khẩu của giao diện cập nhật firmware. Mật khẩu AP Wi-Fi vẫn là cấu hình kết nối mạng; các kiểm tra trạng thái máy và tính hợp lệ của firmware vẫn được thực hiện khi OTA. `tests/heater/web_access_test.cjs` kiểm tra đường upload không phụ thuộc xác thực và các guard OTA còn nguyên.

## Kiểm tra

Node: `profile_preview_test.cjs`, `profile_block_model_test.cjs`, `profile_inline_model_test.cjs`, `profile_lazy_loading_test.cjs`, `profile_navigation_guard_test.cjs` trong `tests/heater/`.

Browser dùng toàn trang gzip thực trong Chrome với API giả lập. `tests/heater/profile_inline_browser_test.cjs` dùng Playwright (module `playwright` hoặc đường dẫn qua `BREWVECTOR_PLAYWRIGHT_MODULE`), chạy từ gốc repo. Kiểm tra Enter/Esc/blur, số thập phân dùng dấu phẩy/chấm, không lưu giá trị lỗi, chuyển phase bằng một click khi đang nhập, điều kiện/giới hạn tại chỗ, lưu HTTP 202 rồi đọc lại, giữ metadata và cấu trúc canonical, giữ bản nháp khi lưu lỗi, giới hạn adaptive -1 không bị reset khi chỉ mở rồi đóng. Sửa mục tiêu trên bài cũ Standard/ramp ml/nhiệt độ riêng không đổi các trường ẩn. Power về 0 cập nhật thành Nghỉ. Bố cục desktop 1536×1152 và mobile 390×844 không tràn ngang trang, không có lỗi JavaScript. Browser API trước đó không khởi tạo được nên dùng Playwright/Chrome.

## Đối chiếu giao diện

Đã dùng view_image xem mockup được duyệt (`exec-252b4e41-41be-49d3-bcb1-8cd653a587a1.png`) và ảnh render thực (`tmp/profile-inline-qa/`). So sánh hàng khối và connector, viền/nền/header theo màu phase, bốn hàng thông số, vị trí input đang sửa, đồ thị và phần tóm tắt. Không còn khung Thông tin bài hoặc các ô kỹ thuật đã yêu cầu bỏ. Tại mobile, ẩn hướng dẫn Enter/Esc để ô đang sửa vẫn cạnh bộ chọn chế độ; chạm ra ngoài xác nhận. Khác biệt có chủ ý: giữ chrome Web thiết bị, nút Lưu vào Main, các nhãn điều kiện chính xác, range flow 0–15 theo thiết bị và giữ thiết lập runtime cũ thay vì xóa chúng. Chưa nạp hoặc kiểm tra máy thật trong lượt triển khai này.
