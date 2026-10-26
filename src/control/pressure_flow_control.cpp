// File: src/control/pressure_flow_control.cpp
// Version: 2026-09-04_v22_DimmedPumpArchitecture
// The local profile executor only produces mode/setpoints. Pressure sensing,
// PressureController execution and PSM actuation are owned by DimmedPumpCore.

#include "pressure_flow_control.h"

#include "interfaces/system_state.h"
#include "src/control/profile_engine.h"
#include "src/hardware/pump_control.h"
#include "src/control/shot_aggregator.h"

bool runInit = false;

namespace {

ControlDebugSnapshot debugSnapshot;

void publishDebug(float commandTarget, float pressureLimit, float flowLimit,
                  uint8_t step, StepMode mode, uint16_t extraFlags = 0) {
  const PumpPSM::ControllerTelemetry telemetry =
      PumpPSM::getControllerTelemetry();

  debugSnapshot = ControlDebugSnapshot{};
  debugSnapshot.commandTarget = commandTarget;
  debugSnapshot.controllerTargetBar = telemetry.pressureTargetBar;
  debugSnapshot.controllerPressureBar = telemetry.rawPressureBar;
  debugSnapshot.pumpFlowShadowGps = telemetry.pumpFlowMlPerS;
  debugSnapshot.puckFlowShadowGps = telemetry.puckFlowMlPerS;
  debugSnapshot.basePct = telemetry.appliedPowerPercent;
  debugSnapshot.maxPct = 100.0f;
  debugSnapshot.cpsRaw =
      telemetry.controllerPowerPercent * 0.01f * PumpPSM::MAX_CPS;
  debugSnapshot.cpsFinal =
      telemetry.appliedPowerPercent * 0.01f * PumpPSM::MAX_CPS;
  debugSnapshot.pressureLimitBar = pressureLimit;
  debugSnapshot.flowLimitMlPerS = flowLimit;
  debugSnapshot.flags = extraFlags;
  if (telemetry.controllerPowerPercent <= 0.0f)
    debugSnapshot.flags |= CTRLDBG_MIN_CLAMP;
  if (telemetry.controllerPowerPercent >= 100.0f)
    debugSnapshot.flags |= CTRLDBG_MAX_CLAMP;
  debugSnapshot.mode = static_cast<uint8_t>(mode);
  debugSnapshot.stepIndex = step;
}

}  // namespace

void persistControlAdaptationIfNeeded() {
  // The pressure controller has no persisted hold-duty state.
}

ControlDebugSnapshot getControlDebugSnapshot() { return debugSnapshot; }

float computePumpCps(float targetPressure, float flowRestriction,
                              StepMode mode) {
  (void)flowRestriction;
  if (mode == MODE_DUTY) {
    return constrain(targetPressure, 0.0f, 100.0f) * 0.01f *
           PumpPSM::MAX_CPS;
  }
  return PumpPSM::getControllerTelemetry().appliedPowerPercent * 0.01f *
         PumpPSM::MAX_CPS;
}

void controlPump_Begin() {
  runInit = true;
  debugSnapshot = ControlDebugSnapshot{};
  PumpPSM::setValveState(true);
  // beginShotRun/ShotAggregator owns one tare for both brew and hot water.
  profileExecutorStart(getCurrentProfileIndex(), shot.visualPressure,
                       shot.flow_gps, shot.pumpPower);
}

void controlPump_End() {
  PumpPSM::setValveState(false);
  PumpPSM::setPower(0.0f);
  runInit = false;
  debugSnapshot = ControlDebugSnapshot{};
  profileExecutorStop();
}

void controlPump_ServicePowerMode(float powerPercent, bool valveOpen) {
  PumpPSM::setValveState(valveOpen);
  PumpPSM::setPower(constrain(powerPercent, 0.0f, 100.0f));
  publishDebug(powerPercent, -1.0f, -1.0f, 0, MODE_DUTY,
               CTRLDBG_DUTY_MODE);
}

extern void stopShotActively(const char* reason);

void controlPump_RunTick() {
  float target = 0.0f;
  float pressureLimit = -1.0f;
  float flowLimit = -1.0f;
  StepMode mode = MODE_PRESSURE;
  if (!profileExecutorTick(shot.visualPressure, ShotAggregator::yieldControlWeight(), shot.flow_gps,
                           shot.waterPumped, shot.pumpPower, target, mode,
                           pressureLimit, flowLimit)) {
    stopShotActively("profile-stop");
    return;
  }

  shot.targetPressure = target;
  const uint8_t step = pfExec.running ? pfExec.currentStep : 0;
  uint16_t flags = 0;

  PumpPSM::setValveState(true);
  if (mode == MODE_DUTY) {
    PumpPSM::setPower(constrain(target, 0.0f, 100.0f));
    flags |= CTRLDBG_DUTY_MODE;
  } else if (mode == MODE_FLOW) {
    PumpPSM::setFlowTarget(max(0.0f, target),
                           pressureLimit > 0.1f ? pressureLimit : 0.0f);
    flags |= CTRLDBG_FLOW_MODE;
    if (pressureLimit > 0.1f) flags |= CTRLDBG_PRESSURE_BUMPER;
  } else {
    PumpPSM::setPressureTarget(max(0.0f, target),
                               flowLimit > 0.1f ? flowLimit : 0.0f);
  }

  shot.pumpPower = PumpPSM::getPercent();
  publishDebug(target, pressureLimit, flowLimit, step, mode, flags);
}
