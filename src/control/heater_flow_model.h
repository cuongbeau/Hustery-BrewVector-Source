#pragma once
#include <cmath>
namespace HeaterFlowModel {
// User's former outlet calibration, trial estimate only. Not an OPV model.
// duty uses applied command (50 CPS at 100%), not a flowmeter measurement.
constexpr float estimate(float dutyPercent, float pressureBar) {
  if (!std::isfinite(dutyPercent) || !std::isfinite(pressureBar) ||
      dutyPercent <= 0 || dutyPercent > 100 || pressureBar < 0 || pressureBar >= 9.8f) return 0;
  const float cps = dutyPercent * 0.5f;
  const float baseCps = cps < 1 ? 1 : cps;
  const float base = 0.156f + (50-baseCps)*0.0016f;
  const float ratio = pressureBar / 9.8f;
  const float efficiency = pressureBar > 0.1f ? 1-ratio*ratio : 1;
  return cps*base*(efficiency < 0.01f ? 0.01f : efficiency);
}
}
