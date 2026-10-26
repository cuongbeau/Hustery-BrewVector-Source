// Compile against the actual independent scale header using -I<scale project>.
#include "tare_accumulator.h"
#include "protocol/hustery_scale_v2.h"

constexpr bool successfulTare() {
  TareAccumulator t;
  if (!t.begin(100, 800, 3, 10)) return false;
  if (t.add(1000, 200) || t.add(1006, 300)) return false;
  return t.add(1003, 400) && t.offset == 1003 &&
      t.state == TareAccumulator::State::Done && !t.add(9000, 500);
}
constexpr bool deadlineCannotCommit() {
  TareAccumulator t;
  t.offset = 77;
  t.begin(100, 800, 3, 10);
  t.add(1000, 200); t.add(1000, 300);
  return !t.add(1000, 800) && !t.add(1000, 900) && t.offset == 77 &&
      t.state == TareAccumulator::State::Failed;
}
constexpr bool missingAdcRecoversOnlyOnNewRequest() {
  TareAccumulator t;
  t.begin(100, 800, 3, 10); t.poll(900);
  if (t.add(1000, 901)) return false;
  if (!t.begin(1000, 1700, 1, 10)) return false;
  return t.add(50, 1100) && t.offset == 50;
}
constexpr bool movingCupDoesNotChangeZero() {
  TareAccumulator t;
  t.offset = 77; t.begin(100, 800, 3, 10);
  t.add(1000, 200); t.add(1020, 300);
  return !t.add(1030, 400) && t.offset == 77 &&
      t.state == TareAccumulator::State::Failed;
}
constexpr bool busyRequestDoesNotRestartDeadline() {
  TareAccumulator t;
  t.begin(100, 800, 3, 10); t.add(10, 200);
  if (t.begin(300, 1000, 1, 10)) return false;
  t.add(10, 700);
  return !t.add(10, 800) && t.state == TareAccumulator::State::Failed;
}
constexpr bool millisWrap() {
  TareAccumulator t;
  if (!t.begin(0xFFFFFF00U, 0x00000100U, 2, 10)) return false;
  t.add(-100, 0xFFFFFF80U);
  return t.add(-102, 0x00000080U) && t.offset == -101;
}
constexpr bool invalidLeaseRejected() {
  TareAccumulator t;
  return !t.begin(100, 100, 3, 10) && !t.begin(100, 99, 3, 10) &&
      !t.begin(100, 2101, 3, 10) && !t.begin(100, 800, 0, 10);
}
static_assert(successfulTare(), "One offset commit after all valid conversions");
static_assert(deadlineCannotCommit(), "No late tare after pump fallback");
static_assert(missingAdcRecoversOnlyOnNewRequest(), "ADC recovery cannot resurrect expired tare");
static_assert(movingCupDoesNotChangeZero(), "Moving water/cup must preserve previous zero");
static_assert(busyRequestDoesNotRestartDeadline(), "Busy request must not extend physical deadline");
static_assert(millisWrap(), "Tare deadline survives millis rollover");
static_assert(invalidLeaseRejected(), "Reject stale/unbounded/empty tare");
