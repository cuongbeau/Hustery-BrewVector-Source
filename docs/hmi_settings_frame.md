# Khung Settings HMI — Mốc 10 (2026-09-06)

**Cập nhật Mốc11:** người dùng đã chốt giữ wizard calibration hiện tại và
encoder chỉnh công suất, bỏ đề xuất chỉnh pump trên touch. Main không TFT
và HMI Settings đã build SUCCESS/chưa nạp.
Các ý định về calibration chi tiết phía dưới là lịch sử Mốc10.

Người dùng chốt bốn nhóm Settings trên màn ngang nhỏ; calibration chi tiết
là bước tiếp theo trước khi tách encoder và tháo TFT. Mốc này triển khai
khung và điều hướng tới chức năng sẵn có, không thay dữ liệu calibration,
Main2150/UART13, firmware cân, controller hoặc Heater SSR.

## Bố cục và điều hướng

Settings320×240 có bốn ô148×89, hai hàng/hai cột; dùng nhãn tiếng Anh theo
font đang có. `ui/settings_layout.h` là geometry chung giữa render/hit-test,
khoảng trống giữa ô không có tác vụ. Bỏ phân trang Settings cũ.

| Ô | Màn con |
|---|---|
| Connections | Wi-Fi, Bluetooth scale, Open Web app |
| Calibration | Wizard pressure hiện có |
| Display & Sound | Độ sáng, tự giảm sáng, âm thanh hiện có |
| System | Device status, Storage & Auto-save, Restart display |

Back từ Wi-Fi/Scale/Web về Connections, từ Diagnostics/Storage về System;
Back từ nhóm về Settings rồi về màn đã mở Settings. Networks/keyboard,
pairing cân và các màn xác nhận giữ quy tắc quay về màn cha tương ứng.
Restart display vẫn là restart HMI với màn xác nhận cũ, không thêm lệnh
restart Main. Không đưa các lựa chọn policy mất cân/Heater vào menu.

System hiển thị connected/offline/incompatible của Main và trạng thái
touch thật. Bỏ số `4 ms` cố định từng được vẽ như latency trong System.
Device status vẫn mở Diagnostics có frame age/counter sẵn có. Storage giữ
Auto-save và Erase history có xác nhận; bỏ hai dòng PLANNED và chỉ dẫn tải
CSV trên Web. Không có lối vào Machine placeholder/Firmware từ menu mới;
source các màn cũ chưa xóa. Cập nhật firmware đi qua địa chỉ Web `/update`.

## Truy cập Web

Open Web app hiển thị URL theo IP STA nếu đang kết nối Wi-Fi. Nếu STA chưa
kết nối và mạng AP máy đang bật, hiển thị SSID AP, mật khẩu mặc định và IP
softAP thật. Khi không có mạng thì báo địa chỉ chưa có, không đưa URL
`0.0.0.0`. Nút Wi-Fi Settings mở cấu hình kết nối hiện có.

`HmiOta::apIp()` là getter read-only: trả IP khi apActive, chuỗi rỗng khi
AP tắt. HMI main loop copy vào buffer local cho UiTelemetry cùng tên AP;
không đổi UART, Web endpoint, credential hoặc policy khởi động/tắt AP.
Trang chỉ hướng dẫn mở trên điện thoại/PC cùng mạng, không tự khởi động
OTA hoặc tải firmware.

## Phần chưa thực hiện

- Calibration vẫn mở wizard áp cũ; chưa làm lại từng bước Start/Stop/chỉnh
  pump% trên cảm ứng, chưa thêm workflow calib flow. Giá trị NVS giữ nguyên.
- Encoder/TFT Main vẫn tồn tại, chưa đủ để tuyên bố có thể tháo TFT ngay.
- ULKA/tune/Heater SSR và phát triển cân vẫn để lại theo quyết định trước.

## Kiểm chứng

- Compiler ESP32-S3 với metadata PlatformIO hiện hành, `-fsyntax-only`:
  PASS cho `main.cpp`, `ui/screen_manager.cpp`, `ui/screens/settings_screens.cpp`,
  `network/ota_service.cpp`. Kiểm tra syntax/interface, không link firmware.
- `tests/hmi_ready/check.ps1`: PASS cho checks sẵn có của Ready/Brew/Result;
  không diễn giải thành test runtime/pixel của Settings mới.
- Rà đường Back và menu: không còn settingsPage_, nút Machine hoặc số4 ms
  cố định trong System. Chưa build/nạp mốc10 hoặc kiểm tra panel thật.
- Binary hiện có vẫn là Mốc9 trong `docs/hmi_result_history.md`, chưa chứa
  Settings mới. Không tạo artifact hoặc hash mới trong mốc source này.

Khi nạp bản chứa Mốc10, cần kiểm tra chữ/vùng chạm4 ô, Back qua nhóm, pairing
cân/Wi-Fi, URL STA/AP và không-network, Auto Save và màn xác nhận restart.
