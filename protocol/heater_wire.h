#pragma once
#include <stdint.h>
namespace HeaterWire {
constexpr uint8_t VERSION = 4;
enum class Op : uint8_t { Off, Configure, Enable, Test, ClearFault };
enum class Mode : uint8_t { Off, Pid, Test, Fault };
enum class Fault : uint8_t { None, Sensor, Stale, Overtemp, MainStalled };
enum class Result : uint8_t { Ok, Invalid, Busy, Unsafe };
#pragma pack(push, 1)
struct Config {
  float target = 115, kp = 2, ki = 0.05f, kd = 0;
  float maxPower = 30, ceiling = 0; // Zero requires explicit commissioning.
  uint16_t idleMinutes = 30;
  uint16_t sleepDelaySeconds = 0, stableSeconds = 10;
  uint8_t autoSleep = 1;
  float heaterW = 1300, inletC = 25, ffScale = 0.25f;
  uint8_t ffEnabled = 0;
  float waterMaxPower = 30; // Explicit commissioning; migration retains old cap.
};
struct Request {
  uint8_t version = VERSION;
  Op op = Op::Off;
  uint32_t ticket = 0;
  Config config{};
  float testPower = 10;
  uint16_t testSeconds = 10;
};
struct Status {
  uint8_t version = VERSION;
  Mode mode = Mode::Off;
  Fault fault = Fault::None;
  Result result = Result::Ok;
  uint32_t ticket = 0, uptime = 0, sampleAge = UINT32_MAX, idleRemaining = 0;
  Config config{};
  float raw = 0, filtered = 0, power = 0, p = 0, i = 0, d = 0;
  uint8_t valid = 0, relay = 0, idleExpired = 0, ready = 0;
  float activeMaxPower = 30;
};
#pragma pack(pop)
static_assert(sizeof(Status) < 128, "heater frame size");
}
