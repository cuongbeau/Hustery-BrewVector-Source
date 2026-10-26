#include "ui_hotwater.h"
#include "ui_theme.h"

namespace {
constexpr uint16_t BG = TftTheme::BG;
constexpr uint16_t CARD = TftTheme::CARD;
constexpr uint16_t BORDER = TftTheme::BORDER;
constexpr uint16_t TEXT = TftTheme::TEXT;
constexpr uint16_t MUTED = TftTheme::MUTED;
constexpr uint16_t CYAN = TftTheme::CYAN;
constexpr uint16_t AMBER = TftTheme::AMBER;

LGFX_Sprite tempSpr(&tft);
LGFX_Sprite metricsSpr(&tft);
bool tempReady = false;
bool metricsReady = false;
float lastTemp = -999.0f;
float lastWater = -999.0f;
float lastPump = -999.0f;

bool ensureSprite(LGFX_Sprite& sprite, bool& ready, int width, int height) {
  if (ready) return true;
  sprite.setColorDepth(8);
  ready = sprite.createSprite(width, height) != nullptr;
  return ready;
}

void pushSpriteAtomic(LGFX_Sprite& sprite, int x, int y) {
  tft.startWrite();
  sprite.pushSprite(x, y);
  tft.endWrite();
}

void drawTemperature(float temp) {
  if (!ensureSprite(tempSpr, tempReady, 456, 110)) return;
  tempSpr.fillSprite(CARD);

  tempSpr.setTextDatum(textdatum_t::top_center);
  tempSpr.setFont(&fonts::Font2);
  tempSpr.setTextColor(MUTED, CARD);
  tempSpr.drawString("WATER TEMPERATURE", 228, 12);

  tempSpr.setFont(&fonts::FreeSansBold24pt7b);
  tempSpr.setTextColor(TEXT, CARD);
  char value[24];
  snprintf(value, sizeof(value), "%.1f C", temp);
  tempSpr.drawString(value, 228, 44);

  pushSpriteAtomic(tempSpr, 12, 54);
  lastTemp = temp;
}

void drawMetrics(float water, float pump) {
  if (!ensureSprite(metricsSpr, metricsReady, 456, 100)) return;
  metricsSpr.fillSprite(BG);

  // Left card: DISPENSED WATER
  metricsSpr.fillRoundRect(0, 0, 220, 100, 8, CARD);
  metricsSpr.drawRoundRect(0, 0, 220, 100, 8, BORDER);
  metricsSpr.setTextDatum(textdatum_t::top_center);
  metricsSpr.setFont(&fonts::Font2);
  metricsSpr.setTextColor(MUTED, CARD);
  metricsSpr.drawString("DISPENSED WATER", 110, 14);
  metricsSpr.setFont(&fonts::FreeSansBold18pt7b);
  metricsSpr.setTextColor(AMBER, CARD);
  char value[24];
  snprintf(value, sizeof(value), "%.0f g", water);
  metricsSpr.drawString(value, 110, 48);

  // Right card: PUMP POWER
  metricsSpr.fillRoundRect(236, 0, 220, 100, 8, CARD);
  metricsSpr.drawRoundRect(236, 0, 220, 100, 8, BORDER);
  metricsSpr.setFont(&fonts::Font2);
  metricsSpr.setTextColor(MUTED, CARD);
  metricsSpr.drawString("PUMP POWER", 346, 14);
  metricsSpr.setFont(&fonts::FreeSansBold18pt7b);
  metricsSpr.setTextColor(CYAN, CARD);
  snprintf(value, sizeof(value), "%.0f %%", pump);
  metricsSpr.drawString(value, 346, 48);

  pushSpriteAtomic(metricsSpr, 12, 174);
  lastWater = water;
  lastPump = pump;
}
}  // namespace

void uiHotStart() {
  tft.fillScreen(BG);
  tft.setTextDatum(textdatum_t::top_left);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(CYAN, BG);
  tft.drawString("HOT WATER DISPENSER", 16, 14);

  tft.fillRoundRect(12, 54, 456, 110, 8, CARD);
  tft.drawRoundRect(12, 54, 456, 110, 8, BORDER);

  tft.setTextDatum(textdatum_t::bottom_center);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(MUTED, BG);
  tft.drawString("TURN KNOB TO ADJUST PUMP  |  HOLD TO STOP", W / 2, 304);

  uiInitDMA();
  lastTemp = lastWater = lastPump = -999.0f;
}

void uiHotUpdate(const ShotData& data) {
  const float temp = data.brewTempEstC > 0.0f ? data.brewTempEstC : data.temperature;
  if (fabsf(temp - lastTemp) >= 0.2f) drawTemperature(temp);
  if (fabsf(data.outGrams - lastWater) >= 0.5f ||
      fabsf(data.pumpPower - lastPump) >= 1.0f) {
    drawMetrics(data.outGrams, data.pumpPower);
  }
}

void uiHotDeinit() {
  tempSpr.deleteSprite();
  metricsSpr.deleteSprite();
  tempReady = metricsReady = false;
}

