// Compile-only against ESP32 headers; never part of firmware runtime.
#include "interfaces/profile_types.h"
#include "interfaces/brew_calibration_adapter.h"
#include "lib/BrewProfileRuntime/src/BrewProfileRuntime.h"
#include <type_traits>

static_assert(!std::is_same<Profile, BrewProfileRuntime::Profile>::value,
              "Legacy and upstream profile types must remain isolated");
static_assert(BrewControlContract::kMachineShotLimitMs == 60000, "Preserve machine limit");

#if __cplusplus >= 201402L
namespace {
using namespace BrewControlContract;
constexpr PressureCal saved{0.75f, 4.0f, 2.0f};
constexpr auto converted = preservePressureCalibration(saved);
static_assert(converted.zeroVoltage == 0.75f && converted.barPerVolt == 4.0f &&
              converted.legacyDivK == 2.0f, "Copy all saved calibration fields");
static_assert(converted.rawBar(3.0f) == 9.0f, "Do not apply divK twice");
static_assert(converted.rawBar(0.5f) == 0.0f, "Clamp below-zero pressure");
static_assert(!PressureCalibration{0.5f, 0.0f, 1.0f}.valid(), "Reject zero gain");
static_assert(!PressureCalibration{0.5f, -1.0f, 1.0f}.valid(), "Reject negative gain");
static_assert(!PressureCalibration{NAN, 4.0f, 1.0f}.valid(), "Reject invalid offset");
static_assert(!std::isfinite(converted.rawBar(NAN)), "Invalid sample stays invalid");

constexpr ScaleSample scale{1000, 100, 42, 15.0f, true, true,
                            ScaleContinuity::SameShotBaseline};
static_assert(scale.usableForYield(1649, 42), "Combined age just below limit");
static_assert(!scale.usableForYield(1650, 42), "Combined age at limit is stale");
static_assert(!scale.usableForYield(1100, 43), "Reject previous shot sample");
static_assert(!scale.usableForYield(1100, 0), "Reject missing active shot");
constexpr bool rejectUntrustedScale() {
  auto sample = scale;
  sample.connected = false;
  if (sample.usableForYield(1100, 42)) return false;
  sample.connected = true;
  sample.continuity = ScaleContinuity::Unknown;
  if (sample.usableForYield(1100, 42)) return false;
  sample.continuity = ScaleContinuity::LostBaseline;
  if (sample.usableForYield(1100, 42)) return false;
  sample.continuity = ScaleContinuity::SameShotBaseline;
  sample.baselineEstablishedAtShotStart = false;
  if (sample.usableForYield(1100, 42)) return false;
  sample.baselineEstablishedAtShotStart = true;
  sample.netWeightG = NAN;
  if (sample.usableForYield(1100, 42)) return false;
  sample.netWeightG = 0.0f;
  sample.sourceAgeMs = UINT32_MAX;
  return !sample.usableForYield(1100, 42);
}
static_assert(rejectUntrustedScale(), "Reject disconnected, untrusted or invalid yield");
constexpr ScaleSample wrapped{UINT32_MAX - 49, 0, 42, 0.0f, true, true,
                              ScaleContinuity::SameShotBaseline};
static_assert(wrapped.usableForYield(50, 42), "Millis wrap preserves fresh sample");
static_assert(!wrapped.usableForYield(700, 42), "Millis wrap preserves stale cutoff");
static_assert(!PhaseFallback{}.valid(), "Timeout must be configured explicitly");
static_assert(PhaseFallback{60000, 1000}.valid(), "Allow machine limit");
static_assert(!PhaseFallback{60001, 1000}.valid(), "Reject beyond machine limit");
static_assert(!PhaseFallback{1000, 1001}.valid(), "Transition cannot exceed timeout");
}  // namespace
#endif

void compileContract(JsonObject json) {
  BrewProfileRuntime::Profile profile{};
  BrewProfileRuntime::parseProfile(json, profile);
  BrewProfileRuntime::writeProfile(json, profile);
  PressureCal existing;
  auto calibration = BrewControlContract::preservePressureCalibration(existing);
  (void)calibration.rawBar(2.75f);
  BrewControlContract::BrewMeasurements measurements;
  (void)measurements.scale.usableForYield(1000, 1);
}
