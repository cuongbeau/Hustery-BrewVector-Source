# Bộ lọc PID và bảo vệ nhiệt

## Trạng thái hiện hành

PID dùng nhiệt từ MAX31855 qua **một bộ lọc thông thấp 0,3 giây**, không có tầng trung vị 3 mẫu. Mỗi mẫu hợp lệ mới tác động ngay vào bộ lọc; PID tính công suất mỗi 1 giây, task bảo vệ/SSR chạy mỗi 10 ms.

Chu kỳ đọc cảm biến là 250 ms. Mẫu báo lỗi chip hoặc không hữu hạn bị loại, giữ nhiệt tốt gần nhất. Lần đọc đầu hoặc sau khoảng không có mẫu tốt quá 1,5 giây khởi tạo lại bộ lọc bằng mẫu mới. Lọc nhiệt không sửa mã lỗi chip hoặc xác nhận nhiệt nước thật.

Bản đang được kiểm chứng trên máy dùng Main Arduino ESP32 2.0.17 và HMI Arduino ESP32 3.1.1. Cách build Main tương ứng nằm trong [INSTALL](INSTALL.md); cấu hình riêng là `platformio.arduino2-test.ini`. Hệ số PID và hiệu chỉnh là cấu hình của từng máy, không được thay bởi việc bỏ trung vị.

## Xác nhận quá nhiệt

Khi một mẫu hợp lệ đạt trần nhiệt cấu hình, Main lập tức đặt công suất/SSR về 0, bỏ Ready và giữ phần I. Sau đó:

- Ba mẫu liên tiếp đạt trần, trải ít nhất 500 ms, khóa lỗi Overtemp. Mỗi mẫu xác nhận cách nhau không quá 500 ms; tick lặp không được đếm lại mẫu cũ. Mẫu lỗi hoặc dưới trần cắt chuỗi xác nhận.
- Trong lúc chờ, PID/Test/FF đều bị chặn. Không tích I hoặc bù thời gian chờ khi phục hồi.
- Tự phục hồi cần ba mẫu liên tiếp trong ít nhất 500 ms, nhiệt thô và nhiệt lọc đều không lớn hơn trần−3°C, mật độ lỗi không quá 8/20 và không còn pause cảm biến. Mẫu lỗi/nhiệt chưa đủ thấp bắt đầu lại khoảng chờ.
- Mẫu đạt trần+10°C hoặc 150°C, lấy ngưỡng thấp hơn, khóa ngay. Khi chưa cấu hình trần, 150°C khóa ngay.
- Configure khi OFF và nhiệt đang đạt trần mới vẫn khóa ngay. Enable/Test từ chối khi đang chờ hoặc nhiệt còn từ trần−3°C. ClearFault chỉ về OFF; OFF/inhibit hủy tự phục hồi.

Thời hạn Test/tự nghỉ vẫn chạy trong khoảng chờ. Bảo vệ này dùng cùng cảm biến điều khiển, không thay bảo vệ nhiệt vật lý độc lập. Các giá trị xác nhận là chính sách firmware thử trên máy dự án, không phải hằng số nhiệt đã hiệu chuẩn cho mọi máy.

## Pause do cảm biến

Cửa sổ 20 mẫu cảnh báo từ 4 lỗi, pause ở 14, phục hồi khi còn tối đa 8, khóa Sensor ở 18. Tuổi mẫu tốt quá 1,5 giây gây pause; quá 5 giây khóa Stale. Heartbeat Main quá 1 giây khóa MainStalled. Số nhiệt hiển thị có thể vẫn là mẫu tốt được giữ trong lúc chip đang báo lỗi.

## Chẩn đoán và tương thích

Frame cảm biến v9 nối tiếp prefix v8; HeaterWire 4, application schema 17 và cấu hình NVS không đổi. HMI mới nhận v8/v9. Main mới với HMI cũ vẫn có đường điều khiển, nhưng phần chẩn đoán cảm biến có thể quá hạn do khác layout.

Trường `medianTemperature` giữ chỗ trong wire/CSV để tương thích, phát NaN (JSON `null`) khi trung vị đã bỏ. Web hiện nhiệt thô → nhiệt lọc PID và nhãn “đã bỏ trung vị”. HMI còn hiển thị đúng trung vị nếu Main cũ gửi giá trị hữu hạn.

Web/CSV có trạng thái chờ quá nhiệt, bộ đếm, thời điểm và đỉnh thô của lần chờ gần nhất. Đây là dữ liệu RAM từ lần bật Main; lỗi đã khóa được lưu riêng theo [chẩn đoán runtime](main_runtime_diagnostics.md). Poll Web có thể bỏ lỡ trạng thái chờ ngắn nhưng bộ đếm/đỉnh gần nhất vẫn còn đến reboot.

## Kiểm chứng và giới hạn

Build/nạp Main và HMI ngày 05/11/2026 thành công; OTA Main 100%, không recovery. API và browser thật xác nhận bộ trung vị đã tắt, cấu hình heater trước/sau khớp và không có lỗi JavaScript. Người dùng báo giữ nhiệt tốt hơn và giảm cảnh báo; chưa xác định nguyên nhân giảm mã lỗi chip.

Phép thử Arduino 3.1.1 → 2.0.17 trước đó, cùng chế độ heater OFF khoảng một phút, ghi 95/238 và 93/240 mẫu lỗi (39,9% và 38,8%). Nhiệt block khác nhau, chưa thấy cải thiện rõ do đổi SDK. SPI software 100 kHz đang được giữ từ bản đối chứng; lượt thử so với 1 MHz trước đó cũng chưa thấy giảm lỗi rõ. Không suy các phép thử ngắn thành kết luận phần cứng hoặc SDK hoàn toàn vô can.

Các test core/model/filter/decoder/error-stats/record và Web/transport/chart/CSV đã qua. `tests/heater/temperature_filter.cpp` kiểm tra đáp ứng ngay mẫu đầu, giữ mẫu lỗi, khởi tạo lại khi quá hạn và các đường xác nhận/phục hồi quá nhiệt.
