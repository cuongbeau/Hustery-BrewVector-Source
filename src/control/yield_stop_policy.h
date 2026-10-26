#pragma once
#include <math.h>

namespace YieldStopPolicy {
// Applied quick yield is a whole-shot limit, independent of profile/phase.
// NAN retains legacy behavior; zero explicitly disables the yield stop.
constexpr float threshold(bool brew, bool legacyMode, float quick, float legacy) {
  if (brew && __builtin_isfinite(quick)) return quick > 0 ? quick : NAN;
  return legacyMode && legacy > 0 ? (legacy > 2 ? legacy - 2 : 0) : NAN;
}
constexpr bool reached(float thresholdG, float eligibleWeightG, bool preparing) {
  return !preparing && __builtin_isfinite(thresholdG) &&
      __builtin_isfinite(eligibleWeightG) && eligibleWeightG >= thresholdG;
}
}
