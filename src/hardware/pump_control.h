// File: src/hardware/pump_control.h
// Version: 2026-09-04_07 (DimmedPump architecture)

#ifndef PUMP_CONTROL_H
#define PUMP_CONTROL_H

#include <Arduino.h>

namespace PumpPSM {

  static const uint8_t MAINS_HZ = 50;
  static const uint8_t CYCLES_PER_SEC = MAINS_HZ;
  static const uint8_t MAX_CPS = 50;

  enum class ControlMode : uint8_t {
    Power = 0,
    Pressure = 1,
    Flow = 2,
  };

  struct ControllerTelemetry {
    ControlMode mode = ControlMode::Power;
    float pressureTargetBar = 0.0f;
    float flowTargetMlPerS = 0.0f;
    float rawPressureBar = 0.0f;
    float controllerFilteredPressureBar = 0.0f; // Internal scalar Kalman, not UI filter.
    float appliedPowerPercent = 0.0f;
    float controllerPowerPercent = 0.0f;
    float pumpFlowMlPerS = 0.0f;
    float puckFlowMlPerS = 0.0f;
    float puckResistance = 0.0f;
    float pumpedWaterMl = 0.0f;
    float coffeeOutputMl = 0.0f;
    uint32_t tareGeneration = 0;
    uint32_t sampledAtMs = 0;
    uint32_t pressureSampledAtMs = 0;
    bool pressureFresh = false, valveOpen = false;
  };

  // One approximately one-second, read-only view of the ZC/PSM timing. The
  // values are copied into Control Trace so a good and bad 50% manual run can
  // be compared without a USB serial connection.
  struct DiagnosticTelemetry {
    uint32_t sequence = 0;
    uint32_t windowMs = 0;
    uint32_t rawEdges = 0;
    uint32_t acceptedEdges = 0;
    uint32_t rejectedEdges = 0;
    uint32_t rawDtMinUs = 0;
    uint32_t acceptedDtMinUs = 0;
    uint32_t acceptedDtAverageUs = 0;
    uint32_t acceptedDtMaxUs = 0;
    uint32_t acceptedShortIntervals = 0;
    uint32_t acceptedLongIntervals = 0;
    uint32_t decisions = 0;
    uint32_t gateOnDecisions = 0;
    uint32_t gateTransitions = 0;
    uint32_t gateHighMinUs = 0;
    uint32_t gateHighMaxUs = 0;
    uint32_t gateLowMinUs = 0;
    uint32_t gateLowMaxUs = 0;
    uint32_t patternLow = 0;
    uint32_t patternHigh = 0;
    uint32_t valueChanges = 0;
    uint32_t taskGapMinUs = 0;
    uint32_t taskGapMaxUs = 0;
    uint32_t taskLateCount = 0;
    uint16_t maxOnRun = 0;
    uint16_t maxOffRun = 0;
    uint16_t valueMin = 0;
    uint16_t valueMax = 0;
    uint8_t patternBits = 0;
  };

  void begin();
  void service();
  // Diagnostic interlock: ADC waits for owner ACK before touching registers.
  bool holdForAdcProbe();
  bool adcProbeHoldAcknowledged();
  void releaseAdcProbeHold();

  // These commands follow the dimmed-pump interface. The dedicated 30 ms pump
  // task is the only place that runs PressureController and applies the
  // resulting power to PSM.
  void setPower(float percent);
  void setPressureTarget(float pressureBar, float flowLimitMlPerS = 0.0f);
  void setFlowTarget(float flowMlPerS, float pressureLimitBar = 0.0f);
  void setValveState(bool open);
  uint32_t tare(); // Returns generation acknowledged in owner telemetry.
  void reloadPumpModel();

  enum class StoppedResetState : uint8_t { None, Pending, Applied, Rejected };
  // Manual diagnostic, requested from Main IDLE only; executed by owner task.
  bool requestStoppedReset();
  StoppedResetState stoppedResetState();
  void cancelStoppedReset();

  void setCPS(float cps);
  float getCPS();
  float getZCHz();

  void setPercent(int percent);
  int getPercent();

  ControllerTelemetry getControllerTelemetry();
  DiagnosticTelemetry getDiagnosticTelemetry();

}  // namespace PumpPSM

#endif  // PUMP_CONTROL_H
