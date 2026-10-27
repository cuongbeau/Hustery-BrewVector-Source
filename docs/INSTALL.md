# Cài đặt Hustery BrewVector

Hướng dẫn này áp dụng cho source hiện tại: Main ESP32 DevKit V1 bản 38 chân
(`esp32dev`) và HMI ESP32-S3 ES3C28P (`esp32-s3-devkitc-1`). Bản đổi tên
BrewVector đã build thành công ngày 2026-10-26 nhưng chưa được xác nhận sau khi
nạp lên máy thật; người làm theo cần kiểm tra phiên bản phần cứng
và các mục chưa xác minh trước khi cấp điện cho tải.

## 1. Đọc phần cứng

Đối chiếu [máy nền](STOCK_MACHINE.md), [linh kiện và dây logic](wiring_diagram.md)
với máy thực tế. Sơ đồ logic không bao gồm dây điện lưới; không suy ra đấu tải
từ các chân GPIO. Chuẩn bị phương án ngắt nguồn độc lập và kiểm tra mạch tải
trước khi nối vào máy.

## 2. Công cụ và source

Cài Python 3 và PlatformIO Core, sau đó tải toàn bộ source vào một thư mục.
`platformio.ini` ở gốc dùng cho Main; `hmi/platformio.ini` dùng cho HMI. Các
thư viện khai báo trong hai file này được PlatformIO tải khi build. Phiên bản
PlatformIO và các thư viện Main hiện chưa được ghim hoàn toàn, nên chưa có cam
kết build tái lập byte-for-byte.

Tạo hai file cấu hình riêng trước khi build:

```powershell
Copy-Item config/local_access.example.h config/local_access.h
Copy-Item hmi/include/local_access.example.h hmi/include/local_access.h
```

Chọn mật khẩu AP Recovery Main và AP HMI khác mật khẩu mẫu; mỗi mật khẩu
AP phải có 8–63 ký tự theo yêu cầu Wi-Fi. Đổi cờ `*_CONFIGURED` thành `1` ở cả
hai file. Hai file `local_access.h` được Git bỏ qua; không đưa mật khẩu thật vào
commit, issue hoặc ảnh chụp màn hình. Mật khẩu mới chỉ có hiệu lực khi nạp
firmware được build với cấu hình đó.

Từ thư mục gốc, chạy:

```powershell
pio run -e esp32dev
pio run -d hmi -e esp32-s3-devkitc-1
```

Hai file firmware sau build là `.pio/build/esp32dev/firmware.bin` và
`hmi/.pio/build/esp32-s3-devkitc-1/firmware.bin`. Chúng dành cho **hai bo khác
nhau**; kiểm tra tên môi trường trước khi nạp.

## 3. Nạp USB lần đầu

Xác định đúng cổng nối với từng bo bằng `pio device list`. Thay `<PORT>` bằng
cổng thực tế; không mặc định COM3/COM4 là đúng cho mọi máy.

```powershell
pio run -e esp32dev -t upload --upload-port <PORT_MAIN>
pio run -d hmi -e esp32-s3-devkitc-1 -t upload --upload-port <PORT_HMI>
```

Chỉ nạp khi đã kiểm tra đúng bo và không có phiên pha/xả đang chạy. Sau nạp,
đối chiếu trạng thái Main–HMI và phiên bản giao thức trước khi dùng các lệnh
điều khiển. Hướng dẫn cập nhật HMI qua Web nằm trong [hmi/README.md](../hmi/README.md).

## 4. Kết nối và hiệu chỉnh

HMI phát AP `BrewVector-HMI-XXXX`, dùng mật khẩu trong `hmi/include/local_access.h`; mở
`http://192.168.4.1` để nhập Wi-Fi 2.4 GHz. Khi HMI kết nối mạng, có thể thử
`http://brewvector-hmi.local`; dùng IP hiển thị trên HMI nếu `.local` không được
phân giải. Web HMI, PID, System, chẩn đoán và OTA qua Web không yêu cầu tài khoản
hoặc mật khẩu đăng nhập. Thiết bị truy cập được địa chỉ HMI có thể sử dụng các
trang/API này. Mật khẩu AP chỉ dùng khi kết nối vào Wi-Fi do HMI phát.

Sau đó làm theo [hướng dẫn sử dụng](user_manual.md), đặc biệt là hiệu chỉnh
zero/áp, nhiệt và Q1/Q9. Các giá trị hiệu chỉnh từ máy khác chỉ là ví dụ; không
chép nguyên vào máy mới. Cân BLE cần thiết cho dừng theo gram, không cần cho
profile chỉ dừng theo thời gian/điều kiện khác.

## 5. Trạng thái xác minh

- Các bài kiểm tra Web/profile bằng Node đã qua trong đợt đổi tên.
- Main ESP32 và HMI ESP32-S3 đã build thành công với PlatformIO ngày
  2026-10-26; chưa xác nhận việc nạp hoặc chuyển dữ liệu profile/lịch sử trên
  thiết bị.
- HMI với trình tạo/chỉnh sửa profile tại chỗ và Web không đăng nhập đã build
  thành công ngày 27/10/2026; chưa nạp/xác minh phần cứng trong lượt triển khai này.
- Bảng linh kiện còn thiếu mã mua hàng của một số cảm biến, module công suất,
  nguồn và đầu nối. Xem cột trạng thái trong [sơ đồ đấu dây](wiring_diagram.md).
