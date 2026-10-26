// Upper TFT idle screen: machine instrument cluster & telemetry hub.
// Navigation, Wi-Fi, history and configuration belong to the lower HMI.

#include "ui_idle.h"

#include <math.h>
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
constexpr uint16_t RED = TftTheme::RED;

constexpr uint16_t ORANGE_RED = TftTheme::ORANGE_RED;

LGFX_Sprite statusSpr(&tft);
LGFX_Sprite tempSpr(&tft);
LGFX_Sprite scaleSpr(&tft);
LGFX_Sprite profileSpr(&tft);

bool statusSprReady = false;
bool tempSprReady = false;
bool scaleSprReady = false;
bool profileSprReady = false;

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

bool detailView = false;
int lastProfile = -2;
int lastThermalState = -1;
bool lastScaleOnline = false;
float lastBrewTemp = -999.0f;
float lastBlockTemp = -999.0f;
float lastWeight = -999.0f;

int thermalState(float brewC, float blockC) {
  if (!isfinite(brewC) || !isfinite(blockC)) return 0;
  if (blockC > 125.0f || brewC > 102.0f) return 4;
  if (brewC >= 96.5f) return 3;
  if (brewC >= 90.0f) return 2;
  if (brewC >= 80.0f) return 1;
  return 0;
}

const char* thermalLabel(int state) {
  switch (state) {
    case 4: return "OVERHEAT";
    case 3: return "HOT";
    case 2: return "READY";
    case 1: return "PREHEAT";
    default: return "WARMING";
  }
}

uint16_t thermalColor(int state) {
  switch (state) {
    case 4: return RED;
    case 3: return RED;
    case 2: return GREEN;
    case 1: return ORANGE_RED;
    default: return ORANGE_RED;
  }
}

void drawHeaderStatus(float weight, bool scaleOnline, int state) {
  if (!ensureSprite(statusSpr, statusSprReady, 230, 32)) return;
  statusSpr.fillSprite(BG);

  // Pill 1: Scale status [ SCALE 0.0g ]
  statusSpr.fillRoundRect(0, 1, 118, 30, 6, CARD);
  statusSpr.drawRoundRect(0, 1, 118, 30, 6, BORDER);
  statusSpr.setTextDatum(textdatum_t::middle_center);
  statusSpr.setFont(&fonts::Font2);
  char scaleBuf[24];
  if (scaleOnline && isfinite(weight)) {
    snprintf(scaleBuf, sizeof(scaleBuf), "[ SCALE %.1fg ]", weight);
    statusSpr.setTextColor(TEXT, CARD);
  } else {
    snprintf(scaleBuf, sizeof(scaleBuf), "[ SCALE OFF ]");
    statusSpr.setTextColor(MUTED, CARD);
  }
  statusSpr.drawString(scaleBuf, 59, 16);

  // Pill 2: Machine Thermal State [ READY ]
  const uint16_t col = thermalColor(state);
  statusSpr.fillRoundRect(124, 1, 104, 30, 6, CARD);
  statusSpr.drawRoundRect(124, 1, 104, 30, 6, col);
  char stateBuf[24];
  snprintf(stateBuf, sizeof(stateBuf), "[ %s ]", thermalLabel(state));
  statusSpr.setTextColor(col, CARD);
  statusSpr.drawString(stateBuf, 176, 16);

  pushSpriteAtomic(statusSpr, 238, 8);
}

void drawTemperatureContent(float brewC, float blockC) {
  if (!ensureSprite(tempSpr, tempSprReady, 216, 126)) return;
  tempSpr.fillSprite(CARD);

  // Big number 93.5 °C in vivid ORANGE_RED
  char valBuf[20];
  snprintf(valBuf, sizeof(valBuf), "%.1f", brewC);
  tempSpr.setTextDatum(textdatum_t::top_center);
  tempSpr.setFont(&fonts::FreeSansBold24pt7b);
  tempSpr.setTextColor(ORANGE_RED, CARD);
  tempSpr.drawString(valBuf, 94, 2);

  // Unit symbol °C
  tempSpr.setTextDatum(textdatum_t::top_left);
  tempSpr.setFont(&fonts::FreeSansBold12pt7b);
  tempSpr.setTextColor(ORANGE_RED, CARD);
  tempSpr.drawString("o", 160, 4);
  tempSpr.drawString("C", 170, 10);

  // Horizontal Thermal Level Bar: 60..110 C
  constexpr int barX = 12, barY = 60, barW = 192, barH = 8;
  tempSpr.fillRoundRect(barX, barY, barW, barH, 4, BORDER);

  const float ratio = constrain((brewC - 60.0f) / 50.0f, 0.0f, 1.0f);
  const int fillW = static_cast<int>(barW * ratio);
  if (fillW > 3) {
    tempSpr.fillRoundRect(barX, barY, fillW, barH, 4, ORANGE_RED);
  }

  // Subtext: BLOCK: 122 C    FLOW: 0.0 g/s
  char subBuf[44];
  snprintf(subBuf, sizeof(subBuf), "BLOCK: %.0f C    FLOW: %.1f g/s", blockC, shot.visualFlow);
  tempSpr.setTextDatum(textdatum_t::bottom_center);
  tempSpr.setFont(&fonts::Font2);
  tempSpr.setTextColor(MUTED, CARD);
  tempSpr.drawString(subBuf, 108, 124);

  pushSpriteAtomic(tempSpr, 16, 86);
}


