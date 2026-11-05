#pragma once
#include <stdint.h>
#include <stddef.h>
namespace HeaterSensorWire {
constexpr uint8_t VERSION = 9;
// Optional diagnostic frame; existing heater commands/status remain unchanged.
#pragma pack(push, 1)
struct Status {
  uint8_t version = VERSION;
  uint8_t issues = 0, lastIssues = 0;
  uint32_t sampledAt = 0, total = 0, errors = 0, lastErrorAt = 0;
  uint32_t frame = 0, lastErrorFrame = 0;
  uint16_t windowSamples = 0, windowErrors = 0;
  uint16_t peakWindowSamples = 0, peakWindowErrors = 0;
  uint32_t consecutiveErrors = 0, maxConsecutiveErrors = 0;
  uint32_t evaluatedWindows = 0, windowsAtOrAbove20 = 0;
  uint8_t windowReady = 0;
  uint8_t controlSamples = 0, controlErrors = 0;
  uint8_t controlCapacity = 0, controlWarnErrors = 0;
  uint8_t controlPauseErrors = 0, controlResumeErrors = 0, controlTripErrors = 0;
  uint16_t samplePeriodMs = 0, maxGoodAgeMs = 0;
  uint16_t pauseGoodAgeMs = 0;
  uint8_t paused = 0, densityPaused = 0;
  float thermalFeedforward = 0, inletFlow = 0, feedforwardKff = 0;
  uint8_t flowQualified = 0, inletValveOpen = 0;
  uint16_t pidPeriodMs = 0;
  float inletPressure = 0, inletDuty = 0;
  // v9 append-only extension; the v8 prefix is unchanged.
  uint8_t overtempPending = 0;
  float medianTemperature = 0; // NaN when the median stage is disabled.
  uint32_t overtempEvents = 0, overtempAt = 0;
  float overtempPeak = 0;
  uint16_t overtempConfirmMs = 0;
};
#pragma pack(pop)
constexpr size_t V8_SIZE = offsetof(Status, overtempPending);
static_assert(sizeof(Status) < 128, "heater diagnostic frame size");
}
