// File: src/hardware/pump_control.cpp
// Version: 2026-09-04_07 (DimmedPump architecture)
// One owner contains the upstream PressureController and PSM, and runs them
// from a dedicated 30 ms task to isolate pump timing.

#include "pump_control.h"

#include <PSM.h>
#include <PressureController/PressureController.h>

#include "config/hardware_pins.h"
#include "interfaces/calibration_types.h"
#include "src/hardware/ads1115_interface.h"
#include "src/app/orchestrator.h"
#include "src/hardware/valve_sense.h"

namespace PumpPSM {

namespace {

constexpr float kControlPeriodS = 0.030f;
constexpr uint32_t kControlPeriodMs = 30U;
constexpr float kHardPressureCutoffBar = 13.5f;

struct PumpCommand {
  ControlMode mode = ControlMode::Power;
  float powerPercent = 0.0f;
  float pressureBar = 0.0f;
  float flowMlPerS = 0.0f;
  bool valveOpen = false;
  bool tareRequested = false;
  uint32_t tareGeneration = 0;
  bool modelReloadRequested = false;
};

static portMUX_TYPE g_stateMux = portMUX_INITIALIZER_UNLOCKED;
static PumpCommand g_command;
static bool g_adcProbeHold = false, g_adcProbeAck = false;
static StoppedResetState g_stoppedResetState = StoppedResetState::None;
static uint32_t g_stoppedResetRequestedMs = 0;
static ControllerTelemetry g_telemetry;
static DiagnosticTelemetry g_diagnostics;
static volatile uint32_t g_acceptedEventsInWindow = 0;
static volatile float g_measuredCps = 0.0f;
static volatile float g_zcHz = 0.0f;
static portMUX_TYPE g_taskTimingMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t g_taskGapMinUs = UINT32_MAX;
static uint32_t g_taskGapMaxUs = 0;
static uint32_t g_taskLateCount = 0;

class DimmedPumpCore {
 public:
  DimmedPumpCore()
      : psm_(DIMMER_ZC_PIN, DIMMER_PSM_PIN, 100, FALLING, 1, 4),
        pressureController_(kControlPeriodS, &pressureTargetBar_,
                            &flowTargetMlPerS_, &currentPressureBar_,
                            &controllerPowerPercent_, &valveStatus_) {
    psm_.set(0);
    configurePumpModel();
  }

  void setup() {
    const unsigned int rawEventHz = psm_.cps();
    g_zcHz = static_cast<float>(rawEventHz);
    if (rawEventHz > 70U) psm_.setDivider(2);
    psm_.set(0);
    psm_.resetCounter();

    BaseType_t created = xTaskCreate(loopTask, "DimmedPump::loop",
                                     configMINIMAL_STACK_SIZE * 4, this, 1,
                                     &taskHandle_);
    if (created != pdPASS) {
      taskHandle_ = nullptr;
      Serial.println("[Pump] FAIL creating DimmedPump task");
    }
  }

