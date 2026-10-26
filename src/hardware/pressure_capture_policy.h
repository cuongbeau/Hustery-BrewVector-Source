#pragma once
#include "protocol/pressure_capture.h"

namespace PressureCapture {
using namespace PressureCaptureWire;
// Real averaging helper, independent of hardware for compile-time contracts.
struct Mean {
  int16_t values[MEAN_WINDOW]{};
  int32_t sum = 0;
  uint8_t count = 0, index = 0;
  constexpr float add(int16_t value) {
    if (count == MEAN_WINDOW) sum -= values[index];
    else ++count;
    values[index] = value;
    sum += value;
    index = (index + 1) % MEAN_WINDOW;
    return static_cast<float>(sum) / count;
  }
};
struct PublishClock {
  uint32_t lastUs = 0;
  constexpr bool due(uint32_t nowUs) {
    const uint32_t elapsed = nowUs - lastUs;
    if (elapsed < PUBLISH_MS * 1000U) return false;
    lastUs += (elapsed / (PUBLISH_MS * 1000U)) * (PUBLISH_MS * 1000U);
    return true; // No catch-up burst after a late/blocked read.
  }
};
constexpr bool canRead(Status status, bool fast, bool stopped) {
  return !fast && stopped && status != Status::Armed && status != Status::Recording;
}
constexpr uint8_t chunkSize(uint16_t total, uint16_t offset) {
  return offset >= total ? 0 :
      (total - offset > CHUNK_ROWS ? CHUNK_ROWS : total - offset);
}
constexpr void advance(Reply& meta, bool brew, bool idle, bool cancel,
                       bool calibrationChanged, uint32_t armedAgeMs,
                       uint32_t runAgeUs) {
  if (meta.status == Status::Armed) {
    if (cancel) { meta.status = Status::Cancelled; meta.end = End::Cancel; }
    else if (armedAgeMs >= 60000U) meta.status = Status::Expired;
    else if (calibrationChanged) { meta.status = Status::Cancelled; meta.end = End::CalibrationChanged; }
    else if (brew) { meta.status = Status::Recording; meta.fastActive = 1; }
    else if (!idle) { meta.status = Status::Cancelled; meta.end = End::StateChanged; }
  }
  if (meta.fastActive) {
    if (meta.status == Status::Recording) {
      if (!brew) { meta.status = Status::Complete; meta.end = End::ShotStopped; }
      else if (cancel) { meta.status = Status::Cancelled; meta.end = End::Cancel; }
      else if (calibrationChanged) { meta.status = Status::Cancelled; meta.end = End::CalibrationChanged; }
      else if (runAgeUs >= DURATION_MS * 1000U) { meta.status = Status::Complete; meta.end = End::Duration; }
    }
    if (!brew) meta.fastActive = 0;
  }
}
}
