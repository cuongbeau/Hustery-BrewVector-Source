#pragma once
#include "protocol/main_diagnostics.h"
#include <stddef.h>
namespace DiagnosticRecord {
constexpr uint32_t MAGIC = 0x42564431;
struct Record {
  uint32_t magic = MAGIC, sequence = 0, bootCount = 0, bootId = 0, faultCount = 0;
  MainDiagnosticsWire::FaultEvent lastFault{};
  uint32_t checksum = 0;
};
constexpr uint32_t checksumBytes(const uint8_t* bytes, size_t size) {
  uint32_t hash = 2166136261U;
  for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 16777619U;
  return hash;
}
inline uint32_t checksum(const Record& record) {
  return checksumBytes(reinterpret_cast<const uint8_t*>(&record), offsetof(Record, checksum));
}
inline bool valid(const Record& record) { return record.magic == MAGIC && record.checksum == checksum(record); }
inline void seal(Record& record) { record.checksum = checksum(record); }
constexpr bool newer(const Record& a, const Record& b) { return static_cast<int32_t>(a.sequence - b.sequence) > 0; }
}
