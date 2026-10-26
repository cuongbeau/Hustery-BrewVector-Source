# Brew hiện hành trong source — 2026-09-11

- Màn 320×240: profile/nhiệt ước tính ở header, thời gian/trạng thái và Stop 68×30 phía trên; đồ thị 268×101, Pressure/Flow/Yield ở hàng cuối. Vùng Stop dùng chung cho vẽ và touch; cảnh báo heater chừa nút Stop.
- Pressure cyan và Flow est. vàng dùng trục trái 0–10, bước2 (bar và ml/s). Temp est. đỏ dùng trục phải 75–100°C, bước5. Target áp nét đứt xám. Thời gian tự mở rộng từ30s; không nối qua gap >1s, mẫu không hữu hạn hoặc ngoài thang.
- Nhiệt lấy từ `UiTelemetry.boilerTempC` (brew estimate); flow là pump model theo tên legacy `flowGps`, không phải phép đo cân. Chỉ thêm nhiệt vào ShotChart RAM; CSV/wire/controller không đổi. Từ 2026-09-12 End Shot dùng chung đồ thị ba đường trên RAM đã dừng; History từ TF giữ đồ thị áp cũ. Chi tiết: `docs/hmi_result_history.md`.
- Kiểm tra constexpr vùng chạm/yield/tare và cú pháp các translation unit HMI liên quan PASS. Chưa build/link, nạp hoặc kiểm tra trên panel trong lượt này. Backup: `_archive/hmi_brew_chart_20260911/`.

## Lịch sử: Mốc 8 — màn pha ngang HMI (2026-09-06)

Người dùng duyệt tạm bố cục ngang 320x240, tiếp tục chỉnh sau. Bản tham khảo
chỉ là concept; đường áp minh họa không phải dữ liệu đo thực tế.

## Đã triển khai

- Biểu đồ áp suất bên trái; cột phải lần lượt bar, g/s, thời gian s, gram g.
  Nút Stop đỏ trầm rộng toàn hàng dưới. Vùng vẽ và touch dùng `brew_layout.h`;
  chạm đồ thị/số liệu/khoảng trống không gửi Stop. Screen chỉ đổi theo Main.
- Header có tên profile, phase, nhiệt độ; Manual hiện công suất pump thực
  từ telemetry khi nhận diện được profile Manual qua catalog. Catalog tự
  refresh cả ở màn Brew nếu thiếu/không khớp sau reconnect HMI.
- Đồ thị dùng cùng ShotChart RAM hiện có, chỉ vẽ pressure thực + target nét
  đứt xám. Không thêm EMA, không nắn dữ liệu theo hình minh họa. Vẽ các mẫu
  cùng cột x để giữ biến động; không nối qua gap telemetry lớn hơn1 s.
  Axis áp tối thiểu10 bar, tăng bội5 khi cần; thời gian tối thiểu30 s, tăng
  bội10. Cột flow chỉ là số liệu, không có đường flow trên màn pha mới.
- Target vắng ở Manual, flow/power, Scale prep hoặc target không dương;
  lưu NAN tại trường targetPressureBar trong chart RAM để ngắt đường. Không
  đổi CSV/wire/schema. Demo đặt pressureTargetValid để giữ target mô phỏng.
- Mất cân hoặc gram không hữu hạn: hiện `-- g`, không lấy model yield thay
  số cân. Mất Main/khác schema: pressure/flow/time/temp hiện `--`, báo dùng
  nút máy; gram vẫn cập nhật nếu BLE HMI còn hợp lệ. Đồ thị cũ giữ để tham khảo.
  `SCALE PREP` hiện TARING SCALE; Stop vẫn gửi lệnh hiện có.
- Cân reconnect có thể hiển thị gram; không có nghĩa đã mở lại yield exit.
  Policy yield/time fallback của Main giữ nguyên, không suy ra từ màn hình.

## Khác concept và phần chưa làm

Giữ font/nhãn tiếng Anh của firmware: Pressure/Target/STOP; Việt hóa để sau.
Header chia tên và phase hai dòng để vừa320px; số lớn dùng font nhỏ hơn khi
cần. Không cố hiển thị chuỗi dài tràn sang nhiệt độ. Các kích thước cần đánh
giá trực tiếp trên panel, chưa chứng nhận bằng screenshot/hardware test.

Chưa đổi các màn Result/History/Settings, chưa gỡ TFT, chưa sửa encoder hoặc
calibration wizard. Main2150/schema13 và firmware cân giữ nguyên; chỉ HMI
thay đổi. Heater SSR, ULKA/tune và phát triển cân vẫn tạm để lại.

## Kiểm chứng

`tests/hmi_ready/check.ps1` PASS: C++11 static_assert routing màn chờ,
disabled actions, vùng Stop Flush/Brew, plot/data/Stop không chồng lấn.
`pio run -d hmi` SUCCESS: RAM96836 B (29.6%), flash1547593 B (23.6%).
Firmware1547952 B tại `hmi/.pio/build/esp32-s3-devkitc-1/firmware.bin`;
SHA256 `E4D2A82AE121D2FFF25F6441A912CE74FC7025779A4E1104FB3601233B095C36`.
Chưa nạp HMI/test màn thật. Không có browser runtime của UI LovyanGFX này.

Sau nạp: so sánh hình trên panel; thử profile áp, Manual, Scale prep và mất
cân. Kiểm tra nút Stop và nút zin máy, không nhầm chạm số liệu thành Stop;
đồ thị áp phản ánh dữ liệu nhận được, không coi đường mượt concept là mục tiêu.
