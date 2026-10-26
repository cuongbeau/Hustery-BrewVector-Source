#pragma once
#include "protocol/heater_sensor_wire.h"
#include "src/control/heater_sensor_policy.h"

// Read-only diagnostic policy, independent of heater state and fault handling.
// No allocation. SensorReader invokes add at most once per SAMPLE_MS.
class TemperatureErrorStats {
 public:
  static constexpr uint32_t SAMPLE_MS = HeaterSensorPolicy::SAMPLE_MS, WINDOW_MS = 5000, GAP_MS = 1000;
  static constexpr unsigned CAPACITY = WINDOW_MS / SAMPLE_MS + 1;

  constexpr void add(bool error, uint32_t now) {
    if (!seen || now - lastAt > GAP_MS) {
      startAt = now;
      count = head = 0;
      consecutive = 0;
    }
    seen = true;
    lastAt = now;
    // Exclude the sample exactly 5 seconds old: window is (now-5000, now].
    while (count && now - entries[head].at >= WINDOW_MS) pop();
    if (count == CAPACITY) pop();
    entries[(head + count) % CAPACITY] = {now, error};
    ++count;
    consecutive = error ? consecutive + 1 : 0;
    if (consecutive > longest) longest = consecutive;
    auto s = snapshot(now);
    if (s.windowReady) {
      ++evaluated;
      if (s.windowErrors * 5U >= s.windowSamples) ++above20;
      if (!peakSamples || s.windowErrors * peakSamples > peakErrors * s.windowSamples) {
        peakErrors = s.windowErrors;
        peakSamples = s.windowSamples;
      }
    }
  }

  constexpr HeaterSensorWire::Status snapshot(uint32_t now) const {
    HeaterSensorWire::Status s;
    for (unsigned i = 0; i < count; ++i) {
      const auto& e = entries[(head + i) % CAPACITY];
      if (now - e.at < WINDOW_MS) {
        ++s.windowSamples;
        if (e.error) ++s.windowErrors;
      }
    }
    // A >1s acquisition gap starts a new observation period, even if Web is open.
    s.windowReady = seen && now - lastAt <= GAP_MS && now - startAt >= WINDOW_MS;
    s.consecutiveErrors = consecutive;
    s.maxConsecutiveErrors = longest;
    s.peakWindowSamples = peakSamples;
    s.peakWindowErrors = peakErrors;
    s.evaluatedWindows = evaluated;
    s.windowsAtOrAbove20 = above20;
    return s;
  }

 private:
  struct Entry { uint32_t at = 0; bool error = false; };
  Entry entries[CAPACITY]{};
  unsigned head = 0, count = 0;
  bool seen = false;
  uint32_t startAt = 0, lastAt = 0, consecutive = 0, longest = 0;
  uint32_t evaluated = 0, above20 = 0;
  uint16_t peakSamples = 0, peakErrors = 0;
  constexpr void pop() { head = (head + 1) % CAPACITY; --count; }
};
