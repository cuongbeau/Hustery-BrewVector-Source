#include "adc_probe.h"
#include "adc_probe_engine.h"
#include "pump_control.h"
#include <Arduino.h>
#include <Wire.h>

namespace AdcProbe {
namespace {
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Reply visible{};
uint32_t pendingId = 0;
bool cancelPending = false;
Engine engine;
struct Bus {
  void (*consumer)(int16_t);
  uint32_t now() { return micros(); }
  bool write(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(0x48); Wire.write(reg);
    Wire.write(static_cast<uint8_t>(value >> 8)); Wire.write(static_cast<uint8_t>(value));
    return Wire.endTransmission() == 0;
  }
  bool read(uint8_t reg, uint16_t& value) {
    Wire.beginTransmission(0x48); Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(uint8_t(0x48), uint8_t(2)) != 2) return false;
    value = (static_cast<uint16_t>(Wire.read()) << 8) | Wire.read();
    return true;
  }
  void publish(int16_t raw) { consumer(raw); }
};
}
bool engaged() {
  portENTER_CRITICAL(&mux);
  const bool result = pendingId || Engine::active(visible.state);
  portEXIT_CRITICAL(&mux);
  return result;
}
void request(const Request& req, Reply& reply, bool allowed) {
  Result result = Result::Ok;
  if (req.version != VERSION || !req.requestId || req.op > Op::Cancel) result = Result::Invalid;
  else if (req.op == Op::Start) {
    portENTER_CRITICAL(&mux);
    const bool same = req.runId && visible.runId == req.runId;
    const bool active = pendingId || Engine::active(visible.state);
    portEXIT_CRITICAL(&mux);
    if (!req.runId) result = Result::Invalid;
    else if (same) {} // Lost reply retries never restart a completed test.
    else if (!allowed || active) result = Result::Busy;
    else if (!PumpPSM::holdForAdcProbe()) result = Result::Busy;
    else {
      portENTER_CRITICAL(&mux);
      visible = Reply{}; visible.runId = req.runId;
      visible.state = State::Pending; visible.pumpHeld = 1;
      pendingId = req.runId; cancelPending = false;
      portEXIT_CRITICAL(&mux);
    }
  } else if (req.op == Op::Cancel) {
    portENTER_CRITICAL(&mux);
    if (!req.runId || req.runId != visible.runId) result = Result::WrongRun;
    else cancelPending = true;
    portEXIT_CRITICAL(&mux);
  }
  portENTER_CRITICAL(&mux);
  reply = visible;
  portEXIT_CRITICAL(&mux);
  reply.requestId = req.requestId; reply.result = result; reply.version = VERSION;
}
bool tick(bool safe, void (*publish)(int16_t)) {
  portENTER_CRITICAL(&mux);
  const uint32_t id = pendingId; pendingId = 0;
  const bool cancel = cancelPending; cancelPending = false;
  portEXIT_CRITICAL(&mux);
  if (id) engine.begin(id, millis(), micros());
  if (!Engine::active(engine.report.state)) return false;
  Bus bus{publish};
  engine.tick(bus, micros(), safe, cancel, PumpPSM::adcProbeHoldAcknowledged());
  if (!engine.report.pumpHeld) PumpPSM::releaseAdcProbeHold();
  portENTER_CRITICAL(&mux);
  visible = engine.report;
  portEXIT_CRITICAL(&mux);
  return true;
}
}
