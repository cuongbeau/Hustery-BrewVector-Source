#pragma once

#include <Arduino.h>

namespace BoardPins {

constexpr gpio_num_t LCD_SCK = GPIO_NUM_12;
constexpr gpio_num_t LCD_MOSI = GPIO_NUM_11;
constexpr gpio_num_t LCD_MISO = GPIO_NUM_13;
constexpr gpio_num_t LCD_CS = GPIO_NUM_10;
constexpr gpio_num_t LCD_DC = GPIO_NUM_46;
constexpr gpio_num_t LCD_BL = GPIO_NUM_45;

constexpr gpio_num_t TOUCH_SDA = GPIO_NUM_16;
constexpr gpio_num_t TOUCH_SCL = GPIO_NUM_15;
constexpr gpio_num_t TOUCH_RST = GPIO_NUM_18;
constexpr gpio_num_t TOUCH_INT = GPIO_NUM_17;
constexpr uint8_t TOUCH_ADDRESS = 0x38;

constexpr gpio_num_t AUDIO_AMP_EN = GPIO_NUM_1;
constexpr gpio_num_t AUDIO_MCLK = GPIO_NUM_4;
constexpr gpio_num_t AUDIO_BCLK = GPIO_NUM_5;
constexpr gpio_num_t AUDIO_DOUT = GPIO_NUM_8;
constexpr gpio_num_t AUDIO_LRCK = GPIO_NUM_7;
constexpr gpio_num_t AUDIO_DIN = GPIO_NUM_6;

constexpr gpio_num_t RGB_LED = GPIO_NUM_42;

constexpr gpio_num_t MAIN_UART_TX = GPIO_NUM_43;
constexpr gpio_num_t MAIN_UART_RX = GPIO_NUM_44;

constexpr gpio_num_t SD_CLK = GPIO_NUM_38;
constexpr gpio_num_t SD_CMD = GPIO_NUM_40;
constexpr gpio_num_t SD_D0 = GPIO_NUM_39;
constexpr gpio_num_t SD_D1 = GPIO_NUM_41;
constexpr gpio_num_t SD_D2 = GPIO_NUM_48;
constexpr gpio_num_t SD_D3 = GPIO_NUM_47;

constexpr gpio_num_t BATTERY_ADC = GPIO_NUM_9;

}  // namespace BoardPins

