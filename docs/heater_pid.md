# PID nhiệt thân thermoblock — 2026-09-08

## Hysteresis lỗi cảm biến — 2026-09-17 — hiện tại

Vòng20 mẫu: cảnh báo4, pause SSR14, tự hồi8, latch Sensor18. Pause tuổi mẫu
tốt1,5s, stale5s, raw ceiling và Main watchdog giữ nguyên. Chưa build/nạp.

## PID theo Gaggimate — 2026-09-08 — hiện tại

Hành vi hiện tại thay mục cũ: bỏ phanh15s/I5%; PID1Hz theo phương trình Gaggimate, FF cộng trước antiwindup nhưng mặc định tắt và inlet chưa qualified. Guards/filter local giữ lại; chưa build/nạp. Các mục dưới là lịch sử.

## Tạm dừng nhiệt 1,5s và khóa 5s — 2026-09-08

Source mới: >1,5s tạm ngừng heater/I; >5s mới latchStale, mẫu tốt trước
hard fault tự tiếp tục có guards, không tích phân bù. Mục10/20 này là lịch sử,
được thay bởi hysteresis ở trên. Web/HMI có trạng thái tạm dừng riêng. Kiểm tra PASS; chưa
build/link/nạp bản hai mức. Bản1s trước đã build/nạp/chạy thực nghiệm.
Hysteresis ở mục đầu thay các ghi chú trước đó.

## Chính sách bỏ mẫu lỗi kiểu Gaggimate — 2026-09-08

Source đã chuyển sang đọc250ms, giữ nhiệt tốt cho PID khi lỗi lẻ, ngắt/latch
khi đủ10 lỗi/20 mẫu hoặc tuổi mẫu tốt>1s. Giữ gain/filter/PWM và các bảo vệ
khác. Cần ClearFault thủ công sau khi phục hồi; chỉ trả OFF. Core/target syntax
Main-HMI/Web tests PASS; chưa build/link/nạp/tune bản này. Các mục dưới là lịch
sử triển khai trước thay đổi, binary cũ chưa chứa chính sách mới.

### Hotfix Web đơ sau OTA HMI (2026-09-08 10:38)

V2 đã build Main10:34/HMI10:34; user nạp HMI và root Web không đổi tab.
Nguyên nhân: handleRoot chèn anchor PID vào giữa script ghép từ WEB_HTML_TOP
và WEB_HTML_BOTTOM, gây `SyntaxError: Unexpected token '<'`. Đã chuyển anchor
sang sau `</script>` trong WEB_HTML_BOTTOM. Test ghép đúng toàn bộ root tái hiện
FAIL trước sửa và PASS sau sửa; Web heater/dashboard tests cũng PASS.
HMI hotfix build SUCCESS10:38:53, binary1628624 bytes; Main10:34 giữ nguyên.
Chưa agent OTA/test thiết bị. `tests/heater/assembled_page_test.cjs` là regression.
Nếu đã nạp HMI15 nhưng Main còn14: mở trực tiếp `/firmware`, nạp Main15 qua
trang OTA riêng trước, rồi HMI hotfix. HMI15 đang chạy chỉ chấp nhận self-OTA
khi Main tương thích và heaterOFF; không dùng tab System đang hỏng để cập nhật.

## V2 — Preheat và thời gian nghỉ cấu hình (source mới, chưa build/nạp)

Mục này thay thế hành vi v1 bên dưới khi có xung đột. V1 đã build Main/HMI
SUCCESS ngày2026-09-08; các binary hiện có thuộc v1, không chứa thay đổi v2.

- Target mặc định115°C **nhiệt thân**. Boot power-on bình thường (`ESP_RST_POWERON`)
  thử Enable đúng một lần sau mẫu cảm biến đầu tiên và heartbeat Main. Chỉ cho
  chạy nếu task watchdog sẵn sàng, không đang chạy nước/OTA, không fault và
  cấu hình hợp lệ. Reset software/OTA/watchdog/brownout/deepsleep không tự bật.
  OFF/Configure/Preheat thủ công hoặc OTA hủy yêu cầu boot đang chờ.
