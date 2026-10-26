# Bộ profile cơ bản cho firmware hiện tại

Đây là các profile mẫu của BrewVector, chưa thử trên máy thật. Mỗi file là một profile JSON canonical mà Web hiện tại có thể nhập riêng.

| File | Trình tự | Thời gian tối đa toàn bài |
| --- | --- | --- |
| 01-classic-9bar.json | 3 bar 5s → lên 9 bar trong 3s rồi giữ | 40s |
| 02-gentle-6bar.json | 3 bar 5s → lên 6 bar trong 3s rồi giữ | 40s |
| 03-bloom-9bar.json | 3 bar 5s → tắt bơm 5s → lên 9 bar trong 4s rồi giữ | 43s |
| 04-soft-finish.json | 3 bar 5s → lên 9 bar 4s → giữ 15s → giảm 9 xuống 6 bar trong 10s, giữ thêm tối đa 6s | 40s |
| 05-flow-2mls.json | 3 bar 5s → mục tiêu flow 2 ml/s, giới hạn áp 9 bar | 40s |

## Nhập và sử dụng

Web → Profiles → Nhập JSON → chọn một file → kiểm tra → Lưu vào Main.
Nhập lần lượt, không nhập cả thư mục/ZIP. Sau khi lưu, chọn bài để pha trên HMI/dashboard.

- Tất cả có điều kiện yield **36g tính từ đầu shot, được kiểm tra ở phase cuối**. Đây là giá trị mẫu, hãy chỉnh theo lượng cà và yield mong muốn. Nếu cần giới hạn yield có hiệu lực từ đầu bài, dùng thẻ Yield nhanh toàn shot trên HMI.
- Khi cân BLE không kết nối, điều kiện yield không áp dụng; bài chạy theo thời gian tối đa ghi trong bảng. Khi có cân, phase cuối dừng khi đạt yield hoặc hết thời gian, điều kiện nào đến trước.
- Thời gian ramp nằm trong thời gian phase, không cộng thêm. Tổng mỗi bài dưới trần 60s của máy.
- Soak chỉ tắt bơm, không khẳng định áp thực lập tức về 0. Áp/flow thực phụ thuộc bánh cà và khả năng máy; target không bảo đảm đạt được.
- Flow là giá trị mô hình flow máy đang dùng, không phải trực tiếp tốc độ tăng gram trên cân. Giới hạn 9 bar có thể khiến flow thấp hơn 2 ml/s.
- Nhiệt độ metadata để 0, không thêm điều khiển Heater SSR. Không dùng adaptive target; ramp theo thời gian từ mục tiêu trước để dễ xem và sửa.
- Import tạo ID mới; nhập lại cùng file tạo thêm bản sao, không cập nhật bản đã lưu.

Đã kiểm tra bằng `hmi/web/profile-validation.js`, giới hạn số phase, tổng thời gian và round-trip JSON. Chưa build/nạp hoặc chạy các bài này trên máy.
