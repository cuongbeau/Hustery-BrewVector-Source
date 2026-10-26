#pragma once

#include <stdint.h>
#include <math.h>

struct ShotSummary {
  uint32_t id = 0;
  float durationS = 0.0f;
  float weightG = 0.0f;
  float modelYieldG = NAN; // Display-only estimate; never a valid physical scale yield.
  float peakPressureBar = 0.0f;
  uint16_t sampleCount = 0;
  char profileName[24] = {};
  uint32_t startedAtEpoch = 0; // Zero when wall clock was not synchronized.
  uint8_t resultFlags = 0; // Bit0: stop event received; bit1: physical scale valid at stop.
};

constexpr bool hasStopYield(const ShotSummary& shot) {
  return (shot.resultFlags & 3U) == 3U && __builtin_isfinite(shot.weightG);
}

constexpr bool hasEstimatedYield(const ShotSummary& shot) {
  return (shot.resultFlags & 1U) != 0 &&
         __builtin_isfinite(shot.modelYieldG) && shot.modelYieldG >= 0.0f;
}
