// Upper TFT run screen: performance telemetry & digital instrument cluster.
// Live curves and touch control belong to the lower HMI.

#include "ui_brew.h"

#include <math.h>
#include <stdio.h>

#include "interfaces/system_state.h"
#include "src/app/orchestrator.h"
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
constexpr uint16_t ORANGE_RED = TftTheme::ORANGE_RED;
constexpr uint16_t GREEN = TftTheme::GREEN;
constexpr uint16_t RED = TftTheme::RED;

constexpr int GAUGE_CX = 122;
constexpr int GAUGE_CY = 162;
constexpr int GAUGE_OUTER_R = 108;
constexpr int GAUGE_INNER_R = 92;
constexpr int GAUGE_SEGMENTS = 30;
constexpr int METRIC_X = 246;
constexpr int METRIC_W = 222;
constexpr int FOOTER_Y = 284;

// Lightweight micro-sprites to prevent any ESP32 heap allocation failures
LGFX_Sprite headerSpr(&tft);
LGFX_Sprite pValSpr(&tft);
LGFX_Sprite flowSpr(&tft);
LGFX_Sprite yieldSpr(&tft);
LGFX_Sprite tempPumpSpr(&tft);

bool headerReady = false;
bool pValReady = false;
bool flowReady = false;
bool yieldReady = false;
bool tempPumpReady = false;

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

float lastPressure = -999.0f;
float lastTarget = -999.0f;
float lastFlow = -999.0f;
float lastWeight = -999.0f;
float lastTemp = -999.0f;
float lastPump = -999.0f;
int lastSecond = -1;
int lastStep = -1;
uint32_t lastStepBucket = UINT32_MAX;
bool flushMode = false;
uint32_t lastPressureDrawMs = 0;
uint32_t lastMetricDrawMs = 0;

bool runtimeStep(StepMode& mode, int& index, const char*& name,
                 uint32_t& elapsedMs, uint32_t& durationMs) {
  if (!pfExec.running || pfExec.profileIndex < 0 ||
      pfExec.profileIndex >= profilesCount ||
      pfExec.currentStep >= profiles[pfExec.profileIndex].stepCount) {
    mode = MODE_PRESSURE;
    index = -1;
    name = "BREW";
    elapsedMs = 0;
    durationMs = 0;
    return false;
  }
  const AdvancedStep& step = profiles[pfExec.profileIndex].steps[pfExec.currentStep];
  mode = step.mode;
  index = static_cast<int>(pfExec.currentStep);
  name = step.name[0] ? step.name : "STEP";
  elapsedMs = millis() - pfExec.stepStartMs;
  durationMs = step.timeoutMs;
  return true;
}

uint16_t pressureColor(float actual, float target, StepMode mode) {
  if (mode != MODE_PRESSURE || target <= 0.1f) return CYAN;
  const float error = fabsf(actual - target);
  if (error <= 0.5f) return GREEN;
  if (error <= 1.5f) return AMBER;
  return CYAN;
}

