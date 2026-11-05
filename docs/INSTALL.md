# Cài đặt Hustery BrewVector

Hướng dẫn này áp dụng cho source hiện tại: Main ESP32 DevKit V1 bản 38 chân
(`esp32dev`) và HMI ESP32-S3 ES3C28P (`esp32-s3-devkitc-1`). Main/HMI đã
build và nạp OTA trên máy dự án ngày 05/11/2026. Người làm theo cần đối chiếu
phiên bản phần cứng và các mục chưa xác minh trước khi cấp điện cho tải.

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
pio run -d . -e esp32dev
pio run -d hmi -e esp32-s3-devkitc-1
```

Hai file firmware sau build là `.pio/build/esp32dev/firmware.bin` và
`hmi/.pio/build/esp32-s3-devkitc-1/firmware.bin`. Chúng dành cho **hai bo khác
nhau**; kiểm tra tên môi trường trước khi nạp.

### Build đúng bản Main đang được thử trên máy dự án

Ngày 05/11/2026, Main đang chạy Arduino ESP32 2.0.17 / IDF 4.4.7, dùng bộ lọc nhiệt 0,3 giây đã bỏ trung vị. Để chọn đúng nền này:

```powershell
pio run -d . -e esp32dev -c platformio.arduino2-test.ini
```

File app nằm tại `tmp/pio-arduino2/build/esp32dev/firmware.bin`. Cấu hình này pin platform `espressif32@7.0.1`, dùng thư mục build riêng và giữ nguyên source. Nếu nạp USB bằng PlatformIO, dùng cùng `-c platformio.arduino2-test.ini` trong lệnh upload. Không lấy nhầm app từ `.pio/build/esp32dev` của cấu hình mặc định.

HMI đã kiểm chứng bằng Arduino ESP32 3.1.1, platform 53.03.11. Hai cấu hình mặc định dùng `platform = espressif32` chưa pin phiên bản; đối chiếu log build vì môi trường cài đặt khác có thể chọn SDK khác. Bản đối chứng Arduino2 không tự chứng minh SDK là nguyên nhân lỗi nhiệt; xem [bộ lọc và kết quả thử](heater_temperature_filter.md).

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

- Các bài kiểm tra core/PID/filter/decoder/diagnostic record và Web/transport/chart đã qua; Main/HMI rollback bộ lọc đã build, nạp và xác minh API/Web ngày 05/11/2026.
- Bản HMI Web với editor theo khối và truy cập không đăng nhập đã build
  ESP32-S3 thành công ngày 27/10/2026. Chưa nạp hoặc xác minh riêng việc chuyển
  dữ liệu profile/lịch sử trên thiết bị trong lượt này.
- Bảng linh kiện còn thiếu mã mua hàng của một số cảm biến, module công suất,
  nguồn và đầu nối. Xem cột trạng thái trong [sơ đồ đấu dây](wiring_diagram.md).
