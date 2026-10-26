#pragma once
#include <stdint.h>
namespace Max31855Frame {
// Bits 0..2: chip OC/SCG/SCV; 3: malformed frame; 4: application range;
// 5: bus read failed. These are observations, not a hardware diagnosis.
constexpr float celsius(uint32_t frame) {
  const int32_t raw = static_cast<int32_t>(frame >> 18);
  return ((raw & 0x2000) ? raw - 0x4000 : raw) * 0.25f;
}
constexpr uint8_t issues(uint32_t frame, bool readOk = true) {
  if (!readOk) return 32;
  const uint8_t faults = frame & 7;
  const bool malformed = (frame & 0x20008U) != 0 ||
      ((frame & 0x10000U) != 0) != (faults != 0);
  // Faulted temperature bits are not usable for range diagnostics.
  return faults | (malformed ? 8 : 0) |
      (!faults && !malformed && (celsius(frame) < -20 || celsius(frame) > 500) ? 16 : 0);
}
}