void drawHeader() {
  StepMode mode;
  int stepIndex;
  const char* stepName;
  uint32_t elapsedMs, durationMs;
  runtimeStep(mode, stepIndex, stepName, elapsedMs, durationMs);

  if (!ensureSprite(headerSpr, headerReady, W, 42)) return;
  headerSpr.fillSprite(BG);

  char title[64];
  const int total = (pfExec.profileIndex >= 0 && pfExec.profileIndex < profilesCount)
                        ? profiles[pfExec.profileIndex].stepCount : 0;
  if (stepIndex >= 0)
    snprintf(title, sizeof(title), "STEP %d / %d  -  %.24s",
             stepIndex + 1, total, stepName);
  else
    snprintf(title, sizeof(title), "EXTRACTION IN PROGRESS");

  headerSpr.setTextDatum(textdatum_t::top_left);
  headerSpr.setFont(&fonts::FreeSansBold12pt7b);
  headerSpr.setTextColor(TEXT, BG);
  headerSpr.drawString(title, 12, 4);

  const int sec = shot.shotTime / 1000U;
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", sec / 60, sec % 60);
  headerSpr.setTextDatum(textdatum_t::top_right);
  headerSpr.setTextColor(CYAN, BG);
  headerSpr.drawString(timeBuf, W - 12, 4);

  constexpr int railX = 12;
  constexpr int railY = 33;
  const int railW = W - 24;
  constexpr int railH = 5;
  const int shownSteps = constrain(total, 1, 8);
  const int gap = 4;
  const int segmentW = (railW - gap * (shownSteps - 1)) / shownSteps;
  const float ratio = durationMs > 0
                          ? constrain(static_cast<float>(elapsedMs) / durationMs,
                                      0.0f, 1.0f)
                          : 0.0f;
  for (int i = 0; i < shownSteps; ++i) {
    const int x = railX + i * (segmentW + gap);
    headerSpr.fillRoundRect(x, railY, segmentW, railH, 2, CARD_ALT);
    if (stepIndex >= 0 && i < stepIndex) {
      headerSpr.fillRoundRect(x, railY, segmentW, railH, 2, CYAN);
    } else if (stepIndex >= 0 && i == stepIndex) {
      const int fillW = static_cast<int>(segmentW * ratio);
      if (fillW > 1)
        headerSpr.fillRoundRect(x, railY, fillW, railH, 2, CYAN);
    }
  }

  pushSpriteAtomic(headerSpr, 0, 0);
  lastStep = stepIndex;
  lastStepBucket = elapsedMs / 200U;
}
void drawPressureGauge(float pressure, float target) {
  StepMode mode;
  int stepIndex;
  const char* stepName;
  uint32_t elapsedMs, durationMs;
  runtimeStep(mode, stepIndex, stepName, elapsedMs, durationMs);
  (void)stepIndex;
  (void)stepName;
  (void)elapsedMs;
  (void)durationMs;

  const uint16_t activeColor = pressureColor(pressure, target, mode);
  const int activeSegments = constrain(
      static_cast<int>(pressure / 12.0f * GAUGE_SEGMENTS + 0.5f),
      0, GAUGE_SEGMENTS);

  tft.startWrite();
  for (int i = 0; i < GAUGE_SEGMENTS; ++i) {
    const int startAngle = 150 + i * 240 / GAUGE_SEGMENTS;
    const int endAngle = 150 + (i + 1) * 240 / GAUGE_SEGMENTS - 2;
    tft.fillArc(GAUGE_CX, GAUGE_CY, GAUGE_OUTER_R, GAUGE_INNER_R,
                startAngle, endAngle,
                i < activeSegments ? activeColor : CARD_ALT);
  }
  tft.drawCircle(GAUGE_CX, GAUGE_CY, GAUGE_OUTER_R + 3, BORDER);
  tft.drawCircle(GAUGE_CX, GAUGE_CY, GAUGE_INNER_R - 3, BORDER);

  if (mode == MODE_PRESSURE && target > 0.1f) {
    const float targetRatio = constrain(target / 12.0f, 0.0f, 1.0f);
    const float angle = (150.0f + 240.0f * targetRatio) * DEG_TO_RAD;
    const int markerX =
        GAUGE_CX + static_cast<int>(cosf(angle) * (GAUGE_OUTER_R - 7));
    const int markerY =
        GAUGE_CY + static_cast<int>(sinf(angle) * (GAUGE_OUTER_R - 7));
    tft.fillCircle(markerX, markerY, 7, AMBER);
    tft.fillCircle(markerX, markerY, 3, BG);
  }
  tft.endWrite();

  if (ensureSprite(pValSpr, pValReady, 136, 108)) {
    pValSpr.fillSprite(BG);
    char value[16];
    snprintf(value, sizeof(value), "%.1f", pressure);
    pValSpr.setTextDatum(textdatum_t::top_center);
    pValSpr.setFont(&fonts::Font8);
    pValSpr.setTextColor(CYAN, BG);
    pValSpr.drawString(value, 68, -4);
    pValSpr.setFont(&fonts::FreeSansBold12pt7b);
    pValSpr.setTextColor(CYAN, BG);
    pValSpr.drawString("bar", 68, 76);
    pushSpriteAtomic(pValSpr, GAUGE_CX - 68, GAUGE_CY - 54);
  }

  const int badgeX = GAUGE_CX - 52;
  const int badgeY = 226;
  tft.fillRoundRect(badgeX, badgeY, 104, 36, 6, BG);
  tft.drawRoundRect(badgeX, badgeY, 104, 36, 6,
                    target > 0.1f ? AMBER : BORDER);
  tft.setTextDatum(textdatum_t::top_center);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(target > 0.1f ? AMBER : MUTED, BG);
  char targetText[28];
  if (mode == MODE_FLOW)
    snprintf(targetText, sizeof(targetText), "TARGET %.1f g/s", target);
  else if (mode == MODE_DUTY)
    snprintf(targetText, sizeof(targetText), "TARGET %.0f%%", target);
  else if (target > 0.1f)
    snprintf(targetText, sizeof(targetText), "TARGET %.1f", target);
  else
    snprintf(targetText, sizeof(targetText), "MANUAL");
  tft.drawString(targetText, GAUGE_CX, badgeY + 9);

  lastPressure = pressure;
  lastTarget = target;
}
void drawFlowValue(float flow) {
  if (!ensureSprite(flowSpr, flowReady, METRIC_W - 12, 42)) return;
  flowSpr.fillSprite(CARD);
  char value[24];
  snprintf(value, sizeof(value), "%.1f g/s", flow);
  flowSpr.setTextDatum(textdatum_t::middle_right);
  flowSpr.setFont(&fonts::FreeSansBold18pt7b);
  flowSpr.setTextColor(AMBER, CARD);
  flowSpr.drawString(value, METRIC_W - 22, 21);
  pushSpriteAtomic(flowSpr, METRIC_X + 6, 70);
  lastFlow = flow;
}
void drawYieldValue(float weight) {
  if (!ensureSprite(yieldSpr, yieldReady, METRIC_W - 12, 42)) return;
  yieldSpr.fillSprite(CARD);
  float targetWeight = 36.0f;
  if (pfExec.profileIndex >= 0 && pfExec.profileIndex < profilesCount &&
      profiles[pfExec.profileIndex].totalTargetWeight > 0.1f) {
    targetWeight = profiles[pfExec.profileIndex].totalTargetWeight;
  }
  char value[32];
  snprintf(value, sizeof(value), "%.1f / %.0f g", weight, targetWeight);
  yieldSpr.setTextDatum(textdatum_t::middle_right);
  yieldSpr.setFont(&fonts::FreeSansBold12pt7b);
  yieldSpr.setTextColor(weight >= targetWeight - 0.2f ? GREEN : CYAN, CARD);
  yieldSpr.drawString(value, METRIC_W - 22, 21);
  pushSpriteAtomic(yieldSpr, METRIC_X + 6, 146);
  lastWeight = weight;
}
void drawTempPumpValue(float temp, float pump) {
  if (!ensureSprite(tempPumpSpr, tempPumpReady, METRIC_W - 12, 48)) return;
  tempPumpSpr.fillSprite(CARD);
  char tempValue[16];
  snprintf(tempValue, sizeof(tempValue), "%.1f C", temp);
  tempPumpSpr.setTextDatum(textdatum_t::middle_center);
  tempPumpSpr.setFont(&fonts::FreeSansBold12pt7b);
  tempPumpSpr.setTextColor(ORANGE_RED, CARD);
  tempPumpSpr.drawString(tempValue, 52, 26);

  char pumpValue[16];
  snprintf(pumpValue, sizeof(pumpValue), "%.0f%%", pump);
  tempPumpSpr.setTextColor(CYAN, CARD);
  tempPumpSpr.drawString(pumpValue, 158, 26);
  pushSpriteAtomic(tempPumpSpr, METRIC_X + 6, 218);
  lastTemp = temp;
  lastPump = pump;
}
void drawStaticCards() {
  tft.startWrite();
  for (int i = 0; i < GAUGE_SEGMENTS; ++i) {
    const int startAngle = 150 + i * 240 / GAUGE_SEGMENTS;
    const int endAngle = 150 + (i + 1) * 240 / GAUGE_SEGMENTS - 2;
    tft.fillArc(GAUGE_CX, GAUGE_CY, GAUGE_OUTER_R, GAUGE_INNER_R,
                startAngle, endAngle, CARD_ALT);
  }
  tft.fillCircle(GAUGE_CX, GAUGE_CY, GAUGE_INNER_R - 4, BG);
  tft.drawCircle(GAUGE_CX, GAUGE_CY, GAUGE_OUTER_R + 3, BORDER);
  tft.drawCircle(GAUGE_CX, GAUGE_CY, GAUGE_INNER_R - 3, BORDER);
  tft.endWrite();

  const int cardY[] = {46, 122, 198};
  const int cardH[] = {68, 68, 76};
  for (int i = 0; i < 3; ++i) {
    tft.fillRoundRect(METRIC_X, cardY[i], METRIC_W, cardH[i], 7, CARD);
    tft.drawRoundRect(METRIC_X, cardY[i], METRIC_W, cardH[i], 7, BORDER);
  }

  tft.setTextDatum(textdatum_t::top_left);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(MUTED, CARD);
  tft.drawString("FLOW", METRIC_X + 12, 54);
  tft.drawString("YIELD", METRIC_X + 12, 130);
  tft.drawFastVLine(METRIC_X + METRIC_W / 2, 207, 58, BORDER);
  tft.drawString("TEMP", METRIC_X + 12, 206);
  tft.drawString("PUMP", METRIC_X + METRIC_W / 2 + 12, 206);

  tft.fillRoundRect(12, FOOTER_Y, W - 24, 28, 6, BG);
  tft.drawRoundRect(12, FOOTER_Y, W - 24, 28, 6, BORDER);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setFont(&fonts::FreeSansBold9pt7b);
  tft.setTextColor(TEXT, BG);
  tft.drawString("HOLD KNOB TO STOP", W / 2, FOOTER_Y + 14);
}
void drawFlushBase() {
  tft.fillScreen(BG);
  tft.setTextDatum(textdatum_t::top_center);
  tft.setFont(&fonts::FreeSansBold18pt7b);
  tft.setTextColor(CYAN, BG);
  tft.drawString("FLUSHING", W / 2, 24);
  tft.drawFastHLine(16, 70, W - 32, BORDER);
  drawStaticCards();
}

