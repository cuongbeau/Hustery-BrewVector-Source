# Máy nền trước khi lắp BrewVector

Tài liệu này ghi lại **máy đã được quan sát** làm nền cho dự án. Tên bán hàng
Winci EM56, HiBREW H21 và Mielux EM3801 xuất hiện trong hồ sơ nghiên cứu;
chưa xác nhận mọi lô và phiên bản bo của các tên này có cùng cấu tạo. Trước khi
lắp, đối chiếu máy thực tế với ảnh, sơ đồ và chân đo của chính phiên bản đó.

## Cấu trúc đã quan sát trên máy nền

| Cụm | Ghi nhận |
|---|---|
| Gia nhiệt | Một thermoblock pha/nước nóng và hai thermoblock hơi mắc nối tiếp; mỗi khối có NTC riêng. |
| Bơm | Hai bơm ULKA: bơm pha/nước nóng và bơm hơi riêng. Mã bơm cụ thể chưa ghi nhận. |
| Đường pha | Bơm pha → hai OPV có thể chỉnh → thermoblock chính → van ba chiều → họng pha 58 mm. Đường hồi OPV về bình nước. |
| Đường nước nóng | Dùng bơm pha và thermoblock chính, qua van điện từ riêng. |
| Đường hơi | Bơm riêng → hai thermoblock hơi → van hơi → vòi hơi. |
| Cảm biến máy nền | Flowmeter đầu vào, phao báo cạn nước, NTC của các thermoblock. |

Đây là sơ đồ chức năng của máy đã quan sát, **không phải** sơ đồ dây điện lưới
hoặc bản xác nhận mọi phiên bản máy thương mại. Các đường ống, van và đầu nối
có thể khác giữa các lô.

## Ranh giới của BrewVector

- Main ESP32 xử lý cảm biến, profile, bơm, heater và các lệnh điều khiển mà mã
  dự án đã tích hợp. HMI ESP32-S3 hiển thị, nhận thao tác, kết nối cân BLE và
  lưu lịch sử; HMI không điều khiển tải trực tiếp.
- Tài liệu đấu nối logic hiện hành: [wiring_diagram.md](wiring_diagram.md).
  Không suy ra dây tải AC từ bảng GPIO.
- Mọi chức năng còn chạy qua bo điều khiển nguyên bản phải được kiểm tra trên
  đúng máy trước khi đấu. Hiện chưa có bản sơ đồ bo zin đầy đủ để công bố.

## Số đo và giới hạn

Trên một máy nền, nhiệt thân thermoblock pha khi đã ổn định được ghi khoảng
113 °C ở mức đặt 88 °C, 116 °C ở 90 °C, 120 °C ở 94 °C và 122 °C ở 96 °C.
Đó là **nhiệt thân tại trạng thái chờ**, không phải nhiệt nước tại họng pha,
không phải bảng hiệu chỉnh dùng chung cho mọi máy. Nhiệt còn thay đổi trong
giai đoạn làm nóng và khi có dòng nước.

## Cần xác minh trên từng máy trước khi lặp lại

- Mã/revision bo, mã hai bơm, công suất heater, điện áp từng van và sơ đồ dây
  tải. Chưa có dữ liệu này đủ chắc để lập bảng mua linh kiện thay thế.
- Loại đầu nối/ống, chuẩn tai portafilter, áp mở hai OPV và vị trí cảm biến áp.
- Cách bo zin xử lý hết nước, steam, lỗi nhiệt và trạng thái ngủ.

Hồ sơ đo và suy luận dài hơn nằm trong context nội bộ của dự án. Chỉ các dữ kiện
có mức chắc chắn được nêu rõ mới được dùng làm hướng dẫn công khai.
