#include "heater_control.h"
#include "src/control/heater_core.h"
#include "config/hardware_pins.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <esp_idf_version.h>
#include <stddef.h>
namespace Heater {
namespace {
HeaterControl::Core core;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
bool water = false, blocked = false, ready = false;
bool powerOnPending = false;
float inletPressure = 0, inletDuty = 0;
#pragma pack(push, 1)
struct LegacyConfig {
  float target, kp, ki, kd, maxPower, ceiling;
  uint16_t idleMinutes;
};
#pragma pack(pop)
void task(void*) {
  const bool registered = esp_task_wdt_add(nullptr) == ESP_OK;
  portENTER_CRITICAL(&mux); ready = registered; portEXIT_CRITICAL(&mux);
  for (;;) {
    portENTER_CRITICAL(&mux);
    core.tick(millis(), water);
    digitalWrite(SSR_HEATER_PIN, core.s.relay ? HIGH : LOW);
    portEXIT_CRITICAL(&mux);
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
}
void begin() {
  digitalWrite(SSR_HEATER_PIN, LOW); pinMode(SSR_HEATER_PIN, OUTPUT);
  Preferences prefs;
  bool loaded = false;
  if (prefs.begin("heater-v4", true)) {
    HeaterWire::Config cfg;
    if (prefs.getBytesLength("config") == sizeof(cfg) &&
        prefs.getBytes("config", &cfg, sizeof(cfg)) == sizeof(cfg) && HeaterControl::validConfig(cfg)) { core.s.config = cfg; loaded = true; }
    prefs.end();
  }
  if (!loaded && prefs.begin("heater-v3", true)) {
    HeaterWire::Config cfg;
    constexpr size_t oldSize = offsetof(HeaterWire::Config, waterMaxPower);
    if (prefs.getBytesLength("config") == oldSize &&
        prefs.getBytes("config", &cfg, oldSize) == oldSize) {
      cfg.waterMaxPower = cfg.maxPower;
      if (HeaterControl::validConfig(cfg)) { core.s.config = cfg; loaded = true; }
    }
    prefs.end();
  }
  if (!loaded && prefs.begin("heater-v2", true)) {
    HeaterWire::Config cfg;
    // v2 is the packed prefix of v3; newly added FF fields retain OFF defaults.
    constexpr size_t oldSize = offsetof(HeaterWire::Config, heaterW);
    if (prefs.getBytesLength("config") == oldSize &&
        prefs.getBytes("config", &cfg, oldSize) == oldSize && HeaterControl::validConfig(cfg)) {
      cfg.waterMaxPower = cfg.maxPower;
      core.s.config = cfg; loaded = true;
    }
    prefs.end();
  }
  if (!loaded && prefs.begin("heater-v1", true)) {
    LegacyConfig old{};
    if (prefs.getBytesLength("config") == sizeof(old) && prefs.getBytes("config", &old, sizeof(old)) == sizeof(old)) {
      HeaterWire::Config cfg;
      cfg.kp=old.kp; cfg.ki=old.ki; cfg.kd=old.kd; cfg.maxPower=old.maxPower;
      cfg.ceiling=old.ceiling; cfg.idleMinutes=old.idleMinutes;
      cfg.waterMaxPower=cfg.maxPower;
      // Preserve commissioned limits/gains; never raise an old thermal ceiling.
      if (HeaterControl::validConfig(cfg)) core.s.config=cfg;
    }
    prefs.end();
  }
  powerOnPending = esp_reset_reason() == ESP_RST_POWERON;
  core.mainAt = millis();
  // Reconfigure/init the IDF task watchdog with panic/reset on timeout.
  esp_err_t watchdogStatus;
#if ESP_IDF_VERSION_MAJOR >= 5
  const esp_task_wdt_config_t watchdogConfig = {5000, (1U << portNUM_PROCESSORS) - 1U, true};
  watchdogStatus = esp_task_wdt_init(&watchdogConfig);
  if (watchdogStatus == ESP_ERR_INVALID_STATE)
    watchdogStatus = esp_task_wdt_reconfigure(&watchdogConfig);
#else
  watchdogStatus = esp_task_wdt_init(5, true);
#endif
  if (watchdogStatus == ESP_OK)
    xTaskCreate(task, "Heater", 3072, nullptr, 2, nullptr);
}
void sample(float c) { portENTER_CRITICAL(&mux); core.sample(c, millis()); digitalWrite(SSR_HEATER_PIN, core.s.relay ? HIGH : LOW); portEXIT_CRITICAL(&mux); }
void heartbeat(bool running, float estimatedFlow, bool valveOpen, bool flowValid, float pressure, float duty) {
  portENTER_CRITICAL(&mux);
  core.mainAt = millis(); water = running;
  inletPressure = std::isfinite(pressure) ? pressure : 0;
  inletDuty = std::isfinite(duty) ? duty : 0;
  // Trial outlet-derived estimate, never presented as a flowmeter measurement.
  core.flowInput(estimatedFlow, valveOpen, flowValid, core.mainAt);
  if (powerOnPending && (core.sampled || core.s.fault != HeaterWire::Fault::None)) {
    powerOnPending = false;
    HeaterWire::Request r; r.op = HeaterWire::Op::Enable;
    core.s.result = core.command(r, millis(), ready && !blocked && !running);
  }
  portEXIT_CRITICAL(&mux);
}
void inhibit() { portENTER_CRITICAL(&mux); powerOnPending = false; blocked = true; core.off(); digitalWrite(SSR_HEATER_PIN, LOW); portEXIT_CRITICAL(&mux); }
void releaseInhibit() { portENTER_CRITICAL(&mux); blocked = false; portEXIT_CRITICAL(&mux); }
HeaterWire::Status status() { portENTER_CRITICAL(&mux); auto result = core.s; portEXIT_CRITICAL(&mux); return result; }
void diagnostics(HeaterSensorWire::Status& result) {
  portENTER_CRITICAL(&mux);
  result.controlSamples = core.sensorWindow.count; result.controlErrors = core.sensorWindow.errors;
  result.paused = core.sensorPaused;
  result.densityPaused = core.sensorDensityPaused;
  result.thermalFeedforward = core.feedforward.output;
  result.inletFlow = core.inletFlow; result.feedforwardKff = core.feedforward.kff;
  result.flowQualified = core.inletFlowValid && millis()-core.flowAt <= 1000;
  result.inletValveOpen = core.inletValveOpen;
  result.pidPeriodMs = HeaterControlModel::PID_PERIOD_MS;
  result.inletPressure = inletPressure; result.inletDuty = inletDuty;
  portEXIT_CRITICAL(&mux);
}
void request(const HeaterWire::Request& r, bool allowed) {
  portENTER_CRITICAL(&mux);
  powerOnPending = false;
  core.s.result = core.command(r, millis(), allowed && !blocked && ready);
  core.s.ticket = r.ticket;
  digitalWrite(SSR_HEATER_PIN, core.s.relay ? HIGH : LOW);
  const bool save = r.op == HeaterWire::Op::Configure && core.s.result == HeaterWire::Result::Ok;
  const auto config = core.s.config;
  portEXIT_CRITICAL(&mux);
  if (save) {
    Preferences prefs;
    const bool opened = prefs.begin("heater-v4", false);
    const bool saved = opened && prefs.putBytes("config", &config, sizeof(config)) == sizeof(config);
    if (opened) prefs.end();
    if (!saved) { portENTER_CRITICAL(&mux); core.s.result = HeaterWire::Result::Unsafe; portEXIT_CRITICAL(&mux); }
  }
}
bool brewReady() {
  portENTER_CRITICAL(&mux);
  const bool result = ready && !blocked && core.s.mode == HeaterWire::Mode::Pid &&
      core.s.ready && core.sampled && !core.sensorWindow.failed() &&
      !core.sensorPaused && millis()-core.sampleAt <= HeaterSensorPolicy::PAUSE_GOOD_AGE_MS &&
      millis()-core.mainAt <= 1000 && core.s.fault == HeaterWire::Fault::None;
  portEXIT_CRITICAL(&mux); return result;
}
bool preheat() {
  portENTER_CRITICAL(&mux);
  powerOnPending = false;
  HeaterWire::Request r; r.op = HeaterWire::Op::Enable;
  const bool result = core.command(r, millis(), ready && !blocked && !water) == HeaterWire::Result::Ok;
  portEXIT_CRITICAL(&mux); return result;
}
}