- Ceiling mặc định0 vẫn khóa gia nhiệt đến khi người dùng cấu hình giới hạn
  phù hợp. V2 không tự đoán ngưỡng bảo vệ hoặc tăng công suất để tránh E2.
- NVS mới `heater-v2/config`. Nếu chưa có v2, đọc đúng layout v1 và giữ gain,
  công suất, ceiling, timeout; đổi mục tiêu mặc định sang115°C. Chỉ dùng migration
  nếu hợp lệ với115°C, không tăng ceiling cũ. Không hợp lệ thì dùng mặc định
  chưa commissioning. Sau Save ghi v2. Setpoint v2 đã lưu được giữ qua reboot.
- `autoSleep` bật/tắt; `idleMinutes`1..240, mặc định30;
  `sleepDelaySeconds`0..3600, mặc định0; tổng thời gian tới OFF = hai khoảng cộng
  lại. Tắt autoSleep giữ PID liên tục nhưng vẫn áp dụng bảo vệ lỗi/TEST deadline.
- Brew/Flush/HotWater/Calibration hoạt động cập nhật timer; sau kết thúc tính lại.
  Lần đầu Preheat đạt Ready cũng tính lại timer. Sau đó dao động nhiệt ra/vào
  vùng Ready không gia hạn idle. Nếu không đạt Ready vẫn có timeout từ lúc Enable.
- `stableSeconds`0..300, mặc định10. Ready cần **cả raw và filtered** nằm trong
  ±2°C quanh target liên tục khoảng thời gian đó. Đây chỉ là sẵn sàng nhiệt thân,
  chưa xác nhận nhiệt đường nước/puck. Ra khỏi vùng thì bỏ Ready, cần ổn định lại.
- HMI Ready và Web dashboard: OFF/Sleep → PREHEAT, Pid chưa Ready → HEATING
  (khóa nút pha), Ready → BREW. Lệnh StartBrew lúc OFF được Main diễn giải là
  Preheat **không pulse relay/không tự chạy shot**; cần nhấn lại khi Ready.
- Main cũng kiểm tra Ready trước khi bắt đầu PROFILE_RUN từ sự kiện nút zin;
  chưa Ready thì giữ pumpOFF và không tự preheat/wakeup bo zin. Không thêm
  lệnh bấm nút zin để đồng bộ sleep. Flush/HotWater/calibration vẫn có thể dùng
  riêng như trước; gate này chỉ dành cho bắt đầu Brew.
- API Configure bổ sung bắt buộc autoSleep(bool), sleepDelaySeconds(integer),
  stableSeconds(integer). GET có ba giá trị này và ready(bool). Chỉnh qua Web
  khi heaterOFF, máy Ready. TFT chỉ có nút Preheat/Brew và cảnh báo, chưa editor.
- HeaterWire version2, application schema15: cả Main/HMI phải cập nhật.
  **Nạp HMI mới trước rồi Main mới**. Main updater UART hỗ trợ gửi firmware
  khi application schema chưa khớp; UI điều khiển bị khóa trong khoảng chuyển tiếp.
  HMI v1 có guard compatible nên nạp Main15 trước có thể chặn OTA HMI v1.

E2 sau khoảng15s khi chuyển heater sang SSR, hết khi phục hồi heater zin:
củng cố giả thuyết bo giám sát đáp ứng nhiệt, chưa xác nhận điều kiện chính xác.
Máy zin tự sleep/tắt màn sau thời gian chưa đo. Các timer v2 chỉ là bộ đếm
ESP32, **chưa chứng minh đồng bộ máy zin hoặc khắc phục E2**. User tự đo sau;
nhận biết wakeup bo zin được hoãn.

Validation v2: core constexpr timer disable/delay/readiness và guard cũ PASS;
syntax target Main/HMI gồm orchestrator/command/live screen PASS; script Web
thật với DOM/network giả và dashboard Preheat/Ready/Stop/offline PASS.
Chưa build/link v2, chưa nạp, chưa kiểm tra phần cứng hoặc visual browser QA.

## V1 — hồ sơ triển khai ban đầu (lịch sử)

Trạng thái: đã triển khai source Main/HMI, kiểm tra lõi + cú pháp target + script Web PASS.
Chưa full build/link, chưa nạp OTA, chưa xác nhận SSR hay tune trên máy thật.

