#pragma once
#include <stdint.h>

namespace ShotSavePolicy {
constexpr unsigned RecentLimit = 10;
constexpr bool accepts(float durationS, float peakPressureBar, uint32_t samples) {
  return samples >= 2 && __builtin_isfinite(durationS) &&
      __builtin_isfinite(peakPressureBar) && durationS > 10.0f &&
      peakPressureBar >= 1.0f;
}
}
