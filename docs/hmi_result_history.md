# End Shot hiện hành — 2026-09-12

End Shot dùng chung `drawBrewChart` với Brew trên dữ liệu ShotChart RAM đã dừng: pressure cyan, flow ước tính vàng, nhiệt ước tính đỏ và target áp nét đứt. Giữ thang trục, khoảng ngắt dữ liệu và kích thước đồ thị 268×101; dịch lên 28px để chừa hàng TIME / YIELD AT STOP / PEAK và nút History/Home. Gram cân hiện tại (nhỏ giọt) vẫn tách riêng ở header. History từ TF giữ đồ thị áp cũ; không bổ sung nhiệt vào CSV trong lần này. Chưa build/link/nạp hoặc kiểm tra panel. Backup source: `_archive/hmi_endshot_chart_20260912/`.

Kiểm chứng: `tests/hmi_ready/check.ps1` PASS (vùng chạm và snapshot lúc dừng); target compiler `-fsyntax-only` PASS cho renderer đồ thị, màn Result/History, màn live và screen manager. Đây không phải kiểm tra hình ảnh trên panel.

# Lịch sử: HMI Result và History — Mốc 9 (2026-09-06)

Người dùng chốt làm Result/History rồi build chung các khung HMI đã duyệt.
Mốc này hoàn thiện hai màn ngang 320×240 bằng LovyanGFX hiện có. Main
2026.09.05-2150, UART schema13, firmware cân và đường điều khiển giữ nguyên.
TFT/encoder/calibration/Settings còn là các phần cần thảo luận trước cutover.

## Hành vi

- Result: tên profile, đồ thị áp thực/target bên trái; thời gian, gram tại
  lúc dừng và peak áp bên phải; History/Home phía dưới. Header có gram cân
  hiện tại riêng để thấy nước nhỏ giọt sau dừng. Home gửi DismissResult cho
  Main; chỉ telemetry Main xác nhận mới đổi màn vận hành.
- History: bốn shot mỗi trang, mới nhất trước, tối đa 40 mục gần đây; mỗi
  mục có ID/profile, ngày giờ nếu biết, thời gian và gram tại dừng. Chạm mở
  summary + đồ thị; Back to History quay lại. Thiếu CSV vẫn xem summary,
  hiện NO CHART nếu không có mẫu. Không có TF thì báo CARD NOT READY.
- Bỏ lối vào Save/Compare trên Result/History. Tự lưu theo setting Auto Save
  hiện có, so sánh/CSV trên Web. Các renderer compare cũ chưa xóa source;
  không còn nút gọi từ hai màn mới. Không làm lại Web trong mốc này.
- Target chỉ vẽ khi trace ở Pressure mode có target dương; đồ thị dùng
  renderer chung với màn pha, không thêm lọc và ngắt qua gap lớn hơn 1 s.
  Result live dùng chart telemetry RAM; History đọc trace CSV25 Hz nên chi
  tiết nét/peak có thể khác vì tần số lấy mẫu, không tuyên bố pixel giống nhau.
- Nhãn tiếng Anh theo bộ font hiện có. Chưa kiểm chứng kích thước chữ/chạm
  trên panel thật; đây vẫn là khung có thể chỉnh sau.

## Số liệu tại dừng

`ShotResultSnapshot` giữ summary RAM, độc lập TF và Auto Save. Trace mở
session, tích lũy duration/peak; `ShotEnd` đúng session đóng băng duration
và gram. Event lặp hoặc trace tới sau không ghi đè kết quả đã đóng. Session
mới hoặc Main chuyển vào Brew/HotWater xóa kết quả trước, tránh hiển thị
shot cũ nếu shot mới thiếu trace. Thiếu trace hoàn toàn thì không có summary.

Gram tại dừng lấy `ShotEnd.scaleWeightG` khi `ShotEndScaleOnline` và finite;
0 g là số hợp lệ, offline/NAN/INF hiện `--`. Không lấy `outputWeightG` hoặc
`modelYieldG` thay thế. Đây là mẫu cân Main có tại sự kiện phần mềm dừng,
không phải đo chính xác thời điểm điện bơm ngắt, và không chứng nhận tare/
baseline liên tục suốt shot. Số cân trực tiếp sau đó chỉ hiện ở header.

`ControlTraceStore` dùng cùng policy khi ghi summary. Không đóng file chỉ
vì nhận telemetry Done trước ShotEnd. Nếu mất event, timeout không nhận trace
2 s hiện có vẫn đóng file, summary PARTIAL/yield không biết. Result RAM
không cần chờ SD; ngược lại, không có TF/tắt Auto Save thì không có bản lưu
bền để mở lại sau reboot. Mốc này không bổ sung retransmission ShotEnd.

## Storage và ngày giờ

Index `/giggauino/traces/index.csv` giữ sáu cột đầu, nối hai cột tùy chọn:

```text
id,profile,duration_s,weight_g,peak_bar,samples,result_flags,started_at_epoch
```

`result_flags`: bit0 có ShotEnd; bit1 số cân hợp lệ tại dừng. Thiếu event
flags0, event thiếu cân flags1, event có cân flags3. `weight_g` không biết
ghi NAN. Loader chấp nhận dòng cũ sáu cột (flags0, epoch0), không coi số
weight legacy là gram vật lý tại dừng. Không xóa hoặc viết lại CSV cũ.
Index đã tồn tại giữ header cũ; có thể có cả dòng6 và8 cột, loader đọc theo
vị trí và chấp nhận phần đuôi tùy chọn. Web `/traces/list` liệt kê trực tiếp
file trace, không phụ thuộc index này; các cột trace và footer không đổi.

HMI khởi tạo SNTP nền một lần khi Wi-Fi STA kết nối, qua Arduino configTime,
UTC+7, không đợi đồng bộ trong loop. Không thêm thư viện hoặc task ứng dụng.
Thời gian hệ thống hợp lệ ở lúc bắt đầu lưu trace thì ghi epoch bắt đầu
(trừ elapsedS); chưa biết thì epoch0/Time unknown, không tự bịa ngày giờ.
SNTP không đổi thời gian millis/đồng hồ shot của Main.

## Kiểm chứng và artifact

- `tests/hmi_ready/check.ps1`: PASS cho geometry/hit-test, yield policy,
  snapshot thực: đóng băng, event lặp/sai session, offline không dùng model,
  shot mới không giữ kết quả cũ. Tests compile-time C++11/C++17, HMI firmware
  build với C++11 hiện hành.
- `pio run -d hmi`: SUCCESS. RAM 97,604/327,680 B (29.8%), flash
  1,554,325/6,553,600 B (23.7%). Binary 1,554,688 B.
- Artifact: `hmi/.pio/build/esp32-s3-devkitc-1/firmware.bin`.
- SHA256: `FF0D7B1C95636006458106907F1BBECE82A0F85E71849BBA96F3068A5946365A`.
- Chưa upload, chưa kiểm chứng phần cứng/SD/SNTP hoặc render pixel tự động.
  Không build Main/cân ở mốc này; không coi đây là thay đổi controller/ULKA.

Sau nạp kiểm tra: stop có cân → gram tại dừng đứng yên khi còn nhỏ giọt;
Home/History/Back và phân trang đúng; shot không cân hiện `--`; không TF
vẫn xem Result; reboot mở lại History; Wi-Fi không Internet hiện giờ chưa
biết. Đây là checklist còn lại, chưa phải kết quả test.
