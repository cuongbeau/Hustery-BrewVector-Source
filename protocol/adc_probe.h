#pragma once
#include <stdint.h>
namespace AdcProbeWire {
constexpr uint8_t VERSION = 1;
constexpr uint8_t SAMPLES = 24;
enum class Op : uint8_t { Status, Start, Cancel };
enum class State : uint8_t { Empty, Pending, Running, Restoring, Complete, Cancelled, Failed, RestoreFailed };
enum class Error : uint8_t { None, Cancel, MachineActive, Timeout, Bus, Config, PumpAck };
enum class Result : uint8_t { Ok, Busy, Invalid, WrongRun, Unavailable };
#pragma pack(push,1)
struct Request { uint32_t requestId; uint32_t runId; Op op; uint8_t version; };
struct Sample {
  uint32_t readyUs; // elapsed from BEFORE config write to ready response
  uint32_t lastBusyUs; // elapsed to START of last poll reporting busy; 0 if none
  int16_t raw;
  uint8_t rateCode;
  uint8_t lowNibble;
};
struct Reply {
  uint32_t requestId;
  uint32_t runId;
  uint32_t startedMs;
  uint32_t maxPollGapUs;
  uint16_t savedConfig;
  uint16_t restoredConfig;
  State state;
  Error error;
  Result result;
  uint8_t count;
  uint8_t version;
  uint8_t pumpHeld;
  Sample samples[SAMPLES];
};
#pragma pack(pop)
static_assert(sizeof(Reply) <= 512, "Probe UART size");
}
