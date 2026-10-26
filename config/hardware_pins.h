// File: config/hardware_pins.h
// Version: 2025-11-21_01
// Changelog: init version
// Refactor: Di chuyển từ root → config/ (giữ nguyên nội dung)

#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

// Legacy TFT pins: reserved for wiring reference, unused after HMI-only cutover.
// Do not repurpose these pins as part of display removal.
// ================== TFT SPI (legacy) ==================
#define TFT_SCLK   18                                           
#define TFT_MOSI   23                 
#define TFT_MISO   -1                                                    
#define TFT_DC      2
#define TFT_CS      5               
#define TFT_RST     4

// ================== MAX31855 ==================
#define MAX31855_CS   27
#define MAX31855_SCK  19
#define MAX31855_SO   21

// ================== PRESSURE ADC ==================
#define PRESSURE_ADC_PIN  34
#define I2C_SDA_PIN 16
#define I2C_SCL_PIN 17

// ================== DIMMER / PSM ==================
#define DIMMER_ZC_PIN   26   // Zero-cross input (interrupt)
#define DIMMER_PSM_PIN  14   // Gate/PSM output

// ================== ENCODER ==================
#define ENC_CLK   32
#define ENC_DT    33
#define ENC_SW    25   // nút nhấn encoder (gom nút cũ)

// ================== ALIAS NÚT CŨ (nếu code chỗ khác còn dùng) ==================
#define BTN_PROFILE  ENC_SW
#define BTN_START    ENC_SW

// ================== SSR HEATER (PID) ==================
#define SSR_HEATER_PIN 13   // GPIO điều khiển SSR heater

// ================== OPTOQUANG (van) ==================
#define CURRENT_SENSE_PIN  35   // OUT của MODULE OPTOQUANG -> GPIO35

// ================== VAN NƯỚC NÓNG (OPTOQUANG) ==================
#define HOTWATER_SENSE_PIN 15   // OUT của PC817 van nước nóng

// ================== RELAY BẤM NÚT VẬT LÝ ==================
#define RELAY_SHOT_PIN 22       // Chân điều khiển Relay (Dùng 22 thay vì 12 để tránh lỗi Boot)

#endif // HARDWARE_PINS_H
