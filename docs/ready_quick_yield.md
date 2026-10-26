# Màn chờ và Yield nhanh — Mốc12, source 2026-09-06

Người dùng duyệt bố cục Ready/Profile/Yield 320x240, chốt triển khai.
Nhiệt độ pha đỏ cam, Block nhỏ bên phải; Profile và Yield cùng hàng; Brew/
Flush hai nút148×50; History/Settings dưới. Manual/OFF là profile, không nút
Manual riêng. Font firmware LovyanGFX khác font mockup: tên dài co về Font2
và cắt theo chiều rộng; chưa render/test panel thật.

## Thao tác

- Xoay encoder ở IDLE chọn profile lại như người dùng yêu cầu; HMI đọc index
  đang chọn do Main gửi. IDLE lấy tên từ profile đã chọn thay vì shot.profileName,
  sửa hiện tên PRESSURE CAL còn sót sau calibration.
- Có cân online và Main Ready/compatible: chạm thẻ Yield mở màn số lớn,
  xoay encoder1 g mỗi step, không dùng acceleration khi chỉnh yield. Profile
  selection bị chiếm bởi phiên chỉnh. Apply/Hủy trên touch, không dùng click
  encoder trong màn này; trở lại Ready khi Main xác nhận kết thúc phiên.
- Giá trị mặc định lấy target volumetric GTE của pha cuối. Manual hoặc pha
  cuối chưa đặt target thì OFF; chỉnh lên để thêm target cho shot tiếp theo.
  Miền0..500 g, 0 là bỏ điều kiện yield cuối cho shot đó. Target có phần lẻ
  vẫn hiện1 chữ số thập phân; mỗi nấc tăng/giảm1 g.
- Profile OFF, hoặc pha cuối có target volumetric LTE/nhiều target weight
  không cho quick edit vì nghĩa dừng không đơn nhất; dùng Web để sửa các
  profile phức tạp đó. Mất cân/link khóa thẻ và hủy pending/editor. Star trên
  thẻ YIELD báo có override cho shot tiếp theo.
- Không sửa calibration hoặc encoder Manual/calibration/hot-water. Không
  khôi phục menu TFT; giữ Main không TFT và Recovery đã làm ở Mốc11.

## Quyền sở hữu và truyền thông

UART schema14: Telemetry thêm quickYieldG, yieldEditToken, quickYieldFlags,
selectedProfileIndex. Commands10..13 Begin/Keep/Apply/Cancel có argument là
token ngẫu nhiên nonzero do HMI tạo. Main giữ draft/pending/running trong RAM
qua `QuickYieldSession`, không ghi profile hoặc NVS/SPIFFS. Main được phép
chỉnh chỉ IDLE + cân online + HMI compatible + profile hỗ trợ.

Editor lease2000 ms; HMI renew mỗi600 ms khi command channel không pending.
Sai token/state, hết lease hoặc mất cân/link không Apply được. Begin lặp
trong cùng phiên không xóa draft; HMI không tự báo Apply thành công trước
ACK. HMI mất kết nối hoặc phiên kết thúc thì đóng editor; UART bị nghẽn có
thể khiến editor hết hạn, không kéo dài quyền điều khiển vô hạn.

Set profile khác và ProfilesRuntime::rebuild xóa pending/editor; chạy Flush/
calibration cũng mất pending khi rời IDLE. `beginShotRun` chung nút máy/HMI
tiêu thụ pending một lần trước khi đổi state và tare. Draft chưa Apply không
được dùng. `finishShot` xóa running. Không có override thì dùng profile như cũ.

## Áp dụng vào điều khiển

- Khi Apply, Yield là ngưỡng dừng TOÀN SHOT, kể cả pha đầu. ShotAggregator
  so gram thật với ngưỡng qua `yield_stop_policy.h`; không đợi pha cuối,
  không trừ lượng nhỏ giọt. Chỉ kiểm tra khi hết chuẩn bị tare và cân còn
  được phép dùng cho yield. Không Apply thì profile hoạt động như cũ.
