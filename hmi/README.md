# Hustery BrewVector HMI

Firmware doc lap cho bo ESP32-S3 ES3C28P 2.8 inch, man hinh ILI9341
landscape 320x240 va cam ung FT6336G.

Truoc khi build, tao `include/local_access.h` theo [INSTALL](../docs/INSTALL.md);
khong dua mat khau thuc vao Git.

Truoc khi build, tao `include/local_access.h` theo [INSTALL](../docs/INSTALL.md);
khong dua mat khau thuc vao Git.

## Build va nap USB lan dau

```powershell
pio run -d hmi -e esp32-s3-devkitc-1
pio run -d hmi -e esp32-s3-devkitc-1 -t upload --upload-port <PORT_HMI>
```

Lan nap USB nay cai OTA bootstrap. Sau do khong can thao man hinh de cam USB nua.

## Ket noi Wi-Fi lan dau

1. Bat HMI va ket noi dien thoai/may tinh vao AP `BrewVector-HMI-XXXX`.
2. Mat khau AP: gia tri `BREWVECTOR_AP_PASSWORD` trong `include/local_access.h` cua ban build.
3. Mo `http://192.168.4.1`, nhap Wi-Fi 2.4 GHz cua nha va bam
   `SAVE & CONNECT`.
4. Sau khi ket noi thanh cong, HMI co dia chi `brewvector-hmi.local`.

AP cai dat se tu tat sau 90 giay khi HMI da vao Wi-Fi nha. Neu mat Wi-Fi trong
15 giay, AP se tu mo lai de co the doi SSID/mat khau ma khong can nap USB.

## Nap OTA

Cach don gian nhat la mo `http://brewvector-hmi.local/update`, dang nhap
`admin` / mat khau `BREWVECTOR_OTA_PASSWORD` trong `include/local_access.h`, sau do chon file:

`hmi/.pio/build/esp32-s3-devkitc-1/firmware.bin`

Neu Windows khong phan giai duoc ten `.local`, xem IP cua HMI tren trang cai
dat/router va mo `http://<IP-HMI>/update`.

Web OTA duoc uu tien vi dung ket noi HTTP mot chieu. Khong dung `espota` tren
Windows de tranh ket noi nguoc bi Firewall chan lam HMI cho den het timeout.

## Kien truc UI

- `src/ui/theme.h`: mau, spacing va kich thuoc dung chung.
- `src/ui/components.*`: header, card, row, button, toggle, slider, chip.
- `src/ui/screen_manager.*`: navigation, chart history va screen composition.
- `src/ui/shot_chart.*`: ring buffer telemetry 20 Hz trong PSRAM, dung chung cho
  Brew, Shot Complete va do thi shot da nap lai tu TF.
- `src/storage/control_trace_store.*`: history source of truth; nhan trace
  schema 10, gom session, buffer 4 KB va luu CSV/index tren TF.
- `src/storage/shot_store.*`: chi giu tuong thich/xoa history v1 cu va thong ke
  dung luong TF; khong tao shot moi.
- `src/ui/ui_types.h`: screen ID va ViewModel telemetry.
- `src/app/demo_data.*`: du lieu gia lap cho phat trien khong can may pha.
- `src/communication/main_link.*`: UART voi bo dieu khien chinh.

Khong them mau hoac ve control rieng trong tung screen neu da co component tuong
ung. Cac screen chi nhan `UiTelemetry`, ve giao dien va phat su kien cham; logic
Wi-Fi, BLE, UART va luu tru se nam trong cac service rieng.

## Man hinh hien co

- Home, Flush, Brew, Shot Complete.
- Brew Profiles lay danh sach runtime tu ESP chinh qua UART; cham profile de
  chon va chi cap nhat ACTIVE sau khi ESP chinh chap nhan.
- Settings page 1 va page 2.
- Wi-Fi, Networks, Scale, Pressure Calibration, Machine.
- Display & Sound, Storage & Logs, System, Diagnostics, Firmware.

## Settings v0.2

- Wi-Fi hien SSID/IP/RSSI that, quet mang bat dong bo, chon mang va nhap mat
  khau bang ban phim cam ung; co Forget va tu mo lai AP cau hinh khi mat mang.
- Brightness PWM 10-100%, auto-dim OFF/1/2/5/10 phut; cham lan dau chi danh
  thuc man hinh de tranh bam nham Start/Stop.
