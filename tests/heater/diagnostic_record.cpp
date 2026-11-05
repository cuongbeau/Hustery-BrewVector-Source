#include "src/model/diagnostic_record.h"
using namespace DiagnosticRecord;
constexpr uint8_t hello[] = {'h','e','l','l','o'};
constexpr uint8_t damaged[] = {'h','e','l','l','p'};
static_assert(checksumBytes(hello, 0) == 2166136261U, "FNV offset basis");
static_assert(checksumBytes(hello, 5) == 1335831723U, "FNV known vector");
static_assert(checksumBytes(hello, 5) != checksumBytes(damaged, 5), "detect changed byte");
constexpr Record sequence(uint32_t n) { Record r; r.sequence = n; return r; }
static_assert(newer(sequence(1), sequence(0xFFFFFFFFU)), "journal sequence wrap");
static_assert(!newer(sequence(3), sequence(3)), "equal slots are not newer");
static_assert(!newer(sequence(2), sequence(3)), "keep newest complete slot");
static_assert(sizeof(Record) == 56 && offsetof(Record, checksum) == 52, "stable persisted layout without padding");
static_assert(sizeof(MainDiagnosticsWire::Status) < 128, "nonblocking UART frame budget");
