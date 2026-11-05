#pragma once
#include "protocol/heater_wire.h"
#include "heater_sensor_policy.h"
#include "heater_control_model.h"
#include <cmath>
namespace HeaterControl {
using namespace HeaterWire;
constexpr float clamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
constexpr bool validConfig(const Config& c) {
  return std::isfinite(c.target) && c.target >= 30 && c.target <= 130 &&
    std::isfinite(c.ceiling) && c.ceiling >= c.target + 5 && c.ceiling <= 150 &&
    std::isfinite(c.maxPower) && c.maxPower > 0 && c.maxPower <= 100 &&
    std::isfinite(c.waterMaxPower) && c.waterMaxPower > 0 && c.waterMaxPower <= 100 &&
    std::isfinite(c.kp) && c.kp >= 0 && c.kp <= 100 &&
    std::isfinite(c.ki) && c.ki >= 0 && c.ki <= 10 &&
    std::isfinite(c.kd) && c.kd >= 0 && c.kd <= 100 &&
    c.idleMinutes >= 1 && c.idleMinutes <= 240 &&
    c.sleepDelaySeconds <= 3600 && c.stableSeconds <= 300 && c.autoSleep <= 1 &&
    std::isfinite(c.heaterW) && c.heaterW >= 100 && c.heaterW <= 4000 &&
    std::isfinite(c.inletC) && c.inletC >= 0 && c.inletC <= 60 &&
    std::isfinite(c.ffScale) && c.ffScale >= 0 && c.ffScale <= 1 && c.ffEnabled <= 1;
}
// Pure controller/state machine. Caller serializes access and owns physical GPIO.
class Core {
 public:
  Status s{};
  uint32_t sampleAt = 0, mainAt = 0, activityAt = 0, testAt = 0, windowAt = 0;
  uint32_t testDuration = 0;
  float testPower = 0;
  bool sampled = false;
  bool stableTracking = false, firstReady = false;
  uint32_t stableAt = 0;
  HeaterSensorPolicy::ErrorWindow sensorWindow;
  bool sensorPaused = false;
  bool sensorDensityPaused = false;
  HeaterControlModel::Pid pid;
  HeaterControlModel::TemperatureRate temperatureRate;
  HeaterControlModel::Feedforward feedforward;
  uint32_t pidAt = 0, flowAt = 0;
  float inletFlow = 0;
  bool inletValveOpen = false, inletFlowValid = false;
  bool previousWater = false;
  // Provisional software policy; physical thermal protection remains necessary.
  static constexpr uint32_t HEAT_CONFIRM_MS = 500, HEAT_SAMPLE_GAP_MS = 500;
  bool overtempPending = false;
  uint8_t hotSamples = 0, coolSamples = 0;
  uint32_t hotAt = 0, coolAt = 0, heatSampleAt = 0;
  uint32_t overtempEvents = 0, overtempAt = 0;
  float overtempPeak = 0;
  constexpr void holdOvertemp() {
    s.power = 0; s.relay = 0; s.ready = 0; stableTracking = false;
    feedforward.output = 0;
  }
  constexpr void checkTemperature(float value, uint32_t now) {
    const float limit = s.config.ceiling > 0 ? s.config.ceiling : 150.0f;
    // Extreme readings still latch immediately, independently of the filter.
    if (value >= clamp(limit + 10, 0, 150)) { trip(Fault::Overtemp); return; }
    if (now - heatSampleAt > HEAT_SAMPLE_GAP_MS) hotSamples = coolSamples = 0;
    heatSampleAt = now;
    if (value >= limit) {
      if (!overtempPending) {
        overtempPending = true; ++overtempEvents; overtempAt = now; overtempPeak = value;
      }
      if (value > overtempPeak) overtempPeak = value;
      coolSamples = 0;
      if (!hotSamples) hotAt = now;
      if (hotSamples < 3) ++hotSamples;
      holdOvertemp();
      if (hotSamples >= 3 && now - hotAt >= HEAT_CONFIRM_MS) trip(Fault::Overtemp);
    } else {
      hotSamples = 0;
      if (!overtempPending) return;
      holdOvertemp();
      if (value <= limit - 3 && s.filtered <= limit - 3 &&
          sensorWindow.recovered() && !sensorPaused) {
        if (!coolSamples) coolAt = now;
        if (coolSamples < 3) ++coolSamples;
        if (coolSamples >= 3 && now - coolAt >= HEAT_CONFIRM_MS) {
          overtempPending = false; coolSamples = 0;
        }
      } else coolSamples = 0;
    }
  }
  constexpr void off(bool expired = false) {
    s.mode = s.fault == Fault::None ? Mode::Off : Mode::Fault;
    s.power = s.p = s.i = s.d = 0; s.relay = 0;
    s.idleExpired = expired;
    s.ready = 0; stableTracking = firstReady = false;
    sensorPaused = sensorDensityPaused = false;
    overtempPending = false; hotSamples = coolSamples = 0;
    pid.reset(); temperatureRate.reset(); feedforward.reset();
    s.activeMaxPower = s.config.maxPower;
  }
  constexpr void trip(Fault fault) { if (s.fault == Fault::None) s.fault = fault; off(); }
  constexpr bool active() const { return s.mode == Mode::Pid || s.mode == Mode::Test; }
  constexpr void pauseSensor(bool density = false) {
    if (density) sensorDensityPaused = true;
    sensorPaused = true; s.power = 0; s.relay = 0;
    s.ready = 0; stableTracking = false;
    feedforward.output = 0; // Preserve I and deadlines while paused.
    s.activeMaxPower = s.config.maxPower;
  }
  constexpr void guardActive(uint32_t now) {
    if (!active()) return;
    if (!sampled || sensorWindow.failed()) trip(Fault::Sensor);
    else if (now - sampleAt > HeaterSensorPolicy::MAX_GOOD_AGE_MS) trip(Fault::Stale);
    else if (now - mainAt > 1000) trip(Fault::MainStalled);
    else if (sensorWindow.shouldPause()) pauseSensor(true);
    else if (now - sampleAt > HeaterSensorPolicy::PAUSE_GOOD_AGE_MS) pauseSensor();
  }
  constexpr void sample(float value, uint32_t now) {
    const bool bad = !std::isfinite(value) || value < -20 || value > 500;
    sensorWindow.add(bad);
    // A new valid sample must not conceal a timeout already crossed while active.
    guardActive(now);
    if (bad) {
      s.valid = 0;
      hotSamples = coolSamples = 0; // Invalid reads cannot confirm heat or recovery.
      if (sensorWindow.failed()) trip(Fault::Sensor);
      // Sensor holds the last accepted temperature; independent 1Hz PID may use it.
      return;
    }
    const bool recovering = sensorPaused || overtempPending;
    const bool first = !sampled || now - sampleAt > HeaterSensorPolicy::PAUSE_GOOD_AGE_MS;
    const float dt = first ? 0.25f : (now - sampleAt) / 1000.0f;
    const float previous = s.filtered;
    s.raw = value;
    // Restore the original single 0.3s low-pass; no median-stage delay.
    s.filtered = first ? value : previous + (dt / (0.3f + dt)) * (value - previous);
    sampled = true; s.valid = 1; sampleAt = now;
    if (s.fault == Fault::None) checkTemperature(value, now);
    if (s.fault != Fault::None) return;
    if (sensorWindow.failed()) { trip(Fault::Sensor); return; }
    temperatureRate.sample(s.filtered, now, first || recovering);
    if (active()) {
      if (sensorDensityPaused) {
        if (sensorWindow.recovered()) sensorPaused = sensorDensityPaused = false;
      } else sensorPaused = false;
    }
    if (overtempPending || (active() && sensorPaused)) return;
    if (s.mode != Mode::Pid || dt <= 0) return;

    if (first) { stableTracking = false; s.ready = 0; }
    // Brew permission is a minimum body temperature, not a tight PID settling
    // band or the estimated brew-water temperature. Keep lower test setpoints usable.
    const float brewMinimum = s.config.target < 100.0f ? s.config.target : 100.0f;
    const bool inBand = s.filtered >= brewMinimum && s.raw >= brewMinimum;
    if (!inBand) { stableTracking = false; s.ready = 0; }
    else {
      if (!stableTracking) { stableTracking = true; stableAt = now; }
      s.ready = now - stableAt >= s.config.stableSeconds * 1000U;
      if (s.ready && !firstReady) { firstReady = true; activityAt = now; }
    }
    // Recalculate P from recovered temperature; reset D and do not integrate
    // the paused interval. PWM resumes only at the next guarded task tick.
    if (recovering) { updatePid(true); pidAt = now; }
  }
  constexpr void updatePid(bool recovery = false) {
    pid.step(s.config.target, s.filtered, s.config.kp, s.config.ki, s.config.kd,
             feedforward.output, s.activeMaxPower, recovery, temperatureRate.rate);
    s.p = pid.p; s.i = pid.i; s.d = pid.d; s.power = pid.output;
    if (s.power == 0) s.relay = 0;
  }
  constexpr void flowInput(float mlS, bool valveOpen, bool valid, uint32_t now) {
    inletFlowValid = valid && std::isfinite(mlS) && mlS >= 0;
    inletFlow = inletFlowValid ? mlS : 0; inletValveOpen = valveOpen; flowAt = now;
  }
  constexpr Result command(const Request& r, uint32_t now, bool allowed) {
    if (r.op == Op::Off) { off(); return Result::Ok; }
    if (!allowed) return Result::Busy;
    if (r.op == Op::Configure) {
      if (active()) return Result::Busy;
      if (!validConfig(r.config)) return Result::Invalid;
      s.config = r.config; s.activeMaxPower = s.config.maxPower; activityAt = now;
      if (s.valid && s.raw >= s.config.ceiling) trip(Fault::Overtemp);
      return Result::Ok;
    }
    if (!s.valid || !sampled || now - sampleAt > HeaterSensorPolicy::PAUSE_GOOD_AGE_MS || now - mainAt > 1000) return Result::Unsafe;
    if (r.op == Op::ClearFault) {
      // Acknowledge a recovered sensor without commissioning the heater.
      // Clearing only returns to OFF; Enable/Test still require validConfig.
      const float limit = s.config.ceiling > 0 ? s.config.ceiling : 150.0f;
      if (s.raw >= limit - 3 || sensorWindow.failed()) return Result::Unsafe;
      s.fault = Fault::None; off(); return Result::Ok;
    }
    if (overtempPending || !validConfig(s.config) || s.raw >= s.config.ceiling - 3) return Result::Unsafe;
    if (sensorWindow.failed() || sensorWindow.shouldPause()) return Result::Unsafe;
    if (s.fault != Fault::None) return Result::Unsafe;
    if (r.op != Op::Enable && r.op != Op::Test) return Result::Invalid;
    if (active()) return Result::Busy;
    if (r.op == Op::Test && (!std::isfinite(r.testPower) || r.testPower <= 0 ||
        r.testPower > s.config.maxPower || r.testSeconds < 1 || r.testSeconds > 60)) return Result::Invalid;
    off(); activityAt = windowAt = testAt = now;
    testDuration = r.testSeconds * 1000U; testPower = r.testPower;
    s.mode = r.op == Op::Enable ? Mode::Pid : Mode::Test;
    temperatureRate.sample(s.filtered, sampleAt, true);
    pidAt = now;
    return Result::Ok;
  }
  constexpr void tick(uint32_t now, bool water) {
    s.uptime = now; s.sampleAge = sampled ? now - sampleAt : UINT32_MAX;
    // Start a fresh SSR window when water starts so the newly calculated
    // compensation does not wait behind the old idle PWM phase.
    if (water && !previousWater && s.mode == Mode::Pid) windowAt = now;
    previousWater = water;
    if (water) activityAt = now;
    guardActive(now);
    const uint32_t idleMs = s.config.idleMinutes * 60000U + s.config.sleepDelaySeconds * 1000U;
    if (active() && s.config.autoSleep && !water && now - activityAt >= idleMs) off(true);
    if (s.mode == Mode::Test && (water || now - testAt >= testDuration)) off();
    feedforward.kff = s.config.ffEnabled ? 100.0f/s.config.heaterW*s.config.ffScale : 0;
    feedforward.inletC = s.config.inletC;
    const bool qualifiedWater = water && s.mode == Mode::Pid && !sensorPaused && !overtempPending &&
        s.config.ffEnabled && s.config.ffScale > 0 && inletValveOpen &&
        inletFlowValid && inletFlow > 0.01f && now-flowAt <= 1000;
    s.activeMaxPower = qualifiedWater && s.config.waterMaxPower > s.config.maxPower
        ? s.config.waterMaxPower : s.config.maxPower;
    // Cap withdrawal is independent of FF magnitude and the 1Hz PID schedule.
    if (s.mode == Mode::Pid && s.power > s.activeMaxPower) s.power = s.activeMaxPower;
    if (s.mode == Mode::Pid && !sensorPaused && !overtempPending) {
      // Raw temperature is the conservative upper-temperature check; the
      // filtered value remains the feedback input to the 1 Hz PID.
      const float hottestBody = s.raw > s.filtered ? s.raw : s.filtered;
      feedforward.update(s.config.target, hottestBody, inletFlow, inletValveOpen,
                         qualifiedWater);
      if (now-pidAt >= HeaterControlModel::PID_PERIOD_MS) { pidAt = now; updatePid(); }
      else s.power = clamp(s.p+s.i+s.d+feedforward.output, 0, s.activeMaxPower);
    } else feedforward.output = 0;
    if (s.mode == Mode::Test && !sensorPaused && !overtempPending) s.power = testPower;
    if (!active() || sensorPaused || overtempPending) s.power = 0;
    s.idleRemaining = active() && s.config.autoSleep ? (now - activityAt >= idleMs ? 0 : idleMs - (now - activityAt)) : 0;
    if (now - windowAt >= 1000) windowAt = now;
    s.relay = active() && !sensorPaused && !overtempPending && s.power > 0 && now - windowAt < static_cast<uint32_t>(s.power * 10);
  }
};
}
