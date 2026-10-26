// File: src/ui/tft_driver.h
// Tách từ ui_display.h dòng 28-61 — LGFX class + init
// Refactor: 2026-04-20

#ifndef TFT_DRIVER_H
#define TFT_DRIVER_H

#include <LovyanGFX.hpp>
#include <lgfx/v1/lgfx_fonts.hpp>
#include "config/hardware_pins.h"

// ====== LGFX PANEL (ST7796 480×320 landscape) ======
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 _panel;
  lgfx::Bus_SPI _bus;
public:
  LGFX(void){
    { // SPI bus
      auto c = _bus.config();
      c.spi_host   = SPI2_HOST;
      c.spi_mode   = 0;
      c.freq_write = 27000000;
      c.pin_sclk   = TFT_SCLK;
      c.pin_mosi   = TFT_MOSI;
      c.pin_miso   = TFT_MISO;
      c.pin_dc     = TFT_DC;
      _bus.config(c);
      _panel.setBus(&_bus);
    }
    { // Panel
      auto c = _panel.config();
      c.pin_cs          = TFT_CS;
      c.pin_rst         = TFT_RST;
      c.panel_width     = 320;
      c.panel_height    = 480;
      c.offset_rotation = 1;
      c.readable        = false;
      c.invert          = false;
      c.rgb_order       = false;
      c.bus_shared      = false;
      _panel.config(c);
    }
    setPanel(&_panel);
  }
};

extern LGFX tft;
extern const int W;
extern const int H;

void uiInit();
void uiInitDMA();

#endif // TFT_DRIVER_H
