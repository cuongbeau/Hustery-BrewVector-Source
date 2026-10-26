#pragma once

#include <LovyanGFX.hpp>

class HmiDisplay : public lgfx::LGFX_Device {
 public:
  HmiDisplay();
  void beginPanel();
  void setBacklight(uint8_t percent);
  uint8_t backlight() const { return backlight_; }

 private:
  lgfx::Panel_ILI9341 panel_;
  lgfx::Bus_SPI bus_;
  uint8_t backlight_ = 100;
};
