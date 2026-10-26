#pragma once
#include <stdint.h>
#include <float.h>
#include <math.h>

namespace StopResultValues {
constexpr bool validScale(bool online, float grams) {
  return online && __builtin_isfinite(grams);
}
constexpr uint8_t flags(bool online, float grams) {
  return validScale(online, grams) ? 3U : 1U;
}
constexpr float yield(bool online, float grams) {
  return validScale(online, grams) ? grams : NAN;
}
}
