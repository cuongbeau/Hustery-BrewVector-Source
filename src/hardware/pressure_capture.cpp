#include "pressure_capture.h"
#include "pressure_capture_policy.h"
#include "ads1115_acquisition.h"
#include "interfaces/calibration_types.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

namespace PressureCapture {
namespace {
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Sample* rows = nullptr;
Reply meta{};
Mean mean;
uint32_t armedAtMs = 0, startedUs = 0;
bool cancelRequested = false;

void end(Status status, End why) { meta.status = status; meta.end = why; }
}

bool engaged() {
  portENTER_CRITICAL(&mux);
  const bool result = meta.status == Status::Armed || meta.fastActive;
  portEXIT_CRITICAL(&mux);
  return result;
}

void request(const Request& req, Reply& reply, bool idle, bool stopped, bool available) {
  // Only this Main-loop endpoint owns allocation. ADC cannot engage from a
  // terminal state until Arm is committed under mux below.
  Result result = Result::Ok;
  portENTER_CRITICAL(&mux);
  const bool active = meta.fastActive || meta.status == Status::Armed;
  const bool same = req.captureId && req.captureId == meta.captureId;
  portEXIT_CRITICAL(&mux);
  if (req.version != VERSION || !req.requestId || req.op > Op::Clear) result = Result::Invalid;
  else if (req.op == Op::Arm && same) {
    // Retry of the same capture is idempotent, including after completion.
  } else if (req.op == Op::Arm) {
    if (!Ads1115Acquisition::singleShot || !available) result = Result::Unsupported;
    else if (!req.captureId) result = Result::Invalid;
    else if (!idle || !stopped || active) result = Result::Busy;
    else if (!isfinite(gPress.offset) || !isfinite(gPress.scale) || gPress.scale <= 0) result = Result::Invalid;
    else {
      constexpr size_t bytes = sizeof(Sample) * CAPACITY;
      if (!rows && ESP.getFreeHeap() >= bytes + 32768U)
        rows = static_cast<Sample*>(malloc(bytes));
      if (!rows) result = Result::NoMemory;
      else {
        portENTER_CRITICAL(&mux);
        meta = Reply{};
        meta.captureId = req.captureId;
        meta.pressureOffset = gPress.offset;
        meta.pressureScale = gPress.scale;
        meta.status = Status::Armed;
        mean = Mean{};
        armedAtMs = millis();
        cancelRequested = false;
        portEXIT_CRITICAL(&mux);
      }
    }
  } else if (req.op != Op::Status && !same) result = Result::WrongCapture;
  else if (req.op == Op::Cancel) {
    portENTER_CRITICAL(&mux);
    cancelRequested = true;
    portEXIT_CRITICAL(&mux);
  } else if (req.op == Op::Clear) {
    if (!stopped || active) result = Result::Busy;
    else {
      free(rows);
      rows = nullptr;
      portENTER_CRITICAL(&mux);
      meta = Reply{};
      cancelRequested = false;
      portEXIT_CRITICAL(&mux);
    }
  }
  portENTER_CRITICAL(&mux);
  // Copy metadata only, not the unused row array in meta.
  memset(&reply, 0, sizeof(reply));
  memcpy(&reply, &meta, offsetof(Reply, samples));
  if (result == Result::Ok && req.op == Op::Read) {
    if (!canRead(meta.status, meta.fastActive, stopped)) result = Result::Busy;
    else if (req.offset > meta.total) result = Result::Invalid;
    else {
      reply.offset = req.offset;
      reply.count = chunkSize(meta.total, req.offset);
      if (reply.count) memcpy(reply.samples, rows + req.offset, reply.count * sizeof(Sample));
    }
  }
  reply.version = VERSION;
  reply.requestId = req.requestId;
  reply.op = req.op;
  reply.result = result;
  portEXIT_CRITICAL(&mux);
}

bool tick(bool brew, bool idle, uint32_t nowMs, uint32_t nowUs) {
  portENTER_CRITICAL(&mux);
  const bool wasArmed = meta.status == Status::Armed;
  advance(meta, brew, idle, cancelRequested,
      gPress.offset != meta.pressureOffset || gPress.scale != meta.pressureScale,
      nowMs - armedAtMs, wasArmed ? 0 : nowUs - startedUs);
  if (wasArmed && meta.status == Status::Recording) {
    meta.startedAtMs = nowMs;
    startedUs = nowUs;
  }
  const bool fast = meta.fastActive;
  portEXIT_CRITICAL(&mux);
  return fast;
}

void record(int16_t raw, bool published, uint32_t nowUs, float filtered, float power) {
  portENTER_CRITICAL(&mux);
  if (meta.status == Status::Recording && rows) {
    if (meta.total < CAPACITY) {
      const float average = mean.add(raw);
      rows[meta.total++] = {nowUs - startedUs, raw, mean.count,
          static_cast<uint8_t>(published ? 1 : 0), average, filtered, power};
    }
    if (meta.total == CAPACITY) end(Status::Complete, End::Capacity);
  }
  portEXIT_CRITICAL(&mux);
}
void busy() {
  portENTER_CRITICAL(&mux);
  if (meta.status == Status::Recording) ++meta.busyPolls;
  portEXIT_CRITICAL(&mux);
}
}
