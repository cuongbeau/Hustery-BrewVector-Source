#pragma once
#include <Arduino.h>
#include <atomic>
#include "protocol/thermal_calibration.h"

struct ThermalModelState {
  float blockTempC = NAN;
  float pathTempEstC = NAN; // Legacy telemetry alias of brewTempEstC.
  float brewTempEstC = NAN;
  float recentWaterLoadG = 0.0f; // Retained for telemetry compatibility; always zero.
};

class ThermalModel {
public:
  void begin();
  void reset(float blockTempC = NAN);
  ThermalModelState update(float blockTempC, float flowGps, bool waterRunning, float dtS);
  const ThermalModelState& state() const { return st; }
  float offsetC() const { return offset_.load(std::memory_order_relaxed); }
  bool setOffsetC(float value) {
    if (!ThermalCalibration::valid(value)) return false;
    offset_.store(value, std::memory_order_relaxed);
    return true;
  }
private:
  // Calibration writes run on Main loop; SensorReader owns the estimate state.
  std::atomic<float> offset_{ThermalCalibration::DefaultOffsetC};
  ThermalModelState st;
};
