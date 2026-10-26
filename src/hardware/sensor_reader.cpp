// File: src/hardware/sensor_reader.cpp
// Tách từ control_logic.h dòng 40-374 — đọc sensor hardware
// Refactor: 2026-04-20
// V19: pressure control consumes shot.pressure (latest calibrated ADS sample)
// directly. The smoothing here is display/profile telemetry only.

#include "sensor_reader.h"
#include "heater_control.h"
#include "config/hardware_pins.h"
#include "src/model/flow_model.h"
#include "src/model/thermal_model.h"
#include "interfaces/system_state.h"
#include "ads1115_interface.h"
#include "pump_control.h"

#include <Adafruit_SPIDevice.h>
#include "max31855_frame.h"
#include "temperature_error_stats.h"

namespace SensorReader {

  // ======================= MAX31855 =======================
  // Same BusIO software SPI pins/mode/rate as the previous Adafruit driver.
  // Decode temperature and fault bits from ONE transaction, never readError()
  // in a second transaction that could describe another conversion.
  static Adafruit_SPIDevice thermo(MAX31855_CS, MAX31855_SCK, MAX31855_SO, -1, 1000000);
  static HeaterSensorWire::Status tempDiagnostics;
  static TemperatureErrorStats tempErrorStats;
  HeaterSensorWire::Status temperatureDiagnostics() {
    auto result = tempDiagnostics;
    const auto stats = tempErrorStats.snapshot(millis());
    result.windowSamples = stats.windowSamples; result.windowErrors = stats.windowErrors;
    result.peakWindowSamples = stats.peakWindowSamples; result.peakWindowErrors = stats.peakWindowErrors;
    result.consecutiveErrors = stats.consecutiveErrors; result.maxConsecutiveErrors = stats.maxConsecutiveErrors;
    result.evaluatedWindows = stats.evaluatedWindows; result.windowsAtOrAbove20 = stats.windowsAtOrAbove20;
    result.windowReady = stats.windowReady;
    Heater::diagnostics(result);
    result.controlCapacity = HeaterSensorPolicy::WINDOW;
    result.controlWarnErrors = HeaterSensorPolicy::WARNING_ERRORS;
    result.controlPauseErrors = HeaterSensorPolicy::PAUSE_ERRORS;
    result.controlResumeErrors = HeaterSensorPolicy::RESUME_ERRORS;
    result.controlTripErrors = HeaterSensorPolicy::MAX_ERRORS;
    result.samplePeriodMs = HeaterSensorPolicy::SAMPLE_MS;
    result.maxGoodAgeMs = HeaterSensorPolicy::MAX_GOOD_AGE_MS;
    result.pauseGoodAgeMs = HeaterSensorPolicy::PAUSE_GOOD_AGE_MS;
    return result;
  }
  static float tempC = NAN;

  static constexpr uint32_t TEMP_SAMPLE_MS = TemperatureErrorStats::SAMPLE_MS;
  static constexpr float    TEMP_ALPHA     = 0.90f;
  static unsigned long lastTempMs = 0;
  static float tempFilt = NAN;

  // Legacy EMA retained only for existing profile/flow/thermal consumers.
  // Live pressure and trace use the ADC-owned sensor filter.
  static float pressureControlFiltered = NAN;
  static constexpr float PRESSURE_TELEMETRY_ALPHA = 0.20f;

  static unsigned long lastTickMs = 0;

  float getTempFiltered() {
    return isnan(tempFilt) ? tempC : tempFilt;
  }

  // ======================= ADC helper: removed in favor of ADS1115_Interface =======================

  // ======================= INIT =======================
  void init() {
    ADS1115_Interface::init();

    delay(300);
    if (!thermo.begin()) Serial.println("[MAX31855] FAIL!");
    else Serial.println("[MAX31855] OK");

    lastTempMs = millis();
    tempFilt   = NAN;
    pressureControlFiltered = NAN;
    lastTickMs = millis();
  }

