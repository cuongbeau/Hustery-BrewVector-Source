// File: src/ui/tft_driver.cpp
// TFT instance + init
// Refactor: 2026-04-20

#include "tft_driver.h"

LGFX tft;
const int W = 480;
const int H = 320;

void uiInit() {
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextWrap(false, false);
}

void uiInitDMA() {
  tft.initDMA();
}
