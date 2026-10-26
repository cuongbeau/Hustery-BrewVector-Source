#pragma once
#include <cmath>
#include <stdint.h>

// Heater PID and thermal feedforward equations, with local thermoblock
// safety scaling and immediate water-on/off scheduling.
// Upstream provenance and license terms are recorded in THIRD_PARTY_NOTICES.md.
// Local output units are percentage points (upstream 0..1000 -> divide gains by 10).
namespace HeaterControlModel {
constexpr uint32_t PID_PERIOD_MS = 1000;
// Local derivative-on-measurement: update only on accepted sensor samples.
// 2s is a provisional noise/lag compromise, not a plant time constant.
struct TemperatureRate {
  static constexpr float FILTER_SECONDS = 2.0f;
  float previous = 0, rate = 0;
  uint32_t at = 0;
  bool initialized = false;
  constexpr void reset() { previous = rate = 0; at = 0; initialized = false; }
  constexpr void sample(float temperature, uint32_t now, bool restart = false) {
    if (!std::isfinite(temperature)) return;
    if (!initialized || restart) {
      previous = temperature; at = now; rate = 0; initialized = true; return;
    }
    const uint32_t elapsed = now - at;
    if (!elapsed) return;
    const float dt = elapsed / 1000.0f;
    const float slope = (temperature - previous) / dt;
    rate += dt / (FILTER_SECONDS + dt) * (slope - rate);
    previous = temperature; at = now;
  }
};
constexpr float bound(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
struct Pid {
  float integral = 0;
  float p = 0, i = 0, d = 0, output = 0;
  constexpr void reset() { integral = p = i = d = output = 0; }
  constexpr void step(float target, float temperature, float kp, float ki, float kd,
                      float ff, float maximum, bool recovery = false, float temperatureRate = 0) {
    const float error = target - temperature;
    // Upstream configured at 1Hz: fixed dt=1s, no delayed-loop catch-up.
    const float increment = recovery ? 0 : error;
    integral += increment;
    p = kp * error; i = ki * integral;
    d = recovery ? 0 : -kd * temperatureRate;
    float sum = p + i + d + ff;
    if ((sum < 0 || sum > maximum) &&
        ((error > 0 && sum > 0) || (error < 0 && sum < 0))) {
      integral -= increment; i = ki * integral;
      sum = p + i + d + ff;
    }
    output = bound(sum, 0, maximum);
  }
};
struct Feedforward {
  // Disabled until the inlet flow/valve mapping and thermal parameters are commissioned.
  float kff = 0, inletC = 23, efficiency = 0.95f, heatLossW = 5;
  float safety = 1, output = 0;
  constexpr void reset() { safety = 1; output = 0; }
  static constexpr float scaling(float error) {
    // Thermoblock: keep flow compensation through a small warm-body margin,
    // then withdraw it before the body reaches target + 5C.
    return error <= 2.0f ? 1.0f :
        error >= 5.0f ? 0.0f : (5.0f-error)/3.0f;
  }
  constexpr float update(float target, float temperature, float flowMlS, bool valveOpen, bool fresh) {
    output = 0;
    if (!fresh || !valveOpen || !std::isfinite(flowMlS) || flowMlS <= 0.01f ||
        !std::isfinite(kff) || kff <= 0 || !std::isfinite(inletC) ||
        !std::isfinite(efficiency) || efficiency <= 0 || efficiency > 1 ||
        !std::isfinite(heatLossW) || heatLossW < 0) return 0;
    // Use the conservative raw/filtered temperature supplied by Core. The
    // requested upper-temperature limit must not lag behind prior FF output.
    safety = scaling(temperature-target);
    if (target > inletC)
      output = ((4.18f*(target-inletC) + heatLossW/flowMlS)/efficiency)*kff*safety*flowMlS;
    if (!std::isfinite(output)) output = 0;
    return output;
  }
};
}
