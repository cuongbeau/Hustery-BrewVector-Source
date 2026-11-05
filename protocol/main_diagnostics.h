#pragma once
#include <stdint.h>
namespace MainDiagnosticsWire {
constexpr uint8_t VERSION = 1;
#pragma pack(push, 1)
struct FaultEvent {
  uint32_t bootId = 0, atMs = 0, sampleAgeMs = 0, sensorFrame = 0;
  float rawC = 0, filteredC = 0;
  uint8_t fault = 0, sensorIssues = 0, windowErrors = 0, windowSamples = 0;
  uint8_t waterRunning = 0, reserved[3] = {};
};
struct Status {
  uint8_t version = VERSION, resetReason = 0, adcFast = 0, adcSingleShot = 0;
  uint32_t bootId = 0, bootCount = 0, uptimeMs = 0;
  uint32_t freeHeap = 0, minFreeHeap = 0, largestFreeBlock = 0;
  uint32_t faultCount = 0;
  FaultEvent lastFault{};
  uint8_t faultSaved = 0, adcMeanSamples = 0;
  uint16_t adcPollMs = 0;
  char build[24] = {};
};
#pragma pack(pop)
static_assert(sizeof(Status) <= 128, "Main diagnostics UART budget");
}