void drawFlush(const ShotData& shotData) {
  const uint32_t elapsedMs = Orchestrator::flushElapsedMs();
  const uint32_t durationMs = Orchestrator::flushDurationMs();
  const float remaining = max(0.0f, (durationMs - min(elapsedMs, durationMs)) / 1000.0f);

  if (ensureSprite(pValSpr, pValReady, 200, 68)) {
    pValSpr.fillSprite(BG);
    char timeText[20];
    snprintf(timeText, sizeof(timeText), "%.1f s", remaining);
    pValSpr.setTextDatum(textdatum_t::top_center);
    pValSpr.setFont(&fonts::FreeSansBold24pt7b);
    pValSpr.setTextColor(TEXT, BG);
    pValSpr.drawString(timeText, 100, 10);
    pushSpriteAtomic(pValSpr, 24, 130);
  }

  const float temp = shotData.brewTempEstC > 0.0f ? shotData.brewTempEstC : shotData.temperature;
  drawFlowValue(shotData.visualFlow);
  drawYieldValue(shotData.outGrams);
  drawTempPumpValue(temp, shotData.pumpPower);

  lastSecond = static_cast<int>(elapsedMs / 100U);
  lastTemp = temp;
  lastPump = shotData.pumpPower;
}
}  // namespace

void uiRunStart() {
  lastPressure = lastTarget = lastFlow = lastWeight = -999.0f;
  lastTemp = lastPump = -999.0f;
  lastSecond = lastStep = -1;
  lastStepBucket = UINT32_MAX;
  flushMode = Orchestrator::getState() == FLUSH_RUN;

  tft.fillScreen(BG);
  uiInitDMA();

  if (flushMode) {
    drawFlushBase();
  } else {
    drawHeader();
    drawStaticCards();

    const float temp = shot.brewTempEstC > 0.0f ? shot.brewTempEstC : shot.temperature;
    drawPressureGauge(shot.visualPressure, shot.targetPressure);
    drawFlowValue(shot.visualFlow);
    drawYieldValue(shot.outGrams);
    drawTempPumpValue(temp, shot.pumpPower);
  }
}

