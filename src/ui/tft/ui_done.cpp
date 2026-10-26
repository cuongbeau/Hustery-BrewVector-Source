// Upper TFT shot summary: extraction scorecard & telemetry summary.
// Detailed interactive graph and comparison live on the lower HMI.

#include "ui_done.h"

#include <stdio.h>

#include "interfaces/system_state.h"
#include "src/communication/hmi_link.h"
#include "src/control/profile_engine.h"
#include "src/hardware/scale_service.h"
#include "ui_theme.h"

namespace {
constexpr uint16_t BG = TftTheme::BG;
constexpr uint16_t CARD = TftTheme::CARD;
constexpr uint16_t CARD_ALT = TftTheme::CARD_ALT;
constexpr uint16_t BORDER = TftTheme::BORDER;
constexpr uint16_t TEXT = TftTheme::TEXT;
constexpr uint16_t MUTED = TftTheme::MUTED;
constexpr uint16_t DIM = TftTheme::DIM;
constexpr uint16_t CYAN = TftTheme::CYAN;
constexpr uint16_t AMBER = TftTheme::AMBER;
constexpr uint16_t GREEN = TftTheme::GREEN;

void drawMetricCard(int x, int y, int w, int h, const char* label, const char* value, uint16_t valCol) {
  tft.fillRoundRect(x, y, w, h, 8, CARD);
  tft.drawRoundRect(x, y, w, h, 8, BORDER);

  tft.setTextDatum(textdatum_t::top_center);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(MUTED, CARD);
  tft.drawString(label, x + w / 2, y + 8);

  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(valCol, CARD);
  tft.drawString(value, x + w / 2, y + 32);
}

void drawSummary() {
  tft.fillScreen(BG);

  // Top Status Badge [ SHOT COMPLETE ]
  tft.fillRoundRect(16, 12, 170, 32, 6, CARD);
  tft.drawRoundRect(16, 12, 170, 32, 6, GREEN);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setFont(&fonts::FreeSansBold9pt7b);
  tft.setTextColor(GREEN, CARD);
  tft.drawString("SHOT COMPLETE", 101, 28);

  // Profile Name on top-right
  const int profileIndex = getCurrentProfileIndex();
  const char* fallback = (profileIndex >= 0 && profileIndex < profilesCount)
                             ? profiles[profileIndex].name : "---";
  const char* name = (lastShot.profileName.length() && lastShot.profileName != "N/A")
                         ? lastShot.profileName.c_str() : fallback;
  char fitted[40];
  snprintf(fitted, sizeof(fitted), "%.32s", name);
  tft.setTextDatum(textdatum_t::middle_right);
  tft.setFont(&fonts::FreeSansBold9pt7b);
  tft.setTextColor(TEXT, BG);
  tft.drawString(fitted, W - 16, 28);

  const float timeS = lastShot.time > 0.01f ? lastShot.time : shot.shotTime / 1000.0f;
  const float weight = shot.outGrams > 0.05f ? shot.outGrams : lastShot.weight;
  const float maxP = shot.maxPressure > 0.1f ? shot.maxPressure : lastShot.peakPressure;
  const float temp = shot.tempAvg > 10.0f ? shot.tempAvg : (shot.brewTempEstC > 0 ? shot.brewTempEstC : shot.temperature);
  const float flowAvg = (timeS > 0.5f && weight > 0.1f) ? (weight / timeS) : 0.0f;
  const float waterPumped = shot.waterPumped > 0.1f ? shot.waterPumped : weight * 1.3f;

  char text[24];

  // Row 1: 4 Main Metrics (W = 108 each)
  snprintf(text, sizeof(text), "%.1fs", timeS);
  drawMetricCard(12, 54, 108, 64, "TIME", text, TEXT);

  snprintf(text, sizeof(text), "%.1fg", weight);
  drawMetricCard(128, 54, 108, 64, ScaleService::isOnline() ? "YIELD" : "YIELD EST", text, CYAN);

  snprintf(text, sizeof(text), "%.1f bar", maxP);
  drawMetricCard(244, 54, 108, 64, "PEAK", text, AMBER);

  snprintf(text, sizeof(text), "%.1f C", temp);
  drawMetricCard(360, 54, 108, 64, "TEMP AVG", text, TftTheme::ORANGE_RED);


  // Row 2: 2 Secondary Analytics (W = 224 each)
  snprintf(text, sizeof(text), "%.1f g/s", flowAvg);
  drawMetricCard(12, 126, 224, 64, "EXTRACTION FLOW AVG", text, AMBER);

  snprintf(text, sizeof(text), "%.1f g", waterPumped);
  drawMetricCard(244, 126, 224, 64, "TOTAL WATER PUMPED", text, CYAN);

  // Info banner
  tft.fillRoundRect(12, 198, 456, 44, 8, CARD_ALT);
  tft.drawRoundRect(12, 198, 456, 44, 8, BORDER);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(HmiLink::compatible() ? CYAN : MUTED, CARD_ALT);
  tft.drawString(HmiLink::compatible() ? "CURVES & ANALYSIS SYNCED TO HMI" : "STANDALONE INSTRUMENT SUMMARY", W / 2, 220);

  // Return to ready button pill
  tft.fillRoundRect(130, 256, 220, 48, 8, CARD);
  tft.drawRoundRect(130, 256, 220, 48, 8, GREEN);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(GREEN, CARD);
  tft.drawString("CLICK TO READY", W / 2, 280);
}
}  // namespace

void uiEndStart() {
  uiInitDMA();
  drawSummary();
}

void uiEndUpdate(const ShotData&, float, bool) {}
void uiEndDeinit() {}
void uiEndRotate(int) {}