void drawScaleContent(float weight, bool scaleOnline) {
  if (!ensureSprite(scaleSpr, scaleSprReady, 216, 126)) return;
  scaleSpr.fillSprite(CARD);

  // Large weight value
  char valBuf[24];
  if (scaleOnline && isfinite(weight)) {
    snprintf(valBuf, sizeof(valBuf), "%.1f g", weight);
  } else {
    snprintf(valBuf, sizeof(valBuf), "--.- g");
  }

  scaleSpr.setTextDatum(textdatum_t::top_center);
  scaleSpr.setFont(&fonts::FreeSansBold24pt7b);
  scaleSpr.setTextColor(scaleOnline ? CYAN : DIM, CARD);
  scaleSpr.drawString(valBuf, 108, 14);

  // Scale status subtext
  scaleSpr.setTextDatum(textdatum_t::bottom_center);
  scaleSpr.setFont(&fonts::Font2);
  if (scaleOnline) {
    scaleSpr.setTextColor(MUTED, CARD);
    scaleSpr.drawString("BLE SCALE READY", 108, 124);
  } else {
    scaleSpr.setTextColor(DIM, CARD);
    scaleSpr.drawString("SCALE OFFLINE", 108, 124);
  }

  pushSpriteAtomic(scaleSpr, 248, 86);
}

void drawProfileContent() {
  const int idx = getCurrentProfileIndex();
  if (!ensureSprite(profileSpr, profileSprReady, 448, 70)) return;
  profileSpr.fillSprite(CARD);

  const char* name = (idx >= 0 && idx < profilesCount) ? profiles[idx].name : "NO PROFILE";
  char titleBuf[64];
  snprintf(titleBuf, sizeof(titleBuf), "PROFILE:  %.36s", name);

  profileSpr.setTextDatum(textdatum_t::top_center);
  profileSpr.setFont(&fonts::FreeSansBold12pt7b);
  profileSpr.setTextColor(TEXT, CARD);
  profileSpr.drawString(titleBuf, 224, 8);

  char detailBuf[80];
  if (idx >= 0 && idx < profilesCount) {
    const Profile& pf = profiles[idx];
    float tgtP = 9.0f;
    for (uint8_t i = 0; i < pf.stepCount; ++i) {
      if (pf.steps[i].mode == MODE_PRESSURE && pf.steps[i].target > 2.0f) {
        tgtP = pf.steps[i].target;
        break;
      }
    }
    snprintf(detailBuf, sizeof(detailBuf), "%u STAGES    %.1f BAR    %.1fg TARGET",
             pf.stepCount, tgtP, pf.totalTargetWeight > 0.1f ? pf.totalTargetWeight : 36.0f);
  } else {
    snprintf(detailBuf, sizeof(detailBuf), "SELECT PROFILE WITH ENCODER");
  }

  profileSpr.setFont(&fonts::Font2);
  profileSpr.setTextColor(AMBER, CARD);
  profileSpr.drawString(detailBuf, 224, 42);

  pushSpriteAtomic(profileSpr, 16, 230);
  lastProfile = idx;
}


void drawIdleBase() {
  tft.fillScreen(BG);

  // Header Title
  tft.setTextDatum(textdatum_t::top_left);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(TEXT, BG);
  tft.drawString("BREWVECTOR", 16, 12);

  // Static Left Card: BREW TEMPERATURE
  tft.fillRoundRect(12, 44, 224, 174, 8, CARD);
  tft.drawRoundRect(12, 44, 224, 174, 8, BORDER);
  tft.setTextDatum(textdatum_t::top_center);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(MUTED, CARD);
  tft.drawString("BREW TEMPERATURE", 124, 56);

  // Static Right Card: LIVE SCALE
  tft.fillRoundRect(244, 44, 224, 174, 8, CARD);
  tft.drawRoundRect(244, 44, 224, 174, 8, BORDER);
  tft.setTextDatum(textdatum_t::top_center);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(MUTED, CARD);
  tft.drawString("LIVE SCALE", 356, 56);

  // Static Bottom Card: PROFILE
  tft.fillRoundRect(12, 224, 456, 84, 8, CARD);
  tft.drawRoundRect(12, 224, 456, 84, 8, BORDER);
}