## Phạm vi

Type K trên thân nhôm qua MAX31855; SSR-40DA nhận GPIO13 Main, active HIGH.
Giữ nhiệt **thân**, không cam kết nhiệt nước/puck hoặc sai số nhiệt cụ thể.
Không dùng `ThermalModel` làm feedback, chưa offset họng/bù lưu lượng/autotune.
PID hiện là lõi độc lập viết cho dự án, không phải port nguyên Heater GaggiMate.

## Kiến trúc và hành vi

- `src/control/heater_core.h`: phép tính và state machine thuần C++17, kiểm tra
  bằng constexpr trên chính code. P trên sai lệch, I conditional anti-windup
  giới hạn 0..maxPower; D trên nhiệt đo, lọc 0,7 s. Kp %/°C,
  Ki %/(°C·s), Kd %·s/°C. Khi OFF/lỗi reset P/I/D và power.
- `src/hardware/sensor_reader.cpp` vẫn sở hữu MAX31855, đọc 200 ms và gửi raw
  vào Heater. Lọc PID tau0,3 s riêng, không thay EMA 0,90 cho hiển thị/model cũ.
  Không tính lại PID trên mẫu cũ. NaN/không hữu hạn/ngoài -20..500°C khóa lỗi.
- `src/hardware/heater_control.*`: task10 ms priority2, stack3072, khóa core/GPIO
  bằng portMUX. Task và các đường ngắt tức thời cùng thuộc namespace owner Heater.
  SSR cửa sổ1000 ms, phân giải nhịp task; relay telemetry là **lệnh**, không đo AC.
- Boot đặt LOW trước Serial/init khác; nạp cấu hình NVS `heater-v1/config`,
  kiểm tra toàn bộ miền giá trị. Không persist chế độ ON. Task watchdog IDF5 s
  có panic/reset, chỉ cho bật nếu đăng ký task watchdog thành công.
- Main loop gửi heartbeat và trạng thái Brew/Flush/HotWater. Khi đang bật,
  mẫu nhiệt quá1 s hoặc heartbeat Main quá1 s → fault/OFF từ task riêng.
  Đây không bảo vệ được mọi lỗi CPU/GPIO hoặc SSR chập.
- OFF / PID / TEST / FAULT. Lỗi latched trong RAM; mẫu trở lại không tự bật.
  Clear cần mẫu tươi, Main tươi, cấu hình hợp lệ, nhiệt raw thấp hơn ceiling3°C;
  Clear chỉ về OFF. Reboot cũng OFF.
- Cấu hình/chạy/clear chỉ nhận khi Main IDLE, ngoài OTA. OFF luôn ưu tiên.
  Cấu hình không thay khi đang PID/TEST. Lưu NVS ở main-loop ngoài critical/task.
- Idle timeout1..240 phút, mặc định30; không có disable trong bản đầu.
  Brew/Flush/HotWater đang chạy liên tục reset timer; hết chạy bắt đầu đếm lại.
  Enable/test/config hợp lệ cũng cập nhật activity. Web polling/heartbeat UART
  không gia hạn. Hết giờ OFF với `idleExpired=1`; không tự bật khi bắt đầu pha.
- TEST1..60 s, công suất<=maxPower, tự OFF khi hết hạn hoặc bắt đầu chạy nước.
- Configure: target30..130°C, ceiling>=target+5 và <=150°C; maxPower>0..100%;
  Kp0..100, Ki0..10, Kd0..100. Đây là giới hạn phần mềm, **không phải xác nhận
  nhiệt an toàn của phần cứng máy**. Ceiling mặc định0 khiến Enable/Test bị từ chối;
  cần chủ động khai báo. Khi chưa cấu hình vẫn báo quá nhiệt raw>=150°C.
- Giá trị khởi đầu target100/Kp2/Ki0,05/Kd0/maxPower30 chỉ là seed chưa tune.

## UART / Web / HMI

`protocol/heater_wire.h`: version1 packed request/status; extension0x38/0x39,
capability `SupportsHeater` bit3, giữ schema14. Status mỗi500 ms. HMI yêu cầu
capability + status mới dưới2 s để bật/chỉnh; OFF vẫn gửi được nếu status cũ
nhưng heartbeat Main vẫn tương thích. Không tự resend khi mất ACK.

