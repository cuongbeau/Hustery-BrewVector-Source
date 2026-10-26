#include "src/hardware/pressure_capture_policy.h"
using namespace PressureCapture;
constexpr bool meanContract() {
  Mean m;
  if (m.add(-100) != -100 || m.count != 1) return false;
  if (m.add(100) != 0 || m.count != 2) return false;
  for (int i = 0; i < 8; ++i) m.add(0);
  if (m.add(100) != 20 || m.count != 10) return false;
  Mean extremes;
  for (int i = 0; i < 100; ++i) if (extremes.add(-32768) != -32768) return false;
  for (int i = 0; i < 10; ++i) extremes.add(32767);
  return extremes.add(32767) == 32767;
}
constexpr bool clockContract() {
  PublishClock c;
  if (c.due(29999) || !c.due(30000) || c.due(30000)) return false;
  if (!c.due(100000) || c.due(100001) || !c.due(120000)) return false;
  c.lastUs = 0xfffffff0U;
  return !c.due(29900) && c.due(30000);
}
constexpr bool lifecycleContract() {
  Reply r{};
  r.status = Status::Armed;
  advance(r, false, true, false, false, 59000, 0);
  if (r.status != Status::Armed || r.fastActive) return false;
  advance(r, true, false, false, false, 59001, 0);
  if (r.status != Status::Recording || !r.fastActive) return false;
  advance(r, true, false, false, false, 74001, 15000000);
  if (r.status != Status::Complete || r.end != End::Duration || !r.fastActive) return false;
  if (canRead(r.status, r.fastActive, true)) return false;
  advance(r, false, false, false, false, 74002, 15001000);
  if (r.fastActive || !canRead(r.status, r.fastActive, true) || canRead(r.status, false, false)) return false;
  // A later shot does not silently re-arm the capture.
  advance(r, true, false, false, false, 75000, 0);
  return r.status == Status::Complete && !r.fastActive;
}
constexpr bool cancellationContract() {
  Reply r{}; r.status = Status::Armed;
  advance(r, true, false, true, false, 1, 0);
  if (r.status != Status::Cancelled || r.fastActive) return false;
  r = Reply{}; r.status = Status::Armed;
  advance(r, true, false, false, false, 60000, 0);
  if (r.status != Status::Expired || r.fastActive) return false;
  r = Reply{}; r.status = Status::Armed;
  advance(r, false, false, false, false, 1, 0);
  if (r.end != End::StateChanged || r.fastActive) return false;
  r = Reply{}; r.status = Status::Recording; r.fastActive = 1;
  advance(r, true, false, true, false, 0, 1);
  if (r.status != Status::Cancelled || !r.fastActive) return false;
  advance(r, false, true, true, false, 0, 2);
  if (r.fastActive || r.end != End::Cancel) return false;
  r = Reply{}; r.status = Status::Recording; r.fastActive = 1;
  advance(r, true, false, false, true, 0, 1);
  if (r.end != End::CalibrationChanged || !r.fastActive) return false;
  r = Reply{}; r.status = Status::Recording; r.fastActive = 1;
  advance(r, false, false, false, false, 0, 1);
  return r.end == End::ShotStopped && !r.fastActive;
}
static_assert(meanContract(), "Signed moving mean, warmup, replacement and overflow");
static_assert(clockContract(), "Publish cadence, no catch-up burst, micros wrap");
static_assert(lifecycleContract(), "Record next shot only, freeze then restore on stop");
static_assert(cancellationContract(), "Expiry, cancel, calibration/state change, early stop");
static_assert(chunkSize(5120, 5120) == 0 && chunkSize(5120, 5110) == 10 &&
              chunkSize(5120, 0) == 20 && chunkSize(5, 6) == 0, "Read bounds");
static_assert(!canRead(Status::Armed, false, true) &&
              !canRead(Status::Recording, false, true), "No live bulk reads");