void drawDetail() {
  tft.fillScreen(BG);
  tft.setTextDatum(textdatum_t::top_left);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(TEXT, BG);
  tft.drawString("BREWVECTOR", 16, 12);

  const int idx = getCurrentProfileIndex();
  const char* profileName = (idx >= 0 && idx < profilesCount) ? profiles[idx].name : "NO PROFILE";
  char fitted[40];
  snprintf(fitted, sizeof(fitted), "%.36s", profileName);

  tft.fillRoundRect(12, 44, 456, 264, 8, CARD);
  tft.drawRoundRect(12, 44, 456, 264, 8, BORDER);

  tft.setTextDatum(textdatum_t::top_center);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(CYAN, CARD);
  tft.drawString(fitted, W / 2, 56);

  if (idx >= 0 && idx < profilesCount) {
    const Profile& pf = profiles[idx];
    char summary[64];
    snprintf(summary, sizeof(summary), "%u STEPS  |  %.0fs MAX  |  %.1fg TARGET",
             pf.stepCount, pf.maxShotTimeS, pf.totalTargetWeight);
    tft.setFont(&fonts::Font2);
    tft.setTextColor(MUTED, CARD);
    tft.drawString(summary, W / 2, 82);

    const int shown = min<int>(pf.stepCount, 5);
    for (int i = 0; i < shown; ++i) {
      const AdvancedStep& step = pf.steps[i];
      const int y = 110 + i * 28;
      tft.setTextDatum(textdatum_t::top_left);
      tft.setFont(&fonts::FreeSansBold9pt7b);
      tft.setTextColor(i == 0 ? AMBER : TEXT, CARD);
      char label[44];
      snprintf(label, sizeof(label), "%d. %.18s", i + 1,
               step.name[0] ? step.name : "STEP");
      tft.drawString(label, 28, y);

      tft.setTextDatum(textdatum_t::top_right);
      char target[32];
      const char* unit = step.mode == MODE_FLOW ? "g/s" :
                         step.mode == MODE_DUTY ? "%" : "bar";
      snprintf(target, sizeof(target), "%.1f %s   %.1fs", step.target,
               unit, step.timeoutMs / 1000.0f);
      tft.setFont(&fonts::Font2);
      tft.setTextColor(MUTED, CARD);
      tft.drawString(target, W - 28, y + 2);
    }
  }

  tft.setTextDatum(textdatum_t::bottom_center);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(MUTED, CARD);
  tft.drawString("CLICK TO RETURN", W / 2, 298);
  lastProfile = idx;
}
}  // namespace

void uiIdleStatic() {
  if (detailView) {
    drawDetail();
    return;
  }

  drawIdleBase();
  uiInitDMA();

  const float blockC = shot.blockTempC > 0.0f ? shot.blockTempC : shot.temperature;
  const float brewC = shot.brewTempEstC > 0.0f ? shot.brewTempEstC : blockC;
  const int state = thermalState(brewC, blockC);

  lastScaleOnline = ScaleService::isOnline();
  lastWeight = ScaleService::getWeight();
  drawHeaderStatus(lastWeight, lastScaleOnline, state);
  drawTemperatureContent(brewC, blockC);
  drawScaleContent(lastWeight, lastScaleOnline);
  drawProfileContent();

  lastBrewTemp = brewC;
  lastBlockTemp = blockC;
  lastThermalState = state;
}

void uiIdleDynamic() {
  if (detailView) {
    if (lastProfile != getCurrentProfileIndex()) drawDetail();
    return;
  }

  const float blockC = shot.blockTempC > 0.0f ? shot.blockTempC : shot.temperature;
  const float brewC = shot.brewTempEstC > 0.0f ? shot.brewTempEstC : blockC;
  const int state = thermalState(brewC, blockC);
  const bool thermalChanged = state != lastThermalState ||
      fabsf(brewC - lastBrewTemp) >= 0.2f || fabsf(blockC - lastBlockTemp) >= 0.8f;

  if (thermalChanged) {
    drawTemperatureContent(brewC, blockC);
    lastBrewTemp = brewC;
    lastBlockTemp = blockC;
    lastThermalState = state;
  }

  const bool scaleOnline = ScaleService::isOnline();
  const float weight = ScaleService::getWeight();
  const bool weightChanged = scaleOnline != lastScaleOnline ||
      (scaleOnline && fabsf(weight - lastWeight) >= 0.1f);

  if (weightChanged) {
    drawScaleContent(weight, scaleOnline);
    lastScaleOnline = scaleOnline;
    lastWeight = weight;
  }

  if (thermalChanged || weightChanged) {
    drawHeaderStatus(weight, scaleOnline, state);
  }

  if (lastProfile != getCurrentProfileIndex()) {
    drawProfileContent();
  }
}

void uiIdleToggleProfileDetail() {
  detailView = !detailView;
  uiIdleStatic();
}

void uiIdleDeinit() {
  statusSpr.deleteSprite();
  tempSpr.deleteSprite();
  scaleSpr.deleteSprite();
  profileSpr.deleteSprite();
  statusSprReady = tempSprReady = scaleSprReady = profileSprReady = false;
}
