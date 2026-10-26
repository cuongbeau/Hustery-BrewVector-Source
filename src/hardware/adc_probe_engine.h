#pragma once
#include "protocol/adc_probe.h"

namespace AdcProbe {
using namespace AdcProbeWire;
// Sole caller is the ADC task. Bus methods return false on any I2C failure.
// One bounded poll per tick; no conversion busy-wait or task creation.
struct Engine {
  Reply report{};
  bool configured = false, restoreStarted = false, saved = false;
  uint32_t conversionUs = 0, busyUs = 0, lastPollUs = 0, totalUs = 0;
  static constexpr uint8_t rate(uint8_t index) { return index < 6 ? 0 : index < 12 ? 2 : index < 18 ? 4 : 7; }
  static constexpr bool active(State s) { return s == State::Pending || s == State::Running || s == State::Restoring || s == State::RestoreFailed; }
  constexpr void begin(uint32_t id, uint32_t nowMs, uint32_t nowUs) {
    *this = Engine{};
    report.runId = id; report.startedMs = nowMs; report.version = VERSION;
    report.state = State::Pending; report.pumpHeld = 1; totalUs = nowUs;
  }
  constexpr void finish() {
    report.state = report.error == Error::None ? State::Complete :
      (report.error == Error::Cancel || report.error == Error::MachineActive ? State::Cancelled : State::Failed);
    report.pumpHeld = 0;
  }
  constexpr void restore(Error error) {
    report.error = error; report.state = State::Restoring;
    restoreStarted = false; configured = false;
  }
  template<class Bus> constexpr void tick(Bus& bus, uint32_t nowUs, bool safe, bool cancel, bool pumpAck) {
    if (!active(report.state)) return;
    if (report.state == State::RestoreFailed) {
      if (cancel) { report.state = State::Restoring; restoreStarted = false; }
      return;
    }
    if (report.state != State::Restoring) {
      if (cancel || !safe) restore(cancel ? Error::Cancel : Error::MachineActive);
      else if (nowUs - totalUs > 5000000U) restore(Error::Timeout);
    }
    if (report.state == State::Pending) {
      if (!pumpAck) {
        if (nowUs - totalUs > 250000U) restore(Error::PumpAck);
        return;
      }
      uint16_t config = 0;
      if (!bus.read(1, config)) { restore(Error::Bus); return; }
      report.savedConfig = config; saved = true;
      // Drain previous conversion in single-shot at the original rate before
      // timing any new rate. Avoid measuring completion of the old conversion.
      conversionUs = nowUs;
      if (!bus.write(1, config | 0x8100U)) { restore(Error::Bus); return; }
      configured = false; report.state = State::Running;
      return;
    }
    if (report.state == State::Restoring) {
      if (!saved) { finish(); return; } // ADC registers never modified.
      if (!restoreStarted) {
        // First wait for any in-flight test conversion to finish. Changing rate
        // while busy does not guarantee the next ready is a fresh normal sample.
        uint16_t config = 0;
        if (!bus.read(1, config)) { report.state = State::RestoreFailed; return; }
        if (!(config & 0x8000U)) {
          if (nowUs - conversionUs > 250000U) report.state = State::RestoreFailed;
          return;
        }
        conversionUs = nowUs;
        // Restore in single-shot first, even for the continuous A/B baseline.
        if (!bus.write(1, report.savedConfig | 0x8100U)) { report.state = State::RestoreFailed; return; }
        restoreStarted = true;
        return;
      }
      uint16_t config = 0, raw = 0;
      if (!bus.read(1, config)) { report.state = State::RestoreFailed; return; }
      const uint16_t expected = (report.savedConfig | 0x0100U) & 0x7fffU;
      if ((config & 0x7fffU) != expected) { report.state = State::RestoreFailed; return; }
      if (!(config & 0x8000U)) {
        if (nowUs - conversionUs > 250000U) report.state = State::RestoreFailed;
        return;
      }
      if (!bus.read(0, raw)) { report.state = State::RestoreFailed; return; }
      // Restore exact saved mode/rate/comparator settings, verify, and only
      // then publish the fresh normal sample and release the pump interlock.
      if (!bus.write(1, report.savedConfig | 0x8000U) || !bus.read(1, config) ||
          (config & 0x7fffU) != (report.savedConfig & 0x7fffU)) {
        report.state = State::RestoreFailed; return;
      }
      report.restoredConfig = config;
      bus.publish(static_cast<int16_t>(raw));
      finish(); return;
    }
    uint16_t config = 0;
    const uint32_t pollStart = bus.now();
    if (!bus.read(1, config)) { restore(Error::Bus); return; }
    const uint32_t checkedUs = bus.now();
    if (lastPollUs && pollStart - lastPollUs > report.maxPollGapUs)
      report.maxPollGapUs = pollStart - lastPollUs;
    lastPollUs = pollStart;
    if (configured && (config & 0x7fffU) != (0x4303U | (rate(report.count) << 5))) {
      restore(Error::Config); return;
    }
    if (!(config & 0x8000U)) {
      busyUs = pollStart - conversionUs;
      if (checkedUs - conversionUs > 250000U) restore(Error::Timeout);
      return;
    }
    if (configured) {
      uint16_t raw = 0;
      if (!bus.read(0, raw)) { restore(Error::Bus); return; }
      const uint8_t code = rate(report.count);
      report.samples[report.count++] = {checkedUs - conversionUs, busyUs,
          static_cast<int16_t>(raw), code, static_cast<uint8_t>(raw & 15U)};
      if (report.count == SAMPLES) { restore(Error::None); return; }
    }
    const uint16_t next = 0xc303U | (rate(report.count) << 5);
    conversionUs = bus.now(); busyUs = 0; lastPollUs = 0;
    if (!bus.write(1, next)) { restore(Error::Bus); return; }
    configured = true;
  }
};
}
