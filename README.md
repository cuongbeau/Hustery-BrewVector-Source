# Hustery BrewVector

Bộ điều khiển espresso gồm Main ESP32 và HMI ESP32-S3. Main xử lý cảm biến, bơm, heater và profile; HMI cung cấp màn hình cảm ứng, Web, cân BLE, lịch sử shot và cập nhật firmware.

## Bắt đầu

1. Xác nhận [máy nền](docs/STOCK_MACHINE.md) và đối chiếu [linh kiện, dây logic](docs/wiring_diagram.md) với phần cứng thật.
2. Theo [hướng dẫn cài đặt](docs/INSTALL.md) để build và nạp riêng Main/HMI.
3. Làm theo [hướng dẫn sử dụng và hiệu chỉnh](docs/user_manual.md) trước khi pha.

Các sơ đồ công khai hiện chỉ mô tả tín hiệu thấp áp; chưa có sơ đồ công suất AC
hoàn chỉnh. Bảng linh kiện còn thiếu một số mã mua hàng đã kiểm chứng. Bản
Firmware Main và HMI đã build thành công bằng PlatformIO ngày 2026-10-26, nhưng
chưa được xác nhận sau khi nạp lên máy. Đây là source chia sẻ để cùng hoàn thiện,
chưa phải hướng dẫn lắp máy đã được xác nhận đầu cuối.

## Cấu trúc

- `src/`, `interfaces/`, `protocol/`, `config/`: firmware Main và giao thức.
- `hmi/`: firmware HMI và Web.
- `profiles/`: profile JSON mẫu.
- `lib/`: thư viện và lớp tích hợp; nguồn gốc, bản upstream và tình trạng giấy phép nằm trong hồ sơ của từng thư viện.
- `docs/`: hướng dẫn sử dụng và tài liệu kỹ thuật.

Cấu hình build nằm trong `platformio.ini` và `hmi/platformio.ini`; chi tiết HMI ở `hmi/README.md`.

## Dữ liệu khi nâng cấp

Main dùng `/profiles_brewvector.json` và có thể đọc file profile cũ khi file mới chưa tồn tại. HMI dùng `/brewvector` trên thẻ TF và thử đổi tên thư mục lịch sử cũ trong lần khởi động đầu tiên. AP HMI là `BrewVector-HMI-XXXX`, địa chỉ mDNS là `brewvector-hmi.local`; mật khẩu AP/OTA được cấu hình riêng theo [INSTALL](docs/INSTALL.md).

## Quyền sử dụng và nguồn gốc

Phần dự án tự tạo được dự định chia sẻ cho mục đích phi thương mại theo
[LICENSE](LICENSE); các phần từ dự án khác có quyền riêng trong
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Tên upstream trong hồ sơ nguồn
gốc và đường dẫn dữ liệu cũ phục vụ chuyển dữ liệu là có chủ đích. Hồ sơ giấy
phép ở đây không thay thế việc kiểm tra điều kiện của các dependency PlatformIO
trước khi phân phối firmware.