void uiRunUpdate(const ShotData& shotData) {
  if (flushMode) {
    const int tenth = static_cast<int>(Orchestrator::flushElapsedMs() / 100U);
    const float temp = shotData.brewTempEstC > 0.0f ? shotData.brewTempEstC : shotData.temperature;
    if (tenth != lastSecond || fabsf(temp - lastTemp) >= 0.2f ||
        fabsf(shotData.pumpPower - lastPump) >= 1.0f) {
      drawFlush(shotData);
    }
    return;
  }

  StepMode mode;
  int stepIndex;
  const char* stepName;
  uint32_t elapsedMs, durationMs;
  runtimeStep(mode, stepIndex, stepName, elapsedMs, durationMs);
  const uint32_t bucket = elapsedMs / 200U;
  const int seconds = shotData.shotTime / 1000U;

  if (stepIndex != lastStep || bucket != lastStepBucket || seconds != lastSecond) {
    drawHeader();
    lastSecond = seconds;
  }

  const uint32_t nowMs = millis();
  // 16 Hz is visually fluid on this panel and leaves more SPI/CPU headroom
  // for the Main controller than repainting the segmented arc at 25 Hz.
  if (nowMs - lastPressureDrawMs >= 60U &&
      (fabsf(shotData.visualPressure - lastPressure) >= 0.05f ||
       fabsf(shotData.targetPressure - lastTarget) >= 0.05f)) {
    drawPressureGauge(shotData.visualPressure, shotData.targetPressure);
    lastPressureDrawMs = nowMs;
  }

  const float temp = shotData.brewTempEstC > 0.0f ? shotData.brewTempEstC : shotData.temperature;
  if (nowMs - lastMetricDrawMs >= 60U) {
    if (fabsf(shotData.visualFlow - lastFlow) >= 0.06f) {
      drawFlowValue(shotData.visualFlow);
    }
    if (fabsf(shotData.outGrams - lastWeight) >= 0.1f) {
      drawYieldValue(shotData.outGrams);
    }
    if (fabsf(temp - lastTemp) >= 0.2f || fabsf(shotData.pumpPower - lastPump) >= 1.0f) {
      drawTempPumpValue(temp, shotData.pumpPower);
    }
    lastMetricDrawMs = nowMs;
  }
}

void uiRunDeinit() {
  headerSpr.deleteSprite();
  pValSpr.deleteSprite();
  flowSpr.deleteSprite();
  yieldSpr.deleteSprite();
  tempPumpSpr.deleteSprite();
  headerReady = pValReady = flowReady = yieldReady = tempPumpReady = false;
}
