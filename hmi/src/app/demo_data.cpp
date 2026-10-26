#include "app/demo_data.h"

#include <math.h>

namespace {
constexpr uint32_t READY_MS = 8000;
constexpr uint32_t FLUSH_MS = 5000;
constexpr uint32_t BREW_MS = 30000;
constexpr uint32_t RESULT_MS = 10000;
}

void DemoData::begin(uint32_t nowMs) {
  state_ = ScreenId::Ready;
  stateStartedMs_ = nowMs;
}

ScreenId DemoData::update(uint32_t nowMs, UiTelemetry& t) {
  t.sourceTimestampMs = nowMs;
  const uint32_t ageMs = nowMs - stateStartedMs_;
  const uint32_t limitMs = state_ == ScreenId::Ready
                               ? READY_MS
                               : state_ == ScreenId::Flush
                                     ? FLUSH_MS
                                     : (state_ == ScreenId::Brew ? BREW_MS
                                                                  : RESULT_MS);
  if (ageMs >= limitMs) {
    nextState(nowMs);
  }

  const float ageS = (nowMs - stateStartedMs_) * 0.001f;
  t.mainConnected = true;
  t.scaleConnected = true;
  t.scaleStable = state_ != ScreenId::Brew || ageS > 2.0f;
  t.profileName = "LEVER 9 BAR";
  t.boilerTempC = 93.0f + 0.15f * sinf(nowMs * 0.0012f);

  if (state_ == ScreenId::Ready) {
    t.pressureBar = 0.0f;
    t.flowGps = 0.0f;
    t.weightG = 0.0f;
    t.elapsedS = 0.0f;
    t.stepIndex = 0;
  } else if (state_ == ScreenId::Flush) {
    t.pressureBar = 0.0f;
    t.flowGps = 5.2f;
    t.weightG = ageS * 5.2f;
    t.elapsedS = ageS;
    t.flushRemainingS = fmaxf(0.0f, 5.0f - ageS);
    t.stepIndex = 0;
  } else if (state_ == ScreenId::Brew) {
    t.elapsedS = ageS;
    t.pressureBar = ageS < 5.0f ? ageS * 1.8f : 9.0f - 0.055f * (ageS - 5.0f);
    t.pressureBar += 0.14f * sinf(ageS * 2.4f);
    t.flowGps = ageS < 4.0f ? 0.55f * ageS : 2.35f - 0.018f * (ageS - 4.0f);
    t.flowGps += 0.05f * sinf(ageS * 3.1f);
    t.weightG = ageS < 3.0f ? 0.0f : (ageS - 3.0f) * 1.55f;
    t.stepIndex = ageS < 4.0f ? 0 : ageS < 10.0f ? 1 : ageS < 22.0f ? 2 : 3;
    t.phaseName = t.stepIndex == 0 ? "PREINFUSION"
                  : t.stepIndex == 1 ? "RAMP"
                  : t.stepIndex == 2 ? "HOLD" : "DECLINE";
    t.targetPressureBar = t.stepIndex < 2 ? 6.0f : t.stepIndex == 2 ? 9.0f : 7.5f;
    t.pressureTargetValid = true;
    t.targetFlowGps = t.stepIndex < 2 ? 2.2f : t.stepIndex == 2 ? 5.8f : 4.2f;
  } else {
    t.elapsedS = 29.4f;
    t.pressureBar = 8.7f;
    t.flowGps = 2.1f;
    t.weightG = 40.2f;
    t.stepIndex = 4;
  }

  return state_;
}

void DemoData::nextState(uint32_t nowMs) {
  if (state_ == ScreenId::Ready) {
    state_ = ScreenId::Flush;
  } else if (state_ == ScreenId::Flush) {
    state_ = ScreenId::Brew;
  } else if (state_ == ScreenId::Brew) {
    state_ = ScreenId::ShotResult;
  } else {
    state_ = ScreenId::Ready;
  }
  stateStartedMs_ = nowMs;
}

void DemoData::setState(ScreenId state, uint32_t nowMs) {
  state_ = state;
  stateStartedMs_ = nowMs;
}