- ES8311 + I2S dung trinh tu da kiem tra tren bo HMI:
  AMP_EN LOW, DMA 8x256, I2S start rieng, DIN GPIO6 va codec unmute 100%; volume
  giao dien duoc ap dung tren bien do PCM.
- Cau hinh HMI luu trong Preferences; restart khong lam mat brightness, volume,
  auto-dim, touch sound hay auto-save.
- TF duoc mount luc khoi dong, hien dung luong that. Main stream Control Trace
  25 Hz; `ShotEnd` dong dung mot session va ghi stop/exit reason.
- Diagnostics hien UART Main, Wi-Fi, touch/audio, TF, heap, PSRAM va uptime.
- Restart HMI co man xac nhan rieng va khong gui lenh restart sang ESP chinh.
- History giu index toi da 40 shot, chia trang 5 shot; cham mot shot de nap lai
  CSV va xem do thi. Compare doi chieu thong so voi shot cu lien ke.
- Erase History co man xac nhan rieng va chi xoa CSV/index tren TF cua HMI.
- Nhung muc chua co du lieu/giao thuc that duoc danh dau `NOT CONFIGURED`,
  `LOCKED` hoac `PLANNED`; UI khong gia lap trang thai da cai vao ESP chinh.

## Profile Editor v0.3

- ESP chinh la nguon profile thuc thi. HMI tai profile day du theo tung step,
  sua tren draft va chi gui lai khi nguoi dung bam `SEND TO MAIN`.
- Library tach khoi Detail. Detail co preview, Activate, Duplicate, Quick va
  Advanced; cham ten profile de mo ban phim rename.
- Quick Edit gom pre-infusion, bloom, main pressure, target yield, max shot time
  va flow cap. Moi thay doi Quick tu tao lai profile 5 step.
- Profile Advanced co toi da 8 step. Moi step tach thanh ba tab Control, Safety
  va Exit Conditions (OR); ho tro add, delete va doi thu tu step.
- Chuyen profile Advanced ve Quick bat buoc qua man canh bao vi cac step thu
  cong se bi thay the.
- UART truyen Begin + tung Step + Commit. Main kiem tra state, gioi han tham so,
  so step va CRC cua toan profile truoc khi ghi SPIFFS/rebuild runtime. Main tu
  choi profile update khi may dang pha.

## Che do du lieu

`HMI_DEMO_MODE=0` trong `platformio.ini`: HMI hien telemetry that tu ESP chinh
qua UART. Cac nut Start Brew, Flush va Stop gui request co sequence; ESP chinh
kiem tra state roi tra ACK/NACK. HMI chi doi man hinh sau khi telemetry that xac
nhan state, khong tu suy doan output da chay.

Command trung sequence khong bi thuc thi lai. Start/Flush co cua so chong bam
lap; Stop duoc uu tien va co deferred-stop neu lenh den dung luc relay Start dang
phat xung.

Nut PROFILES tren Home gui yeu cau danh sach toi ESP chinh. Moi profile gom ten,
so step, target weight va trang thai OFF/MANUAL; ho tro toi da 16 profile va chia
4 profile moi trang. Lenh chon profile bi ESP chinh tu choi neu may dang brew.

Do thi Brew luu theo timestamp cua ESP chinh, toi da 3600 mau (khoang 3 phut o
20 Hz). Renderer gom min/max theo tung cot pixel de giu spike, ve target net dut,
danh dau ranh gioi step va tu mo rong truc thoi gian.

History moi ghi `/brewvector/traces/trace_NNNNNN.csv` va
`/brewvector/traces/index.csv`. Trace gom pressure/flow raw + filtered, pump/CPS,
controller terms/flags, can that + model yield, thermal, step, drop/gap va
metadata ket thuc. Web History co the tai CSV ve may tinh.

## Scale BLE

HMI la BLE central, ho tro driver DIY Hustery, Acaia va Timemore. Scan thu cong
chi liet ke; ket noi sau khi nguoi dung chon MAC. MAC/driver duoc luu de auto
reconnect. Sample stale sau 750 ms va duoc gui sang Main moi 50 ms. Snapshot
giua NimBLE callback va Arduino loop duoc bao ve bang `portMUX`.