  void loop() {
    PumpCommand command;
    bool adcHeld;
    portENTER_CRITICAL(&g_stateMux);
    if (g_stoppedResetState == StoppedResetState::Pending) {
      // Recheck at execution; never interrupt an active mode, valve or shot.
      // Hold the command lock through reset so a new command cannot interleave.
      const bool safe = millis() - g_stoppedResetRequestedMs < 250U &&
          Orchestrator::getState() == IDLE &&
          !ValveSense::isMachineValveActive() &&
          digitalRead(HOTWATER_SENSE_PIN) == HIGH &&
          g_command.mode == ControlMode::Power &&
          g_command.powerPercent == 0.0f && !g_command.valveOpen &&
          appliedPowerPercent_ == 0.0f;
      g_stoppedResetState = safe && psm_.resetStoppedState()
          ? StoppedResetState::Applied : StoppedResetState::Rejected;
    }
    command = g_command;
    adcHeld = g_adcProbeHold;
    g_command.tareRequested = false;
    g_command.modelReloadRequested = false;
    portEXIT_CRITICAL(&g_stateMux);

    if (adcHeld) {
      // Preserve tare/reload handling, but never run a pressure/flow demand
      // against a frozen diagnostic pressure sample.
      command.mode = ControlMode::Power;
      command.powerPercent = 0;
      command.valveOpen = false;
    }

    const auto sensor = ADS1115_Interface::getPressureSnapshot();
    const float pressure = sensor.valid ? sensor.rawBar : 0.0f;
    currentPressureBar_ = constrain(pressure, 0.0f, 15.0f);

    if (command.modelReloadRequested) configurePumpModel();
    if (command.tareRequested) {
      pressureController_.tare();
      pressureController_.reset();
      pumpedWaterMl_ = 0.0f;
      tareGeneration_ = command.tareGeneration;
    }

    mode_ = command.mode;
    valveStatus_ = command.valveOpen ? 1 : 0;
    pressureTargetBar_ = max(0.0f, command.pressureBar);
    flowTargetMlPerS_ = max(0.0f, command.flowMlPerS);

    if (mode_ == ControlMode::Power) {
      appliedPowerPercent_ = constrain(command.powerPercent, 0.0f, 100.0f);
      controllerPowerPercent_ = appliedPowerPercent_;
      pressureTargetBar_ = 0.0f;
      flowTargetMlPerS_ = 0.0f;
    }

    pressureController_.update(
        static_cast<PressureController::ControlMode>(mode_));
    if (mode_ != ControlMode::Power) {
      appliedPowerPercent_ =
          constrain(controllerPowerPercent_, 0.0f, 100.0f);
    }

    // Independent hardware safety boundary around the upstream controller.
    if (currentPressureBar_ > kHardPressureCutoffBar) {
      appliedPowerPercent_ = 0.0f;
    }

    psm_.set(static_cast<unsigned int>(appliedPowerPercent_));
    currentPumpFlowMlPerS_ = pressureController_.getPumpFlowRate();
    pumpedWaterMl_ += currentPumpFlowMlPerS_ * kControlPeriodS;

    ControllerTelemetry telemetry;
    telemetry.mode = mode_;
    telemetry.pressureTargetBar = pressureTargetBar_;
    telemetry.flowTargetMlPerS = flowTargetMlPerS_;
    telemetry.rawPressureBar = currentPressureBar_;
    telemetry.controllerFilteredPressureBar = pressureController_.getFilteredPressure();
    telemetry.appliedPowerPercent = appliedPowerPercent_;
    telemetry.controllerPowerPercent = controllerPowerPercent_;
    telemetry.pumpFlowMlPerS = currentPumpFlowMlPerS_;
    telemetry.puckFlowMlPerS = pressureController_.getCoffeeFlowRate();
    telemetry.puckResistance = pressureController_.getPuckResistance();
    telemetry.pumpedWaterMl = pumpedWaterMl_;
    telemetry.coffeeOutputMl = pressureController_.getCoffeeOutputEstimate();
    telemetry.tareGeneration = tareGeneration_;
    telemetry.sampledAtMs = millis();
    telemetry.pressureSampledAtMs = sensor.sampledAtMs;
    telemetry.pressureFresh = sensor.valid && telemetry.sampledAtMs-sensor.sampledAtMs <= 250U;
    telemetry.valveOpen = command.valveOpen && !adcHeld;
    portENTER_CRITICAL(&g_stateMux);
    g_telemetry = telemetry;
    g_adcProbeAck = adcHeld && g_adcProbeHold && appliedPowerPercent_ == 0;
    portEXIT_CRITICAL(&g_stateMux);
  }

  PSM& psm() { return psm_; }

 private:
  static void loopTask(void* arg) {
    auto* pump = static_cast<DimmedPumpCore*>(arg);
    TickType_t lastWake = xTaskGetTickCount();
    uint32_t lastLoopUs = micros();
    while (true) {
      const uint32_t nowUs = micros();
      const uint32_t gapUs = nowUs - lastLoopUs;
      lastLoopUs = nowUs;
      portENTER_CRITICAL(&g_taskTimingMux);
      if (gapUs < g_taskGapMinUs) g_taskGapMinUs = gapUs;
      if (gapUs > g_taskGapMaxUs) g_taskGapMaxUs = gapUs;
      if (gapUs > 45000U) ++g_taskLateCount;
      portEXIT_CRITICAL(&g_taskTimingMux);
      pump->loop();
      xTaskDelayUntil(&lastWake, pdMS_TO_TICKS(kControlPeriodMs));
    }
  }

