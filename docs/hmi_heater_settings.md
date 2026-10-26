# Cài PID nhiệt trên màn hình HMI

Source cập nhật ngày 2026-09-19; chưa build/nạp hoặc xác minh trên panel thật.

Mở Settings → System → Heater PID. Dùng PREV/NEXT dưới cùng chuyển giữa Overview, PID Control, Safety & Power, Water Compensation và Auto Sleep. Nhãn tiếng Anh theo font/bố cục touch HMI hiện tại; chỉnh bằng nút +/-.

- Overview: trạng thái kết nối Main, nhiệt thân, công suất lệnh, trần đang áp dụng, mục tiêu và bù nước Main đã nhận; OFF, Preheat, Clear fault, Reload/discard draft.
- PID: target, Kp, Ki, Kd.
- Safety & Power: ceiling, trần giữ nhiệt, trần khi chạy nước, thời gian chờ mở Brew.
- Water compensation: bật FF, công suất heater, nhiệt nước vào, ffScale.
- Auto Sleep: bật tự nghỉ, phút chờ, trễ tắt thêm.

Có thể chỉnh bản nháp khi Main tương thích, có status heater mới và không chờ ACK, kể cả lúc heater đang chạy. SAVE, Preheat và Clear fault chỉ mở khi máy Ready và heater Off/Fault. OFF vẫn có vùng chạm riêng khi status quá hạn; MainLink quyết định khả năng gửi. Main tiếp tục kiểm tra điều kiện an toàn của Configure/Enable/ClearFault. Không thêm đường GPIO hoặc logic PID trong HMI.

Sau lần đồng bộ đầu tiên, màn hình báo `Config loaded from Main` thay vì giữ sai `Waiting for Main`. Giá trị đã lưu hiển thị cyan; trường đã đổi trong bản nháp hiển thị amber. Khi đang gia nhiệt, footer báo vẫn được chỉnh nhưng phải OFF trước khi SAVE. Mỗi nhóm có chú thích ngắn về vai trò tham số.

Form là bản nháp: đổi giá trị không tự gửi. SAVE gửi Configure đầy đủ với ticket ngẫu nhiên; chỉ báo Saved by Main khi nhận đúng ticket và Result::Ok. Rejection giữ bản nháp. Quá 4 giây không có ACK báo không xác nhận và không tự gửi lại; pending transport vẫn giữ theo MainLink. Nếu cấu hình Main thay đổi trong lúc bản nháp đã sửa (ví dụ từ Web), khóa Save và yêu cầu Reload/discard edits. Back rời trang không lưu; mở lại lấy cấu hình Main. Preheat dùng cấu hình Main, không dùng bản nháp chưa lưu.

Implementation: hmi/src/ui/heater_settings.h (draft/bounds/ACK), hmi/src/ui/screens/heater_screen.cpp (render/touch), screen_manager.*, ui_types.h, main.cpp; SettingsLayout dùng chung vùng vẽ/chạm System. Không thay đổi protocol HeaterWire4, NVS, Web hoặc bộ điều khiển Main.

Verification: tests/heater/hmi_settings.cpp constexpr kiểm tra đồng bộ báo đã tải config, draft không bị poll ghi đè, ACK sai ticket/rejection/success, conflict từ Main, timeout, ngưỡng quá nhiệt và giới hạn FF. Test này đã qua kiểm tra cú pháp bằng toolchain ESP32. Chưa build firmware và chưa kiểm tra cảm ứng/render trên panel.
