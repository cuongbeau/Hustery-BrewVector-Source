# Linh kiện và kết nối Hustery BrewVector

Source of truth: `config/hardware_pins.h`.

Máy nền được mô tả ở [STOCK_MACHINE.md](STOCK_MACHINE.md). Đây là danh sách
theo **chức năng trong source**, chưa phải giỏ hàng mua sắm đã kiểm chứng cho
mọi phiên bản máy. Mã module, điện áp tải và chân vật lý phải đối chiếu với
linh kiện thực tế trước khi đấu.

## Linh kiện

| Cụm | Linh kiện / vai trò | Trạng thái mã mua hàng |
|---|---|---|
| Main | ESP32 DevKit V1 bản 38 chân; môi trường build `esp32dev` | Loại bo theo máy dự án; chưa ghi chip USB-UART/revision |
| HMI | ESP32-S3 ES3C28P, LCD ILI9341, touch FT6336G | Model bo đã ghi; cần xác nhận revision |
| Nhiệt | MAX31855 và cặp nhiệt Type K đo thân thermoblock | Cần mã đầu dò/cách gá đã thử |
| Áp suất | Cảm biến áp suất đọc qua ADS1115 | Cần mã cảm biến, dải áp và nguồn cấp |
| Bơm | Module dimmer AC RobotDyn điều khiển pha, TXS0102 chuyển mức 3,3/5 V | [Link mua tham khảo (AliExpress, item 32802025086)](https://vi.aliexpress.com/item/32802025086.html); cần đối chiếu biến thể/revision |
| Heater và lệnh | SSR heater, relay nút shot | Cần mã, thông số điện và mạch tải đã kiểm chứng |
| Phản hồi van | Module opto/PC817 cho sense | Cần mã module và đặc tính tín hiệu |
| Thao tác | Encoder xoay kèm nút nhấn | Cần mã cơ khí/đầu nối |
| Lưu trữ | Thẻ TF phù hợp khe HMI | Cần mức dung lượng đã thử |
| Tuỳ chọn | Cân BLE tương thích driver Hustery/Acaia/Timemore | Dừng theo gram chỉ dùng khi cân thật hợp lệ |

Bo Main đang dùng là ESP32 DevKit V1 **38 chân**. Tên DevKit V1 được dùng cho
nhiều bo có bố trí hoặc linh kiện phụ khác nhau; trước khi đấu dây phải đối
chiếu nhãn GPIO trên đúng bo. `esp32dev` là môi trường build, không thay thế
việc xác nhận sơ đồ chân vật lý.

Link RobotDyn ở trên do chủ dự án cung cấp để tham khảo mua hàng. Trang bán có
thể đổi lựa chọn sản phẩm hoặc revision; hãy đối chiếu module nhận được với
chân `VCC`, `GND`, `Z-C`, `PSM` và sơ đồ logic phía dưới. Link này không xác nhận
sơ đồ dây AC hay tính tương thích của mọi lô hàng.

Nguồn, cầu chì, đầu nối, dây, cách điện và vỏ máy phải được chọn theo phần
cứng/tải thực tế. Chưa có sơ đồ công suất AC hoàn chỉnh hoặc mã linh kiện đủ
để coi bảng này là hướng dẫn lắp điện lưới. Tài liệu dưới đây chỉ mô tả logic
và tín hiệu thấp áp.

Các hình tham khảo riêng cho [ADC](wiring/01-ads1115.svg),
[MAX31855](wiring/02-max31855.svg), [RobotDyn](wiring/03-robotdyn.svg),
[heater/relay](wiring/04-heater-relay.svg), [HMI–Main](wiring/05-hmi.svg),
[encoder](wiring/06-encoder.svg), [opto](wiring/07-opto.svg).

## 1. Chân TFT Main cũ (chỉ tham khảo)

| Signal | ESP32 GPIO | Ghi chú |
| :--- | :--- | :--- |
| `TFT_SCLK` | GPIO 18 | SPI clock |
| `TFT_MOSI` | GPIO 23 | SPI MOSI |
| `TFT_MISO` | `-1` | Không dùng |
| `TFT_DC` | GPIO 2 | Data/command |
| `TFT_CS` | GPIO 5 | Chip select |
| `TFT_RST` | GPIO 4 | Reset |

## 2. Encoder

| Signal | ESP32 GPIO | Ghi chú |
| :--- | :--- | :--- |
| `ENC_CLK` | GPIO 32 | Rotary encoder CLK |
| `ENC_DT` | GPIO 33 | Rotary encoder DT |
| `ENC_SW` | GPIO 25 | Nút nhấn encoder |
| `BTN_PROFILE` | GPIO 25 | Alias của `ENC_SW` |
| `BTN_START` | GPIO 25 | Alias của `ENC_SW` |

## 3. Cảm Biến Nhiệt Độ và Áp Suất

### MAX31855 Thermocouple

| Signal | ESP32 GPIO | Ghi chú |
| :--- | :--- | :--- |
| `MAX31855_CS` | GPIO 27 | Thermocouple chip select |
| `MAX31855_SCK` | GPIO 19 | Thermocouple SPI clock |
| `MAX31855_SO` | GPIO 21 | Thermocouple serial output |

### Pressure ADC

| Signal | ESP32 GPIO | Ghi chú |
| :--- | :--- | :--- |
| `I2C_SDA_PIN` | GPIO 16 | ADS1115 SDA |
| `I2C_SCL_PIN` | GPIO 17 | ADS1115 SCL |
| `PRESSURE_ADC_PIN` | GPIO 34 | Định nghĩa legacy/direct pressure ADC input |

## 4. Pump, Heater và Actuators

> [!CAUTION]
> Pump, heater, relay, dimmer và mains wiring có thể có điện áp nguy hiểm. Kiểm tra cách ly, cầu chì, nối đất và enclosure safety trước khi cấp nguồn.

| Signal | ESP32 GPIO | Ghi chú |
| :--- | :--- | :--- |
| `DIMMER_ZC_PIN` | GPIO 26 | Zero-cross input interrupt |
| `DIMMER_PSM_PIN` | GPIO 14 | Pump PSM/gate output |
| `SSR_HEATER_PIN` | GPIO 13 | SSR heater control |
| `RELAY_SHOT_PIN` | GPIO 22 | Relay điều khiển nút shot vật lý; dùng GPIO 22 để tránh lỗi boot |

### RobotDyn 5 V và chuyển mức logic

RobotDyn trong máy được cấp 5 V. GPIO ESP32 chỉ dùng logic 3.3 V, vì vậy không
nối trực tiếp `Z-C` 5 V vào GPIO26. Sơ đồ logic dùng TXS0102 và nối chung mass:

| ESP32 / phía A (3.3 V) | TXS0102 | RobotDyn / phía B (5 V) |
|---|---|---|
| 3V3 | `VCCA` | - |
| - | `VCCB` | 5V |
| GND | `GND` | GND |
| GPIO26 | `A1 <-> B1` | `Z-C` |
| GPIO14 | `A2 <-> B2` | `PSM` |

`OE` phải được enable đúng theo module TXS0102 đang dùng. Trước khi nối ESP32,
đo mức HIGH của `Z-C`; nếu lên gần 5 V thì tuyệt đối không nối thẳng. AC
snubber dùng để dập nhiễu ở tải bơm/mains và không thay thế mạch chuyển mức.

## 5. Valve và Sense Inputs

| Signal | ESP32 GPIO | Ghi chú |
| :--- | :--- | :--- |
| `CURRENT_SENSE_PIN` | GPIO 35 | PC817/opto output cho current hoặc valve sense |
| `HOTWATER_SENSE_PIN` | GPIO 15 | PC817 hot-water valve sense input |

## 6. Cân Không Dây

Cân giao tiếp BLE với HMI. HMI chuyển sample cân sang controller chính qua UART;
việc ghép đôi và reconnect được thực hiện trong HMI Settings → Scale.

## 7. UART Main ↔ HMI

Ba dây bắt buộc; mức logic 3.3 V:

| HMI ES3C28P | Main ESP32 | Chức năng |
|---|---|---|
| GPIO43 `TX` | GPIO39 `RX` / `SVN` | HMI gửi command, scale, heartbeat |
| GPIO44 `RX` | GPIO1 `TX` | Main gửi telemetry, trace, reply |
| `GND` | `GND` | Mass chung |

Không nối TX-TX hoặc RX-RX. Baud hiện tại là 230400, `SERIAL_8N1`.

## 8. Pinout HMI ES3C28P

- LCD ILI9341: SCK 12, MOSI 11, MISO 13, CS 10, DC 46, BL 45.
- Touch FT6336G: SDA 16, SCL 15, RST 18, INT 17, I2C `0x38`.
- Audio ES8311: AMP_EN 1 (LOW=on), MCLK 4, BCLK 5, DOUT 8, LRCK 7,
  DIN 6, I2C `0x18`.
- RGB WS2812: GPIO42.
- SD_MMC: CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47.
- Battery ADC: GPIO9.

> [!IMPORTANT]
> Cập nhật file này khi `config/hardware_pins.h` hoặc
> `hmi/include/board_pins.h` thay đổi.
