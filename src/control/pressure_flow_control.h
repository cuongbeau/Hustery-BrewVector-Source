// File: src/control/pressure_flow_control.h
// Version: 2026-09-04_v22_DimmedPumpArchitecture
// Profile router/debug facade; pump control is owned by DimmedPumpCore.

#ifndef PRESSURE_FLOW_CONTROL_H
#define PRESSURE_FLOW_CONTROL_H

#include <Arduino.h>
#include "interfaces/profile_types.h"

// Persistent control state (extern for orchestrator access)
extern bool runInit;

// Last controller decision, retained as plain numbers so transport/logging can
// sample it without participating in the control calculation.
struct ControlDebugSnapshot {
  float commandTarget = 0.0f;
  float controllerTargetBar = 0.0f;
  float controllerPressureBar = 0.0f;
  float controllerPressureRateBarPerS = 0.0f;
  float pumpFlowShadowGps = 0.0f;
  float puckFlowShadowGps = 0.0f;
  float basePct = 0.0f;
  float pTermPct = 0.0f;
  float iTermPct = 0.0f;
  float dampingPct = 0.0f;
  float minPct = 0.0f;
  float maxPct = 0.0f;
  float cpsRaw = 0.0f;
  float cpsFinal = 0.0f;
  float pressureLimitBar = -1.0f;
  float flowLimitMlPerS = -1.0f;
  uint16_t flags = 0;
  uint8_t mode = 0;
  uint8_t stepIndex = 0;
};

enum ControlDebugFlags : uint16_t {
  CTRLDBG_PRIMING = 1U << 0,
  CTRLDBG_LOW_PRESSURE_FLOOR = 1U << 1,
  CTRLDBG_HIGH_PRESSURE_PUSH = 1U << 2,
  CTRLDBG_OVERSHOOT_BRAKE = 1U << 3,
  CTRLDBG_MIN_CLAMP = 1U << 4,
  CTRLDBG_MAX_CLAMP = 1U << 5,
  CTRLDBG_SLEW_UP = 1U << 6,
  CTRLDBG_SLEW_DOWN = 1U << 7,
  CTRLDBG_PRESSURE_BUMPER = 1U << 8,
  CTRLDBG_FLOW_MODE = 1U << 9,
  CTRLDBG_DUTY_MODE = 1U << 10,
  CTRLDBG_HIGH_FULL_PUSH = 1U << 11,
  CTRLDBG_FLOW_PRESSURE_LIMIT = 1U << 12,
  CTRLDBG_INTEGRATOR_HOLD = 1U << 13,
  CTRLDBG_STEP_I_RESET = 1U << 14,
  CTRLDBG_POSITIVE_I_RELEASE = 1U << 15,
};

float computePumpCps(float targetPressure, float flowRestriction, StepMode mode);
void  controlPump_Begin();
void  controlPump_End();
void  controlPump_RunTick();
void  controlPump_ServicePowerMode(float powerPercent, bool valveOpen);
void  persistControlAdaptationIfNeeded();
ControlDebugSnapshot getControlDebugSnapshot();

#endif // PRESSURE_FLOW_CONTROL_H
