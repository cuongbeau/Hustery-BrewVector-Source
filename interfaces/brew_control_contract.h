#pragma once

// Foundation only: no GPIO, scheduling, storage or active runtime callers.
#include <cmath>
#include <cstdint>

#if __cplusplus >= 201402L
#define BREW_CONTROL_CONSTEXPR14 constexpr
#else
#define BREW_CONTROL_CONSTEXPR14 inline
#endif

namespace BrewControlContract {

constexpr uint32_t kPumpPeriodMs = 30;
constexpr uint32_t kProcessPeriodMs = 100;
constexpr uint32_t kMachineShotLimitMs = 60000;
constexpr uint32_t kScaleFreshnessMs = 750;

// Same voltage domain as ADS1115_Interface::getVoltage(). Do not multiply
// legacy divK again: the active pressure conversion does not use that field.
struct PressureCalibration {
  float zeroVoltage = 0.5f;
  float barPerVolt = 4.0f;
  float legacyDivK = 1.0f; // Preserved metadata, not applied to this conversion.

  constexpr bool valid() const {
    return std::isfinite(zeroVoltage) && std::isfinite(barPerVolt) &&
           barPerVolt > 0.0f;
  }
  BREW_CONTROL_CONSTEXPR14 float rawBar(float voltage) const {
    if (!valid() || !std::isfinite(voltage)) return NAN;
    const float pressure = (voltage - zeroVoltage) * barPerVolt;
    if (!std::isfinite(pressure)) return NAN;
    return pressure > 0.0f ? pressure : 0.0f;
  }
};

struct PressureSample {
  uint32_t sampledAtMs = 0;
  float rawBar = 0.0f;
  float controllerFilteredBar = 0.0f;
  float displayFilteredBar = 0.0f;
  bool rawValid = false;
  bool controllerFilteredValid = false;
  bool displayFilteredValid = false;
};

struct FlowSample {
  uint32_t sampledAtMs = 0;
  float pumpMlPerS = 0.0f;
  float puckMlPerS = 0.0f;
  float pumpedWaterMl = 0.0f;
  bool valid = false; // Model validity, not proof of physically measured flow.
};

enum class ScaleContinuity : uint8_t { Unknown, SameShotBaseline, LostBaseline };

struct ScaleSample {
  uint32_t receivedAtMs = 0;
  uint32_t sourceAgeMs = 0;
  uint32_t shotSessionId = 0;
  float netWeightG = 0.0f;
  bool connected = false;
  bool baselineEstablishedAtShotStart = false;
  ScaleContinuity continuity = ScaleContinuity::Unknown;

  // The future executor must latch LostBaseline/Unknown until shot end after
  // continuity loss; this predicate alone does not maintain that history.
  BREW_CONTROL_CONSTEXPR14 bool usableForYield(uint32_t nowMs, uint32_t activeShotId) const {
    const uint32_t transportAge = nowMs - receivedAtMs;
    return connected && baselineEstablishedAtShotStart && activeShotId != 0 &&
           shotSessionId == activeShotId &&
           continuity == ScaleContinuity::SameShotBaseline &&
           std::isfinite(netWeightG) && sourceAgeMs < kScaleFreshnessMs &&
           transportAge < kScaleFreshnessMs - sourceAgeMs;
  }
};

struct BrewMeasurements {
  uint32_t shotSessionId = 0;
  PressureSample pressure;
  FlowSample flow;
  ScaleSample scale;
  float modelYieldG = 0.0f; // Explicitly separate from BLE yield exits.
  bool modelYieldValid = false;
};

enum class PumpMode : uint8_t { Power, Pressure, Flow };
struct BrewActuation {
  PumpMode mode = PumpMode::Power;
  float powerPercent = 0.0f;
  float pressureBar = 0.0f;
  float flowMlPerS = 0.0f;
  bool valveOpen = false;
  float temperatureTargetC = 0.0f;
  bool temperatureTargetPresent = false; // Heater SSR deferred; not enabled.
};

// Out-of-band local policy; never pretend these fields exist upstream.
struct PhaseFallback {
  uint32_t timeoutMs = 0; // Must be explicitly configured before activation.
  uint32_t yieldTransitionFallbackMs = 0; // Unconfigured; require >0 if used.
  constexpr bool valid() const {
    return timeoutMs > 0 && timeoutMs <= kMachineShotLimitMs &&
           yieldTransitionFallbackMs <= timeoutMs;
  }
};

// Future Main-side executor interface. No implementation is wired in yet.
// Profile load/validation lives outside the 30 ms pump task. Its active copy
// must remain immutable for the shot; HMI cannot own the executor.
class BrewProcessAdapter {
 public:
  virtual ~BrewProcessAdapter() = default;
  virtual void updateMeasurements(const BrewMeasurements& measurements) = 0;
  virtual void progress(uint32_t nowMs) = 0;
  virtual BrewActuation actuation() const = 0;
  virtual bool active() const = 0;
  virtual void abort() = 0;
};

}  // namespace BrewControlContract