- Custom profile: bản JSON chạy tạm bỏ target volumetric pha cuối khi có
  override (kể cả0), để ngưỡng cũ không dừng sớm khi tăng Yield. Không thêm
  target mới vào pha cuối. Các mốc yield chuyển pha trước, transition và
  điều kiện khác giữ nguyên; không ghi profile hoặc storage.
- Thời gian/pressure/flow exit hoặc limit60 s vẫn có thể kết thúc trước khi
  đạt yield. Tăng yield không tự tăng thời gian phase hay vô hiệu hóa exit khác.
- Mất scale continuity dùng `yieldControlWeight()`/time fallback hiện có;
  reconnect không mở lại quyền yield. Model không thay cân để dừng.
- Manual: quick target dùng cùng auto-yield consumer nhưng so gram trực tiếp,
  không trừ DRIP_OFFSET_GRAMS legacy (custom BrewProcess cũng brewDelay0).
  Nếu không override thì auto-yield legacy giữ như cũ. Không thay PID/PSM.

## Source và kiểm chứng

Main: `src/app/quick_yield.{h,cpp}`, `quick_yield_session.h`, StateMachine,
Orchestrator begin/end/idle name, ProfileEngine runtime clone/reset và
ShotAggregator whole-shot target. MainLink/command handler gửi trạng thái/ACK.
HMI: ReadyLayout, live_screens, YieldEditor actions và lease trong main loop.
Encoder driver thêm tham số accelerate mặc địnhtrue, chỉ editor dùngfalse.

ESP32/ESP32-S3 fsyntax-only PASS cho source Main/HMI thay đổi; sửa lỗi include
profile_engine thiếu trong MainLink trước khi kiểm tra lại thành công.
`tests/hmi_ready/check.ps1` PASS: geometry theo bố cục mới, result cũ, session
thực quick yield (consume một shot, draft không tự apply, sai token, lease
wrap, cancel/loss, giới hạn và Begin lặp). Chưa test hardware/UART runtime,
render pixel hoặc cân tại ngưỡng thực tế cho bản sửa.

Mốc12 ban đầu đã build Main `2026.09.06-1239` và HMI schema14 SUCCESS.
Bản sửa whole-shot sau shot490 đã build Main2026.09.06-1632/HMI schema14
SUCCESS; user đã nạp và xác nhận chạy tốt. Source ADC A/B tiếp theo là mốc
riêng chưa build, không diễn giải thành Yield vẫn lỗi.

### Sửa sau shot490 (2026-09-06)

User cài khoảng26 g: CSV tại5.04 s đạt27.2048 g, tại6.52 s đạt38.5282 g,
pump100%, step0, scale_flags5 (online + yield allowed). CSV không có ngưỡng
đã Apply nên không chứng minh việc lưu pending. Code trước chỉ đặt target
ở pha cuối, không đáp ứng yêu cầu dừng toàn shot; user đã xác nhận sửa nghĩa
này. Không kết luận tare timeout từ shot này. Tests `yield_stop.cpp` replay
ngưỡng CSV với session Apply thật, kiểm tra pre-tare/cân mất/OFF/legacy và
shot kế tiếp không thừa hưởng override. Không đổi PID/PSM/scale policy.
Kiểm tra cú pháp các source đổi Main/HMI PASS, tests/hmi_ready/check.ps1
PASS. Không full build/link/nạp trong lượt sửa này.
Kiểm tra cú pháp các source đổi Main/HMI PASS, tests/hmi_ready/check.ps1
PASS. Không full build/link/nạp trong lượt sửa này.

Checklist sau nạp: tên profile sau calibration, xoay chọn gồm OFF/Manual,
Begin/rotate/Apply/Cancel, mất cân khi chỉnh, đổi profile bỏ override, bắt
đầu bằng nút zin và HMI, profile gốc không đổi, shot sau trở về mặc định,
fallback mất cân giữa pha. Giữ việc điều tra ULKA/Heater SSR ngoài mốc này.
