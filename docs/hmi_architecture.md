# ESP32-S3 HMI Architecture

> Tài liệu tổng thể và ranh giới runtime hiện hành nằm tại
> `docs/system_architecture.md`. File này giữ vai trò mô tả phạm vi giao diện.

## Mục tiêu

Tách giao diện cảm ứng, kết nối và lưu trữ khỏi ESP32 điều khiển chính. Main ESP giữ quyền điều khiển pump, heater, profile executor, cảm biến và toàn bộ logic an toàn.

## Phân công hai màn hình

### TFT trên — Main ESP

1. Boot/self-test.
2. Heating.
3. Ready/Idle.
4. Profile quick-select dự phòng bằng encoder.
5. Brew Live: số lớn, không đồ thị.
6. Ending/Stopping.
7. Shot Complete.
8. Hot Water/Steam.
9. Calibration status.
10. Warning/Fault.

### Touchscreen dưới — ESP32-S3

1. HMI boot và trạng thái kết nối.
2. Home dashboard.
3. Profile library.
4. Profile preview.
5. Profile editor và curve validation.
6. Live Brew chart.
7. Shot Result: Overview, Graph, Steps.
8. Shot History.
9. Compare Shots.
10. Connections: Main, Scale, Wi-Fi, BLE.
11. Calibration wizards.
12. Machine, display và audio settings.
13. Storage/import/export.
14. System/firmware.
15. Diagnostics/Service Mode.

## Quyền sở hữu dữ liệu

| Dữ liệu/chức năng | Chủ sở hữu |
|---|---|
| Pump, heater, relay, valve | Main ESP |
| Sensor và machine state | Main ESP |
| Profile đang chạy | Main ESP |
| Safety limits | Main ESP |
| Weight-stop prediction | Main ESP |
| Profile lưu và thực thi | Main ESP |
| Profile draft/editor/cache | HMI |
| Wi-Fi, Web, BLE | HMI |
| BLE scale + reconnect/pair | HMI; sample chuyển UART sang Main |
| Lịch sử shot dài hạn | HMI/TF |
| Touch, theme, audio | HMI |

## Trạng thái triển khai

UART hai chiều, heartbeat/CRC/ACK, command an toàn, profile sync, calibration,
Wi-Fi/Web/TF, BLE scale và OTA Main qua HMI đều đã triển khai. Protocol hiện là
schema 10; Control Trace + ShotEnd tạo History chuẩn trên TF.

## Ranh giới module HMI

- `communication/MainLink`: transport/frame state; không render UI.
- `scale`: scan/pair/driver/normalized sample; không quyết định pump.
- `storage/ControlTraceStore`: session, buffer TF, index và load chart.
- `network/HmiOta`: Wi-Fi/AP/mDNS; HTTP route nằm trong `web`.
- `web/routes`: Dashboard, History và Firmware API.
- `ui/screens`: render/touch theo từng miền, dùng component/theme chung.
