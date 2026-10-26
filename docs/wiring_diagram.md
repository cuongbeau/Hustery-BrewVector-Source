# Linh kiện và kết nối Hustery BrewVector

Source of truth: `config/hardware_pins.h`.

Máy nền được mô tả ở [STOCK_MACHINE.md](STOCK_MACHINE.md). Số lượng dưới đây
dành cho **một máy** và từ khóa có thể dùng để tìm linh kiện. Đây là gợi ý mua
theo phần cứng và firmware hiện tại, chưa phải bộ linh kiện đã thử trên mọi
phiên bản máy. Đối chiếu ảnh, nhãn chân và kích thước với máy của mình trước
khi đặt.

## Gợi ý mua linh kiện

### Có thể tìm mua theo mã hoặc loại bo

| Linh kiện | SL | Từ khóa tìm mua | Kiểm tra trước khi đặt |
|---|---:|---|---|
| Bo điều khiển Main | 1 | `ESP32 DevKit V1 38 pin` | Đối chiếu nhãn GPIO và vị trí chân với sơ đồ bên dưới; `esp32dev` chỉ là môi trường build, chưa xác nhận chip USB-UART/revision. |
| Bo HMI có cảm ứng | 1 | `ES3C28P 2.8 inch ESP32-S3 capacitive touch` | Đúng ES3C28P, LCD ILI9341 và touch FT6336G; xác nhận revision. Không chọn biến thể ES3N28P không cảm ứng. [Tài liệu nhà sản xuất](https://www.lcdwiki.com/res/ES3C28P/2.8inch_IPS_ESP32-S3_ES3C28P_ES3N28P_User_Manual.pdf). |
| Module đọc nhiệt | 1 | `MAX31855 K type thermocouple module` | Chọn module MAX31855 dành cho Type K, có chân CS/CLK/DO; không thay bằng MAX6675 chỉ vì hình giống. |
| Đầu dò nhiệt | 1 | `ungrounded K type thermocouple probe` | Cần gá chắc lên thân thermoblock; kích thước và kiểu gá chưa được ghi lại. Đầu đo nối vỏ kim loại có thể không phù hợp MAX31855. [Hướng dẫn Adafruit](https://learn.adafruit.com/thermocouple?view=all). |
| Module ADC áp suất | 1 | `ADS1115 16 bit I2C module` | Đúng ADS1115, không nhầm ADS1015; firmware dùng địa chỉ I²C `0x48` và ngõ A0. Cảm biến áp suất là món riêng ở bảng dưới. |
| Dimmer bơm | 1 | `RobotDyn AC dimmer 1 channel ZC PSM` | Có chân VCC/GND/Z-C/PSM; đối chiếu phiên bản, điện áp và tải bơm. [Link mua tham khảo do chủ dự án cung cấp](https://vi.aliexpress.com/item/32802025086.html). |
| Chuyển mức logic | 1 | `TXS0102 2 channel level shifter module` | Có chân VCCA/VCCB/OE; sơ đồ hiện dùng 3,3 V phía ESP32 và 5 V phía dimmer. [Thông số TI](https://www.ti.com/product/TXS0102). |
| Núm xoay kèm nhấn | 1 | `rotary encoder CLK DT SW push button` | Chọn kiểu trục, núm, chiều dài và cách gá vừa vị trí lắp. |
| Thẻ nhớ HMI | 1 | `microSD 16GB` | 16 GB là **đề xuất để thử**, chưa ghi nhận dung lượng/định dạng đã kiểm chứng trên bo này. |

### Cần xác nhận trên máy rồi mới chốt mã mua

| Linh kiện | SL tham khảo | Gợi ý tìm kiếm | Thông tin còn thiếu |
|---|---:|---|---|
| Cảm biến áp suất nước | 1 | `water pressure transducer 0-20 bar analog` | Ren/cách lắp, nguồn cấp và dải điện áp ngõ ra. Firmware đọc qua ADS1115 và hiệu chuẩn theo điện áp; ngõ vào ADC không được vượt nguồn cấp của ADC. [Giới hạn ADS1115](https://www.ti.com/lit/ds/symlink/ads1115.pdf). |
| SSR cho heater | 1 | `SSR AC output DC control heater` | Điện áp/công suất heater, điện áp kích thực tế từ mạch điều khiển, tản nhiệt và cách lắp. Chưa xác nhận mã SSR cụ thể. |
| Relay bấm nút shot | 1 | `single channel dry contact relay active low` | Firmware xuất mức LOW để kích; cần xác nhận module nhận mức điều khiển 3,3 V và tiếp điểm phù hợp mạch nút của máy. |
| Mạch đọc tín hiệu van | 2 ngõ | `PC817 optocoupler input module` | Firmware có hai tín hiệu sense; phải biết điện áp/dạng tín hiệu của bo zin trước khi chọn module. Không đưa điện lưới trực tiếp vào module PC817 chung chung. |

Cân BLE là **tùy chọn**: chọn loại được driver Hustery/Acaia/Timemore hỗ
trợ; dừng theo gram chỉ dùng khi HMI nhận mẫu cân hợp lệ. Bơm, heater, van,
nguồn và phần cơ khí của máy nền không nằm trong danh sách mua thêm này; xem
[máy nền](STOCK_MACHINE.md) trước khi dùng linh kiện thay thế.

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
