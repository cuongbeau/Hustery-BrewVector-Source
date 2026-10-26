#include "hardware/hmi_display.h"

#include <Arduino.h>

#include "board_pins.h"

HmiDisplay::HmiDisplay() {
  {
    auto cfg = bus_.config();
    cfg.spi_host = SPI2_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000;
    cfg.freq_read = 16000000;
    cfg.spi_3wire = false;
    cfg.use_lock = true;
    cfg.dma_channel = SPI_DMA_CH_AUTO;
    cfg.pin_sclk = BoardPins::LCD_SCK;
    cfg.pin_mosi = BoardPins::LCD_MOSI;
    cfg.pin_miso = BoardPins::LCD_MISO;
    cfg.pin_dc = BoardPins::LCD_DC;
    bus_.config(cfg);
    panel_.setBus(&bus_);
  }

  {
    auto cfg = panel_.config();
    cfg.pin_cs = BoardPins::LCD_CS;
    cfg.pin_rst = -1;
    cfg.pin_busy = -1;
    cfg.panel_width = 240;
    cfg.panel_height = 320;
    cfg.offset_x = 0;
    cfg.offset_y = 0;
    cfg.offset_rotation = 0;
    cfg.readable = true;
    // ES3C28P's ILI9341 panel is wired for the inverted command set.
    // Leaving this false turns near-black navy into a yellow background.
    cfg.invert = true;
    cfg.rgb_order = false;
    cfg.dlen_16bit = false;
    cfg.bus_shared = true;
    panel_.config(cfg);
  }

  setPanel(&panel_);
}

void HmiDisplay::beginPanel() {
  pinMode(BoardPins::LCD_BL, OUTPUT);
  digitalWrite(BoardPins::LCD_BL, LOW);

  init();
  // The HMI is mounted upside-down in the enclosure. Rotation 3 keeps the
  // 320x240 landscape layout while turning the whole UI by 180 degrees.
  setRotation(3);
  fillScreen(TFT_BLACK);

  ledcAttach(BoardPins::LCD_BL, 12000, 8);
  setBacklight(100);
}

void HmiDisplay::setBacklight(uint8_t percent) {
  backlight_ = constrain(percent, 0, 100);
  ledcWrite(BoardPins::LCD_BL, static_cast<uint32_t>(backlight_) * 255U / 100U);
}
