#pragma once

#include <Arduino.h>

#include "ui/ui_types.h"

class DemoData {
 public:
  void begin(uint32_t nowMs);
  ScreenId update(uint32_t nowMs, UiTelemetry& telemetry);
  void nextState(uint32_t nowMs);
  void setState(ScreenId state, uint32_t nowMs);
  ScreenId state() const { return state_; }

 private:
  ScreenId state_ = ScreenId::Ready;
  uint32_t stateStartedMs_ = 0;
};
