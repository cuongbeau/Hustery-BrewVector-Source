#include "src/hardware/temperature_error_stats.h"

constexpr bool steady(unsigned errorsPer20) {
  TemperatureErrorStats stats;
  for (unsigned n = 0; n <= 100; ++n) stats.add(n % 20 < errorsPer20, n * 250);
  const auto s = stats.snapshot(25000);
  return s.windowReady && s.windowSamples == 20 && s.windowErrors == errorsPer20 &&
      s.evaluatedWindows == 81 && s.peakWindowErrors == errorsPer20 &&
      s.peakWindowSamples == 20 &&
      s.windowsAtOrAbove20 == (errorsPer20 >= 4 ? 81U : 0U);
}
static_assert(steady(0) && steady(3) && steady(4));

constexpr bool burstHistory() {
  TemperatureErrorStats stats;
  for (unsigned n = 0; n < 50; ++n) stats.add(n >= 25 && n < 35, n * 250);
  const auto burst = stats.snapshot(12250);
  for (unsigned n = 50; n <= 75; ++n) stats.add(false, n * 250);
  const auto clean = stats.snapshot(18750);
  return burst.maxConsecutiveErrors == 10 && burst.consecutiveErrors == 0 &&
      clean.windowErrors == 0 && clean.peakWindowErrors == 10 &&
      clean.peakWindowSamples == 20 && clean.windowsAtOrAbove20 > 0;
}
static_assert(burstHistory());

constexpr bool boundaryAndGap() {
  TemperatureErrorStats stats;
  stats.add(true, 0);
  for (unsigned n = 1; n < 20; ++n) stats.add(false, n * 250);
  auto partial = stats.snapshot(4999);
  stats.add(false, 5000);
  auto full = stats.snapshot(5000);
  auto stale = stats.snapshot(10001);
  stats.add(true, 11000);
  auto restart = stats.snapshot(11000);
  return !partial.windowReady && partial.windowErrors == 1 &&
      full.windowReady && full.windowSamples == 20 && full.windowErrors == 0 &&
      !stale.windowReady && stale.windowSamples == 0 && !restart.windowReady &&
      restart.windowSamples == 1 && restart.consecutiveErrors == 1;
}
static_assert(boundaryAndGap());

constexpr bool delayedAndWrap() {
  TemperatureErrorStats stats;
  const uint32_t start = UINT32_MAX - 2500;
  for (unsigned n = 0; n <= 20; ++n) stats.add(n % 5 == 0, start + n * 250);
  const auto s = stats.snapshot(start + 5000);
  return s.windowReady && s.windowSamples == 20 && s.windowErrors == 4 &&
      s.windowsAtOrAbove20 == 1; // Count actual samples, not a fixed percentage denominator.
}
static_assert(delayedAndWrap());
