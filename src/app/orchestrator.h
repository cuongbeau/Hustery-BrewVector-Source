// File: src/app/orchestrator.h
// Trái tim hệ thống — điều phối tất cả module
// Tách từ control_logic.h
// Refactor: 2026-04-20

#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <Arduino.h>
#include "interfaces/shot_types.h"

struct ControlTelemetrySnapshot {
  const char* stateName = "UNKNOWN";
  const char* profileName = "";
  const char* phaseName = "";
  float pressureBar = 0.0f;
  float targetValue = 0.0f;
  float flowGps = 0.0f;
  float weightG = 0.0f;
  float elapsedS = 0.0f;
  float pumpPercent = 0.0f;
  bool scaleConnected = false;
  uint8_t currentStep = 0;
  uint8_t totalSteps = 0;
  uint8_t controlMode = 0;
  uint32_t stepMaxMs = 0;
};

namespace Orchestrator {
  void init();
  void update();
  
  // State access
  State getState();
  bool  isShotActiveState();
  uint32_t flushElapsedMs();
  uint32_t flushDurationMs();
  
  // Validated remote commands from the HMI transport.
  void startShotFromRemote();
  void stopShotFromRemote();
  void startOrStopFlushFromRemote(uint32_t ms = 5000);
  bool startPressureCalibrationFromRemote();
  void stopPressureCalibration(const char* reason = "calibration-stop");
  void adjustPressureCalibrationPower(int encoderSteps);
  bool pressureCalibrationRunning();
  
  // Manual control (encoder)
  void startShotManual();
  void stopShotManual();
  void stopActiveShot(const char* reason = "active-stop");
  void forceIdle();
  void enterSettings();
  
  ControlTelemetrySnapshot telemetrySnapshot();
  
}

// Global function (needed by pressure_flow_control.cpp and shot_aggregator.cpp)
void stopShotActively(const char* reason = "profile-stop");

#endif // ORCHESTRATOR_H
