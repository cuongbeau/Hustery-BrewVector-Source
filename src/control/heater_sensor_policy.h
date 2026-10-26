#pragma once
#include <stdint.h>
namespace HeaterSensorPolicy {
constexpr uint32_t SAMPLE_MS = 250;
constexpr unsigned WINDOW = 20;
constexpr unsigned WARNING_ERRORS = 4;  // 20%: diagnostics only.
constexpr unsigned PAUSE_ERRORS = 14;   // 70%: SSR off, automatic recovery.
constexpr unsigned RESUME_ERRORS = 8;   // 40%: hysteresis before resuming.
constexpr unsigned MAX_ERRORS = 18;     // 90%: latch the sensor fault.
constexpr uint32_t PAUSE_GOOD_AGE_MS = 1500;
constexpr uint32_t MAX_GOOD_AGE_MS = 5000;
// Fixed-sample count window, not a wall-clock error percentage.
// Does not reset on Off/Enable/Clear; diagnostics continue after a latched fault.
struct ErrorWindow {
  uint8_t results[WINDOW]{};
  uint8_t index = 0, count = 0, errors = 0;
  constexpr void add(bool bad) {
    if (count == WINDOW) errors -= results[index];
    else ++count;
    results[index] = bad ? 1 : 0;
    errors += results[index];
    index = (index + 1) % WINDOW;
  }
  constexpr bool warning() const { return errors >= WARNING_ERRORS; }
  constexpr bool shouldPause() const { return errors >= PAUSE_ERRORS; }
  constexpr bool recovered() const { return errors <= RESUME_ERRORS; }
  constexpr bool failed() const { return errors >= MAX_ERRORS; }
};
}
