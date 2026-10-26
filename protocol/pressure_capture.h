#pragma once
#include <stdint.h>

// Optional diagnostic extension. Existing schema-14 telemetry is unchanged.
namespace PressureCaptureWire {
constexpr uint8_t VERSION = 1;
constexpr uint16_t CAPACITY = 5120;
constexpr uint16_t POLL_MS = 3;
constexpr uint16_t PUBLISH_MS = 30;
constexpr uint16_t DURATION_MS = 15000;
constexpr uint8_t MEAN_WINDOW = 10;
constexpr uint8_t CHUNK_ROWS = 20;
enum class Op : uint8_t { Status, Arm, Read, Cancel, Clear };
enum class Status : uint8_t { Empty, Armed, Recording, Complete, Cancelled, Expired };
enum class Result : uint8_t { Ok, Busy, Invalid, NoMemory, Unsupported, WrongCapture };
enum class End : uint8_t { None, Duration, ShotStopped, Capacity, Cancel, StateChanged, CalibrationChanged };
#pragma pack(push, 1)
struct Request {
  uint32_t requestId;
  uint32_t captureId;
  uint16_t offset;
  Op op;
  uint8_t version;
};
struct Sample {
  uint32_t readUs; // Relative to capture start; NOT an ADC conversion timestamp.
  int16_t raw;
  uint8_t meanSamples;
  uint8_t flags; // bit 0: this raw sample was published to the 30 ms sensor path.
  float meanRaw; // Diagnostic only, signed counts before calibration/clamping.
  float controllerFilteredBar; // Asynchronous read-only owner snapshot.
  float pumpPercent; // Requested/applied PSM percentage, not measured AC power.
};
struct Reply {
  uint32_t requestId;
  uint32_t captureId;
  uint32_t startedAtMs;
  uint32_t busyPolls;
  float pressureOffset;
  float pressureScale;
  uint16_t total;
  uint16_t offset;
  uint8_t count;
  uint8_t fastActive; // Remains set after duration/cancel until brew ends.
  Op op;
  Status status;
  Result result;
  End end;
  uint8_t version;
  uint8_t reserved;
  Sample samples[CHUNK_ROWS];
};
#pragma pack(pop)
static_assert(sizeof(Sample) == 20, "Capture RAM budget");
static_assert(sizeof(Reply) <= 512, "Capture reply must fit UART frame");
}