  // ======================= UPDATE: ĐỌC SENSOR =======================
  void update(ShotData& shot, bool isShotActive, unsigned long shotStart) {
    const unsigned long nowMs = millis();

    // ---- TEMP ----
    if (nowMs - lastTempMs >= TEMP_SAMPLE_MS) {
      lastTempMs = nowMs;
      uint8_t bytes[4] = {};
      const bool readOk = thermo.read(bytes, sizeof(bytes));
      const uint32_t frame = (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) |
                            (uint32_t(bytes[2]) << 8) | bytes[3];
      const uint8_t issues = Max31855Frame::issues(frame, readOk);
      tempErrorStats.add(issues != 0, nowMs);
      tempDiagnostics.sampledAt = nowMs;
      tempDiagnostics.frame = frame;
      tempDiagnostics.issues = issues;
      ++tempDiagnostics.total;
      if (issues) {
        ++tempDiagnostics.errors;
        tempDiagnostics.lastIssues = issues;
        tempDiagnostics.lastErrorAt = nowMs;
        tempDiagnostics.lastErrorFrame = frame;
      }
      double c = issues ? NAN : Max31855Frame::celsius(frame);
      Heater::sample(static_cast<float>(c));
      if (!isnan(c)) {
        tempC = (float)c;
        if (isnan(tempFilt)) tempFilt = tempC;
        else                 tempFilt = TEMP_ALPHA*tempFilt + (1.0f - TEMP_ALPHA)*tempC;
      }
      shot.temperature = isnan(tempFilt) ? tempC : tempFilt;
      shot.blockTempC = shot.temperature;
    }

    // ---- TIME & DELTA ----
    uint32_t elapsedMs = nowMs - lastTickMs;
    if(elapsedMs == 0) elapsedMs = 1;
    float dt_s = (float)elapsedMs / 1000.0f;
    shot.dt = dt_s;
    if (isShotActive) shot.shotTime = nowMs - shotStart;
    else              shot.shotTime = 0;

    // ---- PRESSURE (raw controller input + independent UI/profile filter) ----
#ifdef PRESSURE_ADC_PIN
    {
      const auto pressureSample = ADS1115_Interface::getPressureSnapshot();
      const float p_raw = pressureSample.valid ? pressureSample.rawBar : 0.0f;
      shot.visualPressure = pressureSample.sensorFilteredBar;

      float prev_pSmooth = shot.smoothedPressure;
      shot.pressure = p_raw;

      if (!isfinite(pressureControlFiltered)) {
        pressureControlFiltered = p_raw;
      } else {
        pressureControlFiltered +=
            PRESSURE_TELEMETRY_ALPHA *
            (p_raw - pressureControlFiltered);
      }
      shot.smoothedPressure = pressureControlFiltered;

      shot.pressureChangeSpeed = (shot.smoothedPressure - prev_pSmooth) / dt_s;
    }
#else
    shot.pressure = 0.0f;
    shot.smoothedPressure = 0.0f;
    shot.pressureChangeSpeed = 0.0f;
#endif

    // ---- FLOW (upstream owner estimate, no second local estimator) ----
    {
      const auto pumpModel = PumpPSM::getControllerTelemetry();
      float raw_flow = pumpModel.pumpFlowMlPerS;
      float prev_fSmooth = shot.smoothedPumpFlow;
      shot.flow_gps = raw_flow;
      shot.smoothedPumpFlow = raw_flow; // Already filtered by upstream estimator.

      shot.pumpFlowChangeSpeed = (shot.smoothedPumpFlow - prev_fSmooth) / dt_s;
    }

    // ---- BREW ESTIMATE (filtered block temperature minus calibrated offset) ----
    {
      if (shot.blockTempC == 0.0f && isfinite(shot.temperature)) {
        shot.blockTempC = shot.temperature;
      }
      const bool waterRunning = isShotActive || shot.smoothedPumpFlow > 0.05f;
      ThermalModelState th = thermal.update(shot.blockTempC, shot.smoothedPumpFlow,
                                            waterRunning, dt_s);
      if (isfinite(th.blockTempC)) shot.blockTempC = th.blockTempC;
      if (isfinite(th.pathTempEstC)) shot.pathTempEstC = th.pathTempEstC;
      if (isfinite(th.brewTempEstC)) shot.brewTempEstC = th.brewTempEstC;
      shot.thermalLoadG = th.recentWaterLoadG;
    }

    // ---- FLOW VISUAL SMOOTHING (pressure already filtered in ADC task) ----
    {
      shot.visualFlow = shot.smoothedPumpFlow;
    }

    // ---- TEMP EXTREMA ----

    if (isShotActive) {
      float shotTemp = shot.brewTempEstC > 0.0f ? shot.brewTempEstC : shot.temperature;
      if (shotTemp > shot.tempMax) shot.tempMax = shotTemp;
      if (shotTemp < shot.tempMin) shot.tempMin = shotTemp;
    }

    lastTickMs = nowMs;
  }

} // namespace SensorReader
