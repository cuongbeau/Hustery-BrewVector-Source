#pragma once
#include <stdint.h>
#include <stddef.h>

// Identical header is installed in the independent SmartScale project.
// FF51 notifies this packet; FF53 is a read-only capability byte (=2).
namespace HusteryScaleV2 {
constexpr uint8_t Version = 2;
enum class TareState : uint8_t { Idle, Busy, Done, Failed };
enum Flags : uint8_t { Valid = 1, Stable = 2 };
struct __attribute__((packed)) Sample {
  uint8_t magic = 0xA6;
  uint8_t version = Version;
  uint32_t bootId = 0;
  uint32_t transmittedAtMs = 0;
  uint32_t sampleSequence = 0; // ADC conversions, never notification count.
  uint16_t sampleAgeMs = UINT16_MAX;
  uint32_t tareGeneration = 0; // Includes local tare/zero changes.
  uint32_t tareToken = 0;
  float weightG = 0;
  TareState tareState = TareState::Idle;
  uint8_t flags = 0;
  uint8_t battery = 0xFF;
  uint8_t checksum = 0;
};
struct __attribute__((packed)) TareRequest {
  uint8_t command = 2;
  uint32_t token = 0; // Same token is idempotent, including after completion.
  uint32_t bootId = 0;
  uint32_t deadlineMs = 0; // Scale uptime: bounds accepting AND committing tare.
};
inline uint8_t checksum(const Sample& packet) {
  auto bytes = reinterpret_cast<const uint8_t*>(&packet);
  uint8_t result = 0;
  for (size_t i = 0; i < sizeof(Sample) - 1; ++i) result ^= bytes[i];
  return result;
}
static_assert(sizeof(Sample) == 32, "BLE v2 sample wire size");
static_assert(sizeof(TareRequest) == 13, "BLE v2 command wire size");
}
