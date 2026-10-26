// File: src/model/flow_model.cpp
// Version: 2026-09-18_Q1Q9Affine

#include "flow_model.h"

#include "interfaces/calibration_types.h"
#include "src/hardware/pump_control.h"
#include "src/model/pump_flow_calibration.h"

namespace {

float fullDriveFlow(float pressureBar) {
  // Two-point affine full-drive model. Extrapolate
  // below 1 bar so free-flow operation remains continuous.
  const float q1 = gFlow.q1MlPerS;
  const float q9 = gFlow.q9MlPerS;
  return max(0.25f, PumpFlowCalibration::interpolate(
                        q1, q9, max(0.0f, pressureBar)));
}

}  // namespace

void FlowEstimator::begin() {}

void FlowEstimator::reset() {}

float FlowEstimator::getPumpFlowPerClick(float pressure, float cps,
                                         float pStallOverride) {
  (void)cps;
  (void)pStallOverride;  // OPV pressure is not the pump stall pressure.
  return fullDriveFlow(pressure) / PumpPSM::MAX_CPS;
}

float FlowEstimator::getPumpFlow(float cps, float pressure) {
  const float duty = constrain(cps / PumpPSM::MAX_CPS, 0.0f, 1.0f);
  return duty * fullDriveFlow(pressure);
}

float FlowEstimator::getFlowPerClickAtZeroBar(float cps) {
  (void)cps;
  return fullDriveFlow(0.0f) / PumpPSM::MAX_CPS;
}

float FlowEstimator::getRawFPCAtPressure(float pressure, float cps) {
  return getPumpFlowPerClick(pressure, cps);
}

float FlowEstimator::getClicksPerSecondForFlow(float targetFlow,
                                               float pressure,
                                               float pStallOverride) {
  (void)pStallOverride;
  if (targetFlow <= 0.0f) return 0.0f;
  const float available = fullDriveFlow(pressure);
  if (available <= 0.001f) return PumpPSM::MAX_CPS;
  return constrain(targetFlow / available * PumpPSM::MAX_CPS,
                   0.0f, static_cast<float>(PumpPSM::MAX_CPS));
}

float FlowEstimator::update(float pressureBar) {
  const float commandedCps =
      PumpPSM::getPercent() * 0.01f * PumpPSM::MAX_CPS;
  return getPumpFlow(commandedCps, pressureBar);
}
