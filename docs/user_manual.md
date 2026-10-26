# Hướng dẫn sử dụng Hustery BrewVector

Đối chiếu source ngày 2026-09-23. Tính năng trên máy phụ thuộc firmware thực sự đã nạp; tài liệu này không xác nhận máy đang chạy bản mới nhất.

## Hiệu chỉnh nhiệt pha — source 17/9

Touch: Settings→Calibration→Brew temperature; Web: Calib→Brew temperature offset. Nhiệt pha ước tính bằng nhiệt thân trừ offset; mặc định21°C, chỉnh0–60°C và Save khi máy nghỉ. Tăng offset làm nhiệt hiển thị thấp hơn. Reload khi có conflict/timeout. Giá trị lưu trên Main qua khởi động lại; PID/FF không đổi. Chưa kiểm chứng trên máy. [Chi tiết](thermal_offset_calibration.md).

## Màn hình và thao tác pha

Brew trong source mới có đồ thị rộng: áp suất xanh cyan và flow ước tính vàng dùng trục trái 0–10; nhiệt ước tính đỏ dùng trục phải 75–100°C. Pressure/Flow/Yield ở cuối màn, Stop nhỏ phía trên bên phải. Giá trị ngoài thang không được vẽ; đọc số hiện tại ở header/footer. Chi tiết và trạng thái kiểm chứng: [Brew](hmi_brew_frame.md).

HMI cảm ứng là giao diện chính. TFT Main cũ đã bị loại khỏi build; không dùng hướng dẫn menu TFT của phiên bản cũ. Main vẫn điều khiển máy khi HMI mất kết nối, nên màn hình mất dữ liệu không có nghĩa bơm hoặc heater đã tắt.

Màn chờ HMI đặt Brew Est. lớn bên trái, nhiệt Block nhỏ phía dưới và đồ thị Brew Est. 60 giây bên phải. Đường target nét đứt là mục tiêu heater theo nhiệt Block trừ offset hiệu chỉnh, không phải số target Block nguyên gốc. Khi thiếu dữ liệu Main/heater mới, đường tương ứng được ẩn. Hàng dưới gồm Preheat/Start Brew, Flush và Menu; chạm Menu để mở History hoặc Settings. Chạm ngoài menu để đóng; thao tác này không kích hoạt các nút bên dưới.

1. Ở màn chờ, chọn profile và kiểm tra trạng thái Main/cân.
2. Nếu heater OFF, nút Preheat chỉ yêu cầu gia nhiệt. Khi Ready mới yêu cầu Brew; không coi lần nhấn Preheat là lệnh pha tự động sau đó.
3. Khi pha, Stop trên HMI/Web gửi yêu cầu Main dừng. Kiểm tra phản hồi và trạng thái máy; khi HMI mất kết nối dùng công tắc vật lý của máy để dừng pha.
4. Flush mặc định5 giây, có thể Stop sớm. Flush không phải shot History.
5. Result hiển thị đồ thị áp suất, flow ước tính và nhiệt ước tính như Brew, giữ đường dữ liệu khi kết thúc shot. Thời gian, gram tại stop và peak áp ở dưới đồ thị; gram nhỏ giọt sau đó được hiển thị riêng. Thiếu dữ liệu cân/stop có thể hiện -- hoặc PARTIAL. History lưu trên TF vẫn dùng đồ thị áp.

Ready dựa trên nhiệt thân raw và filtered đạt ít nhất min(100°C,target) trong thời gian Chờ mở Brew. Đây là quyền pha, không xác nhận nhiệt nước ra họng.

## Profile và Quick Yield

- Web Profiles dùng thư viện và form chỉnh phase, hỗ trợ import/export. Save lưu profile; activate/chọn bài là thao tác riêng. Không sửa/xóa khi máy đang pha.
- Manual/OFF nằm trong danh sách profile. Manual dùng encoder để chỉnh công suất.
- Chạm Yield trên HMI để mở editor, xoay encoder1 g mỗi nấc rồi Apply hoặc Hủy. Apply chỉ đặt gram cho shot kế tiếp và không ghi đè profile lưu.
- Quick Yield dừng theo tổng gram toàn shot. Mất cân hoặc mất continuity có thể vô hiệu dừng theo gram cho phần còn lại; không dùng model thay cân.
- Profile có timeout; giới hạn executor toàn shot60 giây. Flow là ước tính từ mô hình, không phải flowmeter. Phase điều khiển solenoid độc lập valve=0 chưa được hỗ trợ.

## Settings, cân và mạng

Settings có Connections, Calibration, Display & Sound, System. Trong Connections mở Wi-Fi, Bluetooth scale hoặc Open Web app.

- Cân: scan, chọn đúng thiết bị và kết nối; Forget xóa ghép nối đã lưu. Có driver DIY Hustery, Acaia, Timemore, nhưng không mặc định mọi model đều đã được thử thật.
- Khi bắt đầu shot, SCALE PREP là thời gian chuẩn bị tare với bơm OFF. Cân Hustery v2 có xác nhận tare; mất link hoặc mẫu quá hạn không được coi là gram hợp lệ.
- Web: dùng địa chỉ hiện trong Open Web app. Máy tính/điện thoại cần cùng mạng với HMI hoặc kết nối AP của HMI. Dùng IP nếu tên .local không phân giải được.
- Display & Sound chỉnh hiển thị/âm thanh; System có trạng thái lưu trữ và Auto Save. Firmware Center mở trên Web.