- `/heater`: trang authenticated theo cơ chế Web hiện có, có link từ dashboard.
- GET `/api/heater`: online/ticket/result/mode/fault/uptime/sampleAge/
  idleRemaining/raw/filtered/power/p/i/d/relay/valid/idleExpired và cấu hình.
- POST JSON `/api/heater`: op0=OFF,1=Configure,2=Enable,3=Test,4=ClearFault.
  Configure cần target,ceiling,kp,ki,kd,maxPower,idleMinutes; Test cần
  testPower,testSeconds. HTTP202 chỉ gửi UART; đối chiếu ticket trong GET.
  result0=OK,1=Invalid,2=Busy,3=Unsafe (cũng có thể lỗi lưu NVS).
- Đồ thị nhiệt lọc/mục tiêu/công suất; CSV tối đa7200 status mẫu trong RAM trình
  duyệt, chỉ khi trang mở; mất mạng có khoảng trống, đóng trang mất dữ liệu.
  Không có log heater tự lưu TF trong bản đầu; log chưa có flow/áp.
- HMI TFT cảnh báo đỏ lỗi heater hoặc mất freshness sau khi đã thấy extension.
  Điều chỉnh/tuning qua Web; chưa có editor PID trực tiếp trên TFT.
  Main hỏng có thể không báo lỗi; HMI chỉ báo trạng thái heater không xác định.

## OTA và lần chạy đầu

Main UART OTA và Recovery đều `Heater::inhibit()` trước Update.begin.
OTA lỗi/kết thúc không tự bật lại. HMI OTA với Main có heater yêu cầu status
tươi, không command pending, modeOFF hoặcFAULT, relayOFF; Main updater không busy.
Chặn lệnh bật/chỉnh heater khi HMI Update đang chạy hoặc Main updater busy.
Main cũ tương thích không có capability vẫn cập nhật HMI được.

1. Build cả Main/HMI khi người dùng yêu cầu; không dùng firmware.bin cũ để test
   tính năng này. Ưu tiên nạp Main trước rồi HMI; boot heaterOFF.
2. Kiểm tra nhiệt Type K và lỗi/ON/OFF/idle bằng phần thấp áp, chưa cấp tải heater.
3. Trước chạy tải xác định công suất heater, giới hạn nhiệt phù hợp và cầu chì
   nhiệt độc lập thực tế (hiện chưa xác nhận). Nhập cấu hình có chủ đích.
4. Khi có người giám sát, chạy TEST công suất/thời gian giới hạn phù hợp máy,
   giữ trang mở để xuất CSV; quan sát cả giai đoạn sau ngắt.
5. Chỉnh PI khi OFF và Ready, bật PID, xuất log; chưa hứa một lượt đạt độ ổn định.

SSR chập hoặc GPIO kẹtHIGH không thể xử lý bằng cảnh báo/tắt phần mềm. User chọn
phạm vi bảo vệ trên Main/HMI, chưa bổ sung relay/watchdog ngoài. Nhãn cầu chì
hiện chưa đủ thông tin phân biệt cầu chì quá dòng và cầu chì nhiệt.

## Kiểm tra

- `tests/heater/check.ps1`: actual-core compile-time scenarios startup, stale,
  Main stalled, sensor latch, overtemp raw, test deadline, idle/water, saturation,
  wrap, invalid config, OFF và clear; syntax các TU thay đổi bằng thư viện target.
- `node tests/heater/web_test.cjs`: script Web thật trong DOM/network giả;
  config, ACK, từ chối, timeout không retry, OFF, cảnh báo và CSV.
- Chưa full link, chưa browser visual QA, chưa stress cross-core hay kiểm chứng
  điện/cảm biến/SSR/warmup/idle/brew trên phần cứng. Không coi tests là chứng nhận an toàn.
# Cập nhật chẩn đoán cảm biến 2026-09-08

User xác nhận mẫu lỗi mới xen kẽ sau ClearFault. Source thêm chẩn đoán cùng
khung SPI và sửa nháy cảnh báo HMI; chưa build/nạp. Xem
[giao thức Main–HMI](../protocol/protocol.md) cho API, wire và giới hạn.
