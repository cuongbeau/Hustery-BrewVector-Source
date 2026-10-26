#include "ui_calibration.h"

#include <math.h>
#include <stdio.h>

#include "src/app/orchestrator.h"
#include "src/ui/tft/tft_driver.h"

namespace {
constexpr uint16_t BG = 0x0842;
constexpr uint16_t CARD = 0x10A4;
constexpr uint16_t CYAN = 0x05FF;
constexpr uint16_t AMBER = 0xFD20;
constexpr uint16_t TEXT = 0xFFFF;
constexpr uint16_t MUTED = 0x9CF3;

float lastPressure = -999.0f;
float lastPower = -999.0f;

void drawValue(float pressure, float power) {
  tft.fillRect(30, 92, W - 60, 64, BG);
  char value[32];
  snprintf(value, sizeof(value), "%.2f bar", pressure);
  tft.setTextDatum(textdatum_t::top_center);
  tft.setTextColor(TEXT, BG);
  tft.setFont(&fonts::FreeSansBold24pt7b);
  tft.drawString(value, W / 2, 97);

  tft.fillRect(30, 212, W - 60, 32, BG);
  snprintf(value, sizeof(value), "PUMP %.0f%%", power);
  tft.setFont(&fonts::FreeSansBold18pt7b);
  tft.setTextColor(AMBER, BG);
  tft.drawString(value, W / 2, 214);
}
}  // namespace

void uiCalibrationStart() {
  lastPressure = lastPower = -999.0f;
  tft.fillScreen(BG);
  tft.drawRoundRect(16, 16, W - 32, H - 32, 10, CYAN);
  tft.setTextDatum(textdatum_t::top_center);
  tft.setFont(&fonts::FreeSansBold18pt7b);
  tft.setTextColor(CYAN, BG);
  tft.drawString("PRESSURE CALIBRATION", W / 2, 33);
  tft.drawFastHLine(38, 72, W - 76, CARD);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(MUTED, BG);
  tft.drawString("TEST PUMP RUNNING", W / 2, 76);
  tft.drawRoundRect(30, 89, W - 60, 70, 8, CARD);
  tft.setTextColor(MUTED, BG);
  tft.drawString("Mechanical gauge is the reference", W / 2, 174);
  tft.drawString("Rotate encoder: -/+ 5% pump", W / 2, 191);
  tft.setTextColor(TEXT, BG);
  tft.drawString("Brew button or HMI STOP = stop", W / 2, 258);
}

void uiCalibrationUpdate(const ShotData& shotData) {
  if (fabsf(shotData.visualPressure - lastPressure) >= 0.02f ||
      fabsf(shotData.pumpPower - lastPower) >= 1.0f) {
    drawValue(shotData.visualPressure, shotData.pumpPower);
    lastPressure = shotData.visualPressure;
    lastPower = shotData.pumpPower;
  }
}

void uiCalibrationDeinit() {}

