#pragma once

#include "calibration_types.h"
#include "brew_control_contract.h"

namespace BrewControlContract {
// Read-only conversion. No persistence writes or replacement defaults.
BREW_CONTROL_CONSTEXPR14 PressureCalibration preservePressureCalibration(const PressureCal& old) {
  PressureCalibration result;
  result.zeroVoltage = old.offset;
  result.barPerVolt = old.scale;
  result.legacyDivK = old.divK;
  return result;
}
}  // namespace BrewControlContract