  void configurePumpModel() {
    // Affine full-drive pump model calibrated at 1 and 9 bar.
    pressureController_.setPumpFlowCoeff(gFlow.q1MlPerS, gFlow.q9MlPerS);
    pressureController_.setPumpSlipPolyCoeffs(0.0f, 0.0f, 0.0f, 0.0f);
  }

  PSM psm_;
  uint32_t tareGeneration_ = 0;
  float pressureTargetBar_ = 0.0f;
  float flowTargetMlPerS_ = 0.0f;
  float currentPressureBar_ = 0.0f;
  float controllerPowerPercent_ = 0.0f;
  float appliedPowerPercent_ = 0.0f;
  float currentPumpFlowMlPerS_ = 0.0f;
  float pumpedWaterMl_ = 0.0f;
  int valveStatus_ = 0;
  ControlMode mode_ = ControlMode::Power;
  PressureController pressureController_;
  TaskHandle_t taskHandle_ = nullptr;
};

static DimmedPumpCore* g_pump = nullptr;

void updateCommand(ControlMode mode, float powerPercent, float pressureBar,
                   float flowMlPerS) {
  portENTER_CRITICAL(&g_stateMux);
  g_command.mode = mode;
  g_command.powerPercent = constrain(powerPercent, 0.0f, 100.0f);
  g_command.pressureBar = max(0.0f, pressureBar);
  g_command.flowMlPerS = max(0.0f, flowMlPerS);
  portEXIT_CRITICAL(&g_stateMux);
}

}  // namespace

// Strong implementation of PSM.Library's weak callback. It is telemetry-only:
// it runs after the library's own 4 ms interrupt filter and never touches the
// control pin, divider or accumulator.
void recordAcceptedZeroCross() {
  g_acceptedEventsInWindow++;
}

void begin() {
  g_command = PumpCommand{};
  g_telemetry = ControllerTelemetry{};
  g_diagnostics = DiagnosticTelemetry{};
  static DimmedPumpCore pump;
  g_pump = &pump;
  g_pump->setup();

  // Discard the one-second auto-detection activity from PSM::cps(); runtime
  // diagnostic windows must describe only normal operation.
  (void)g_pump->psm().takeDiagnostics();

  noInterrupts();
  g_acceptedEventsInWindow = 0;
  interrupts();
  g_measuredCps = 0.0f;
}

void service() {
  if (g_pump == nullptr) return;

  static uint32_t lastMeasureMs = millis();
  static long lastClickCounter = 0;
  const uint32_t nowMs = millis();
  const uint32_t elapsedMs = nowMs - lastMeasureMs;
  if (elapsedMs < 1000U) return;

  uint32_t acceptedEvents;
  noInterrupts();
  acceptedEvents = g_acceptedEventsInWindow;
  g_acceptedEventsInWindow = 0;
  interrupts();

  const long clickCounter = g_pump->psm().getCounter();
  const long clickDelta = clickCounter - lastClickCounter;
  lastClickCounter = clickCounter;

  const float elapsedS = static_cast<float>(elapsedMs) * 0.001f;
  g_zcHz = acceptedEvents / elapsedS;
  g_measuredCps = clickDelta > 0 ? clickDelta / elapsedS : 0.0f;
  lastMeasureMs = nowMs;

  const PSM::Diagnostics psmDiagnostics = g_pump->psm().takeDiagnostics();
  DiagnosticTelemetry diagnostics;
  diagnostics.sequence = g_diagnostics.sequence + 1U;
  diagnostics.windowMs = elapsedMs;
  diagnostics.rawEdges = psmDiagnostics.rawEdges;
  diagnostics.acceptedEdges = psmDiagnostics.acceptedEdges;
  diagnostics.rejectedEdges = psmDiagnostics.rejectedEdges;
  diagnostics.rawDtMinUs = psmDiagnostics.rawDtMinUs;
  diagnostics.acceptedDtMinUs = psmDiagnostics.acceptedDtMinUs;
  diagnostics.acceptedDtAverageUs = psmDiagnostics.acceptedDtAverageUs;
  diagnostics.acceptedDtMaxUs = psmDiagnostics.acceptedDtMaxUs;
  diagnostics.acceptedShortIntervals =
      psmDiagnostics.acceptedShortIntervals;
  diagnostics.acceptedLongIntervals =
      psmDiagnostics.acceptedLongIntervals;
  diagnostics.decisions = psmDiagnostics.decisions;
  diagnostics.gateOnDecisions = psmDiagnostics.gateOnDecisions;
  diagnostics.gateTransitions = psmDiagnostics.gateTransitions;
  diagnostics.gateHighMinUs = psmDiagnostics.gateHighMinUs;
  diagnostics.gateHighMaxUs = psmDiagnostics.gateHighMaxUs;
  diagnostics.gateLowMinUs = psmDiagnostics.gateLowMinUs;
  diagnostics.gateLowMaxUs = psmDiagnostics.gateLowMaxUs;
  diagnostics.patternLow = psmDiagnostics.patternLow;
  diagnostics.patternHigh = psmDiagnostics.patternHigh;
  diagnostics.valueChanges = psmDiagnostics.valueChanges;
  diagnostics.maxOnRun = psmDiagnostics.maxOnRun;
  diagnostics.maxOffRun = psmDiagnostics.maxOffRun;
  diagnostics.valueMin = psmDiagnostics.valueMin;
  diagnostics.valueMax = psmDiagnostics.valueMax;
  diagnostics.patternBits = psmDiagnostics.patternBits;
  portENTER_CRITICAL(&g_taskTimingMux);
  diagnostics.taskGapMinUs =
      g_taskGapMinUs == UINT32_MAX ? 0 : g_taskGapMinUs;
  diagnostics.taskGapMaxUs = g_taskGapMaxUs;
  diagnostics.taskLateCount = g_taskLateCount;
  g_taskGapMinUs = UINT32_MAX;
  g_taskGapMaxUs = 0;
  g_taskLateCount = 0;
  portEXIT_CRITICAL(&g_taskTimingMux);
  portENTER_CRITICAL(&g_stateMux);
  g_diagnostics = diagnostics;
  portEXIT_CRITICAL(&g_stateMux);

  const ControllerTelemetry telemetry = getControllerTelemetry();
  if (telemetry.appliedPowerPercent > 0.0f) {
    Serial.printf("[DimmedPump] zc=%.1fHz power=%.1f%% actual=%.1fCPS mode=%u\n",
                  g_zcHz, telemetry.appliedPowerPercent, g_measuredCps,
                  static_cast<unsigned int>(telemetry.mode));
  }
}

void setPower(float percent) {
  updateCommand(ControlMode::Power, percent, 0.0f, 0.0f);
}

void setPressureTarget(float pressureBar, float flowLimitMlPerS) {
  updateCommand(ControlMode::Pressure, 0.0f, pressureBar, flowLimitMlPerS);
}

void setFlowTarget(float flowMlPerS, float pressureLimitBar) {
  updateCommand(ControlMode::Flow, 0.0f, pressureLimitBar, flowMlPerS);
}

void setValveState(bool open) {
  portENTER_CRITICAL(&g_stateMux);
  g_command.valveOpen = open;
  portEXIT_CRITICAL(&g_stateMux);
}

uint32_t tare() {
  portENTER_CRITICAL(&g_stateMux);
  g_command.tareRequested = true;
  if (++g_command.tareGeneration == 0) ++g_command.tareGeneration;
  const uint32_t generation = g_command.tareGeneration;
  portEXIT_CRITICAL(&g_stateMux);
  return generation;
}

void reloadPumpModel() {
  portENTER_CRITICAL(&g_stateMux);
  g_command.modelReloadRequested = true;
  portEXIT_CRITICAL(&g_stateMux);
}

bool requestStoppedReset() {
  if (Orchestrator::getState() != IDLE ||
      ValveSense::isMachineValveActive() ||
      digitalRead(HOTWATER_SENSE_PIN) != HIGH) return false;
  portENTER_CRITICAL(&g_stateMux);
  const bool safe = g_pump != nullptr && !g_adcProbeHold &&
      g_stoppedResetState != StoppedResetState::Pending &&
      g_command.mode == ControlMode::Power &&
      g_command.powerPercent == 0.0f && !g_command.valveOpen &&
      g_telemetry.appliedPowerPercent == 0.0f;
  if (safe) {
    g_stoppedResetRequestedMs = millis();
    g_stoppedResetState = StoppedResetState::Pending;
  }
  portEXIT_CRITICAL(&g_stateMux);
  return safe;
}

StoppedResetState stoppedResetState() {
  portENTER_CRITICAL(&g_stateMux);
  const StoppedResetState result = g_stoppedResetState;
  portEXIT_CRITICAL(&g_stateMux);
  return result;
}

bool holdForAdcProbe() {
  if (Orchestrator::getState() != IDLE || ValveSense::isMachineValveActive() ||
      digitalRead(CURRENT_SENSE_PIN) != HIGH || digitalRead(HOTWATER_SENSE_PIN) != HIGH) return false;
  portENTER_CRITICAL(&g_stateMux);
  const bool safe = g_pump && !g_adcProbeHold &&
      g_stoppedResetState != StoppedResetState::Pending &&
      g_command.mode == ControlMode::Power && g_command.powerPercent == 0 &&
      !g_command.valveOpen && g_telemetry.appliedPowerPercent == 0;
  if (safe) { g_adcProbeHold = true; g_adcProbeAck = false; }
  portEXIT_CRITICAL(&g_stateMux);
  return safe;
}
bool adcProbeHoldAcknowledged() {
  portENTER_CRITICAL(&g_stateMux);
  const bool ack = g_adcProbeHold && g_adcProbeAck;
  portEXIT_CRITICAL(&g_stateMux);
  return ack;
}
void releaseAdcProbeHold() {
  portENTER_CRITICAL(&g_stateMux);
  g_adcProbeHold = false; g_adcProbeAck = false;
  portEXIT_CRITICAL(&g_stateMux);
}

void cancelStoppedReset() {
  portENTER_CRITICAL(&g_stateMux);
  if (g_stoppedResetState == StoppedResetState::Pending)
    g_stoppedResetState = StoppedResetState::Rejected;
  portEXIT_CRITICAL(&g_stateMux);
}

void setCPS(float cps) {
  if (!isfinite(cps) || cps < 0.0f) cps = 0.0f;
  if (cps > static_cast<float>(MAX_CPS))
    cps = static_cast<float>(MAX_CPS);

  // Same effective conversion as the referenced pump model
  // _psm.set(static_cast<int>(_power)).
  setPower(cps * 2.0f);
}

float getCPS() {
  return g_measuredCps;
}

float getZCHz() {
  return g_zcHz;
}

void setPercent(int percent) {
  percent = constrain(percent, 0, 100);
  setPower(static_cast<float>(percent));
}

int getPercent() {
  ControlMode commandMode;
  float commandedPower;
  portENTER_CRITICAL(&g_stateMux);
  commandMode = g_command.mode;
  commandedPower = g_command.powerPercent;
  portEXIT_CRITICAL(&g_stateMux);
  if (commandMode == ControlMode::Power) {
    return static_cast<int>(lroundf(commandedPower));
  }
  return static_cast<int>(
      lroundf(getControllerTelemetry().appliedPowerPercent));
}

ControllerTelemetry getControllerTelemetry() {
  ControllerTelemetry telemetry;
  portENTER_CRITICAL(&g_stateMux);
  telemetry = g_telemetry;
  portEXIT_CRITICAL(&g_stateMux);
  return telemetry;
}

DiagnosticTelemetry getDiagnosticTelemetry() {
  DiagnosticTelemetry diagnostics;
  portENTER_CRITICAL(&g_stateMux);
  diagnostics = g_diagnostics;
  portEXIT_CRITICAL(&g_stateMux);
  return diagnostics;
}

}  // namespace PumpPSM

void onPSMInterrupt() {
  PumpPSM::recordAcceptedZeroCross();
}