## Calibration

Pressure Calibration nằm trong Settings → Calibration. Zero phải thực hiện ở áp thực0 bar. Với phép thử áp có tải, cần lắp dụng cụ tham chiếu và bố trí đường xả phù hợp trước khi chạy Test Pump; theo các bước trên wizard, dùng encoder Main chỉnh công suất và nhập áp đồng hồ cơ để Capture & Save.

Stop trên HMI hoặc giữ encoder trong CALIBRATION_RUN sẽ yêu cầu dừng test; đây là ngoại lệ riêng của calibration. Không áp thao tác giữ encoder để dừng Brew/hot-water. Chờ kết quả lưu thành công trước khi rời wizard.

Pump Flow Calibration dùng Q1 và Q9: lưu lượng nước full-power đo khi hệ ổn định lần lượt tại1 bar và9 bar. Dùng van tiết lưu/đường xả đo được, chờ áp ổn định, hứng nước trong khoảng thời gian biết trước rồi nhập `ml/s`; không dùng espresso trên cân làm phép đo trực tiếp vì puck giữ nước và chất hòa tan làm gram cốc khác lưu lượng nước bơm. Main nội suy affine giữa Q1/Q9 và lưu trong `/flow_cal.json`. Puck Flow trên History là ước tính `virtualScale()` riêng, có thể bắt đầu muộn với puck thoáng; không chỉnh Q1/Q9 để bù lỗi first-drop đó.

## Heater trên Web

Mở trang /heater. Nhiệt hiển thị cho PID là nhiệt thân Type K. Nhiệt nước ước tính không phải phép đo nước và không dùng thay cảm biến heater.

- Power-on bình thường có thể tự Preheat nếu qua guards và cấu hình hợp lệ. Reset/OTA không đồng nghĩa power-on thông thường.
- Lưu cấu hình phải chờ Main xác nhận. Trần giữ nhiệt và trần khi chạy nước là hai giá trị riêng; trần nước chỉ có hiệu lực khi đủ điều kiện bù nước. Đặt trần nước thấp hơn trần giữ nhiệt không hạ trần giữ nhiệt.
- Bù nước là thử nghiệm, mặc định tắt; dữ liệu flow là ước tính. Không tự nâng trần/PID dựa trên thông số mặc định.
- Tự nghỉ dùng thời gian chờ cộng trễ tắt thêm. Bỏ chọn tự nghỉ sẽ giữ nhiệt theo cấu hình; chưa đồng bộ sleep/wakeup với bo zin.
- TẠM DỪNG cảm biến: lệnh SSR OFF và có thể tự tiếp tục khi có mẫu tốt, nếu chưa fault. Lỗi khóa cần xử lý điều kiện và Xóa lỗi; Xóa lỗi không tự bật heater và không xóa lịch sử đếm lỗi.
- Thống kê5 giây là chẩn đoán, không phải vòng điều khiển20 mẫu. Vòng điều khiển cảnh báo4 lỗi, pause14, hồi8 và khóa18; mẫu lỗi lẻ có thể bị bỏ qua trong khi PID dùng mẫu tốt gần nhất.
- CSV phiên /heater chỉ ghi khi trang mở, tối đa7.200 mẫu; tải trước khi đóng. Đây là log khác Control Trace trên TF.

Khi báo mất dữ liệu, không coi đó là xác nhận heater đã OFF. Xem [tài liệu PID nhiệt](heater_pid.md) và trạng thái thực tế của máy trước khi thao tác tiếp.

## History và firmware

History lưu trên TF khi điều kiện ghi/Auto Save cho phép; Result RAM vẫn có thể hiện dù chưa có file. Chờ kết quả backend khi lưu/xóa. Download CSV ở History để phân tích dữ liệu shot.

Firmware HMI: chọn binary của target ESP32-S3 tại /update. Firmware Main: dùng Firmware Center /firmware, upload image Main rồi theo dõi bước truyền/cài đặt. Không tráo hai binary; không update/reboot khi máy đang chạy nước hoặc heater chưa qua interlock. Lệnh build và output: [INSTALL](INSTALL.md).

Không áp quy tắc cũ “luôn nạp HMI trước Main”: thứ tự phụ thuộc cặp schema/bridge thực tế. Với thiết bị kẹt tương thích heater, xem hồ sơ phục hồi và xác định đúng phiên bản trước thao tác. Main Recovery là giao diện dự phòng status/OTA/reboot, không có profile editor/History.

## Encoder Main hiện hành

| Ngữ cảnh | Thao tác |
|---|---|
| IDLE | Xoay chọn profile; double-click hoặc giữ ít nhất5 giây rồi thả để bật/tắt Recovery |
| Quick Yield editor | Xoay chỉnh gram; Apply/Hủy trên HMI |
| Manual/hot-water | Xoay theo điều khiển công suất hiện có |
| Calibration test | Xoay chỉnh công suất; giữ để dừng |
| Shot End | Click/giữ để trở về chờ |
| Brew/hot-water | Giữ encoder không phải lệnh Stop |

Không dùng các thử nghiệm ULKA cũ như quy trình vận hành thường ngày.
