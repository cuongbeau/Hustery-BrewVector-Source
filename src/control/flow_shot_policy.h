#pragma once
#include <stdint.h>

// Main-side bookkeeping only; upstream estimator math remains in its owner.
struct FlowShotPolicy {
  uint32_t expectedTare = 0;
  bool scaleAllowed = false;

  constexpr void begin(uint32_t generation, bool scaleOnline) {
    expectedTare = generation;
    scaleAllowed = scaleOnline;
  }
  constexpr bool accepts(uint32_t generation) const {
    return expectedTare != 0 && generation == expectedTare;
  }
  constexpr bool allowScale(bool online) {
    if (!online) scaleAllowed = false;
    return scaleAllowed;
  }
};
