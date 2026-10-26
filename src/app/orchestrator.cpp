// File: src/app/orchestrator.cpp
// Trái tim hệ thống — tổng hợp từ control_logic.h
// Refactor: 2026-04-20

#include "orchestrator.h"
#include "quick_yield.h"
#include "state_machine.h"
#include "calibration_store.h"

#include "interfaces/system_state.h"

#include "src/hardware/pump_control.h"
#include "src/hardware/heater_control.h"
#include "src/hardware/encoder.h"
#include "src/hardware/sensor_reader.h"
#include "src/hardware/ads1115_interface.h"
#include "src/hardware/valve_sense.h"
#include "src/hardware/relay_driver.h"
#include "src/hardware/scale_service.h"

#include "src/model/flow_model.h"
#include "src/model/predictive_gate_learning.h"
#include "src/model/thermal_model.h"

#include "src/control/pressure_flow_control.h"
#include "src/control/profile_engine.h"
#include "src/control/shot_aggregator.h"
#include "src/communication/hmi_link.h"

namespace Orchestrator {

  // ====== Internal state ======
  // The pump owner reads this only to validate a manual stopped PSM reset.
  // Aligned 32-bit loads/stores are atomic on ESP32 and cannot block while the
  // pump task holds its command lock. Volatile forces each cross-core read.
  static volatile State state = IDLE;
  static_assert(sizeof(State) == sizeof(uint32_t) && alignof(State) >= alignof(uint32_t),
                "Pump diagnostic requires atomic 32-bit state reads");
  static unsigned long shotStart = 0;
  static int manualDuty = 100;
  static int manualStep = 2;
  static bool shotFinalized = false;
  static bool shotPreparing = false;

  static uint32_t flushStopMs = 0;
  static uint32_t flushStartedMs = 0;
  static uint32_t calibrationStartedMs = 0;
  static constexpr uint32_t CALIBRATION_MAX_MS = 12000;
  static constexpr int CALIBRATION_POWER_STEP = 5;

  static HmiProtocol::ShotStopReason protocolStopReason(const char* reason) {
    if (!reason) return HmiProtocol::ShotStopReason::Unknown;
    if (strcmp(reason, "profile-stop") == 0)
      return HmiProtocol::ShotStopReason::Profile;
    if (strcmp(reason, "brew-stop") == 0)
      return HmiProtocol::ShotStopReason::BrewSwitch;
    if (strcmp(reason, "remote-stop") == 0)
      return HmiProtocol::ShotStopReason::Hmi;
    if (strcmp(reason, "manual-stop") == 0)
      return HmiProtocol::ShotStopReason::Manual;
    if (strcmp(reason, "auto-yield") == 0)
      return HmiProtocol::ShotStopReason::AutoYield;
    if (strcmp(reason, "hot-stop") == 0)
      return HmiProtocol::ShotStopReason::HotWaterSwitch;
    return HmiProtocol::ShotStopReason::Unknown;
  }

  // ====== State access ======
  State getState() { return state; }
  
  bool isShotActiveState() {
    return (state == PROFILE_RUN || state == HOTWATER_RUN ||
            state == CALIBRATION_RUN);
  }

  bool pressureCalibrationRunning() { return state == CALIBRATION_RUN; }

  uint32_t flushElapsedMs() {
    return state == FLUSH_RUN ? millis() - flushStartedMs : 0U;
  }

  uint32_t flushDurationMs() {
    if (state != FLUSH_RUN || flushStopMs <= flushStartedMs + 200U) return 5000U;
    return flushStopMs - flushStartedMs - 200U;
  }

  static void beginShotRun(State newState, const char* profileName) {
    if (newState == PROFILE_RUN && !Heater::brewReady()) {
      PumpPSM::setCPS(0.0f);
      Serial.println("[Heater] Brew blocked; use Preheat and wait for ready");
      return;
    }
    QuickYield::beginShot(newState == PROFILE_RUN);
    state = newState;
    shotStart = millis();
    shotFinalized = false;
    shot.profileName = profileName;
    // Keep the pump OFF until the scale prestart finishes or its lease expires.
    PumpPSM::setCPS(0.0f);
    shot.targetPressure = 0;
    shot.pumpPower = 0;
    runInit = false;
    ScaleService::beginShotTare();
    shotPreparing = true;
    ShotAggregator::reset(profileName, newState == PROFILE_RUN);
  }

  static bool finishShot(const char* reason, bool pressRelayIfValveActive) {
    if (shotFinalized || (state != PROFILE_RUN && state != HOTWATER_RUN)) {
      return false;
    }

    const bool stoppedDuringPreparation = shotPreparing;
    ScaleService::endShot();
    shotPreparing = false;
    bool valveActive = ValveSense::isMachineValveActive();
    if (pressRelayIfValveActive && valveActive) {
      RelayDriver::pulse(200);
      Serial.printf("[Logic] STOP relay pulse (%s)\n", reason ? reason : "unknown");
    }

    if (state == PROFILE_RUN && reason &&
        (strcmp(reason, "brew-stop") == 0 ||
         strcmp(reason, "manual-stop") == 0 ||
         strcmp(reason, "remote-stop") == 0)) {
      profileExecutorMarkManualStop();
    }

    const ProfileStepExit profileExit = state == PROFILE_RUN && !stoppedDuringPreparation
                                            ? profileExecutorLastExit()
                                            : ProfileStepExit{};
    const uint8_t stopStep = profileExit.valid
                                 ? profileExit.step
                                 : (state == PROFILE_RUN && pfExec.running
                                        ? pfExec.currentStep
                                        : 0);
    HmiLink::queueShotEnd(protocolStopReason(reason),
                          static_cast<uint8_t>(profileExit.reason), stopStep,
                          valveActive);

    ShotAggregator::finalize(shotStart);
    if (state == PROFILE_RUN) {
      controlPump_End();
    }
    PumpPSM::setCPS(0.0f);
    shot.targetPressure = 0;
    shot.pumpPower = 0;
    state = SHOT_END;
    shotFinalized = true;
    QuickYield::endShot();

    Serial.printf("[Logic] Shot finished (%s, valve=%s)\n",
                  reason ? reason : "unknown", valveActive ? "active" : "off");
    return true;
  }

  // ====== Init ======
  void init() {
    Enc::begin();
    PumpPSM::begin();
    flow.begin();
    thermal.begin();
    ValveSense::init();
    RelayDriver::init();
    SensorReader::init();
    PredictiveGateLearning::begin();
    loadLastShot();

    shot.profileName = profiles[getCurrentProfileIndex()].name;
  }

  // ====== Force Idle ======
  void forceIdle() {
    ScaleService::endShot();
    shotPreparing = false;
    state = IDLE;
    shotFinalized = false;
    shot.shotTime = 0;
    shot.targetPressure = 0;
    shot.pumpPower = 0;
    shot.puckWeak = false;
    shot.shotTooLong = false;
    PumpPSM::setCPS(0.0f);
    flow.reset();
    shot.profileName = profiles[getCurrentProfileIndex()].name;
  }

  void enterSettings() {
    state = SETTINGS_MENU;
  }

  // ====== Shot controls ======
  void startShotFromRemote() {
    if (!Heater::brewReady()) return;
    if (!ValveSense::isMachineValveActive()) {
      RelayDriver::pulse(200);
      Serial.println("[HMI] Request START shot");
    }
  }

  void stopShotFromRemote() {
    if (state == PROFILE_RUN || state == HOTWATER_RUN) {
      finishShot("remote-stop", true);
      Serial.println("[HMI] Request STOP shot");
    } else if (ValveSense::isMachineValveActive()) {
      RelayDriver::pulse(200);
      Serial.println("[HMI] Request STOP valve-only");
    }
  }

  void startOrStopFlushFromRemote(uint32_t ms) {
    if (state == FLUSH_RUN) {
      if (ValveSense::isMachineValveActive()) RelayDriver::pulse(200);
      flushStopMs = 0;
      state = IDLE;
      PumpPSM::setCPS(0.0f);
      shot.pumpPower = 0;
      shot.targetPressure = 0;
      Serial.println("[HMI] Request STOP flush actively");
      return;
    }
    if (!ValveSense::isMachineValveActive() && (state == IDLE || state == SHOT_END)) {
      state = FLUSH_RUN; 
      flushStartedMs = millis();
      RelayDriver::pulse(200);
      flushStopMs = millis() + 200 + ms;
      Serial.printf("[HMI] Request FLUSH for %u ms\n", ms);
    }
  }

  bool startPressureCalibrationFromRemote() {
    if (state != IDLE && state != SHOT_END) return false;
    if (ValveSense::isMachineValveActive()) return false;

    state = CALIBRATION_RUN;
    calibrationStartedMs = millis();
    manualDuty = 100;
    shot.profileName = "PRESSURE CAL";
    shot.targetPressure = 0.0f;
    shot.pumpPower = 0.0f;
    PumpPSM::setCPS(0.0f);
    RelayDriver::pulse(200);
    Serial.println("[CAL] Pressure test start: pump target 100%");
    return true;
  }

  void stopPressureCalibration(const char* reason) {
    if (state != CALIBRATION_RUN) return;
    PumpPSM::setCPS(0.0f);
    shot.pumpPower = 0.0f;
    shot.targetPressure = 0.0f;
    if (ValveSense::isMachineValveActive()) RelayDriver::pulse(200);
    state = IDLE;
    calibrationStartedMs = 0;
    Serial.printf("[CAL] Pressure test stop (%s)\n", reason ? reason : "unknown");
  }

  void adjustPressureCalibrationPower(int encoderSteps) {
    if (state != CALIBRATION_RUN || encoderSteps == 0) return;
    manualDuty = constrain(manualDuty + encoderSteps * CALIBRATION_POWER_STEP,
                           0, 100);
  }

  void startShotManual() {
    beginShotRun(PROFILE_RUN, profiles[getCurrentProfileIndex()].name);
    manualDuty = 50;
  }

  void stopShotManual() {
    finishShot("manual-stop", false);
  }

  void stopActiveShot(const char* reason) {
    finishShot(reason, true);
  }

  // ====== Hot water tick ======
  static void hotWaterTick() {
    int step = Enc::readSteps();
    if (step != 0) manualDuty = constrain(manualDuty + step * manualStep, 0, 100);

    shot.targetPressure = 0;
    PumpPSM::setPercent(manualDuty);
    shot.pumpPower = PumpPSM::getPercent();
  }

  // ====== Control pump tick (100Hz) ======
  static void controlPumpTick() {
    if ((state==PROFILE_RUN || state==HOTWATER_RUN) && millis()-shotStart>=60000U) {
      finishShot("max-time",true);
    }
    if ((state == PROFILE_RUN || state == HOTWATER_RUN) && shotPreparing) {
      if (ScaleService::shotTarePending()) {
        PumpPSM::setCPS(0.0f);
        shot.targetPressure = 0;
        shot.pumpPower = 0;
        controlPump_ServicePowerMode(0.0f, false);
        ShotAggregator::update(state, shotStart);
        return;
      }
      shotPreparing = false;
      ShotAggregator::armScaleForShot();
    }
    if (state == PROFILE_RUN) {
      if (!runInit) controlPump_Begin();

      if (isManualProfile()) {
        int step = Enc::readSteps(); 
        if (step != 0) manualDuty = constrain(manualDuty + step * manualStep, 0, 100);
        shot.targetPressure = 0;
        PumpPSM::setPercent(manualDuty);
        shot.pumpPower = PumpPSM::getPercent();
        controlPump_ServicePowerMode(shot.pumpPower, true);
      } else if (isFullOutProfile()) {
        shot.targetPressure = 0;
        PumpPSM::setPercent(100);
        shot.pumpPower = PumpPSM::getPercent();
        controlPump_ServicePowerMode(shot.pumpPower, true);
      } else {
        controlPump_RunTick(); 
      }

    } else if (state == HOTWATER_RUN) {
      hotWaterTick();
      controlPump_ServicePowerMode(shot.pumpPower, true);

    } else if (state == FLUSH_RUN) {
      PumpPSM::setPercent(100);
      shot.pumpPower = 100;
      shot.targetPressure = 0;
      controlPump_ServicePowerMode(shot.pumpPower, true);

    } else if (state == CALIBRATION_RUN) {
      shot.targetPressure = 0.0f;
      if (ValveSense::isMachineValveActive()) {
        PumpPSM::setPercent(manualDuty);
        shot.pumpPower = PumpPSM::getPercent();
        controlPump_ServicePowerMode(shot.pumpPower, true);
      } else {
        PumpPSM::setCPS(0.0f);
        shot.pumpPower = 0.0f;
        controlPump_ServicePowerMode(0.0f, false);
      }

    } else {
      if (runInit) controlPump_End();
      shot.targetPressure = 0;
      PumpPSM::setCPS(0.0f);
      shot.pumpPower = 0;
      controlPump_ServicePowerMode(0.0f, false);
    }

    ShotAggregator::update(state, shotStart);
  }

  // ====== Process valve events ======
  static void processValveEvent(ValveEvent evt) {
    switch (evt) {
      case ValveEvent::BREW_START:
        if (state != PROFILE_RUN && state != HOTWATER_RUN && state != FLUSH_RUN &&
            state != CALIBRATION_RUN) {
          beginShotRun(PROFILE_RUN, profiles[getCurrentProfileIndex()].name);
          manualDuty = 50;
        }
        break;

      case ValveEvent::BREW_STOP:
        if (state == CALIBRATION_RUN)
          stopPressureCalibration("brew-button");
        else
          finishShot("brew-stop", false);
        break;

      case ValveEvent::HOT_START:
        if (state != PROFILE_RUN) {
          beginShotRun(HOTWATER_RUN, "HOT WATER");
          manualDuty = 50;
          runInit = false;
        }
        break;

      case ValveEvent::HOT_STOP:
        finishShot("hot-stop", false);
        break;

      default:
        break;
    }
  }

  ControlTelemetrySnapshot telemetrySnapshot() {
    ControlTelemetrySnapshot snapshot;
    switch (state) {
      case IDLE:          snapshot.stateName = "IDLE";          break;
      case PROFILE_RUN:   snapshot.stateName = "PROFILE_RUN";   break;
      case HOTWATER_RUN:  snapshot.stateName = "HOTWATER_RUN";  break;
      case SHOT_END:      snapshot.stateName = "SHOT_END";      break;
      case FLUSH_RUN:     snapshot.stateName = "FLUSH_RUN";     break;
      case SETTINGS_MENU: snapshot.stateName = "SETTINGS_MENU"; break;
      case CALIBRATION_RUN: snapshot.stateName = "PRESSURE_CAL"; break;
      default:            snapshot.stateName = "UNKNOWN";       break;
    }

    snapshot.pressureBar = shot.visualPressure;
    snapshot.targetValue = shot.targetPressure;
    snapshot.flowGps = shot.visualFlow;
    // Outside a shot, shot.outGrams is intentionally reset. Publish the live
    // physical scale value when available so HMI tare/setup screens do not
    // misleadingly remain at 0 g.
    snapshot.weightG = ScaleService::isOnline()
                           ? ScaleService::getWeight() : shot.outGrams;
    snapshot.scaleConnected = isPhysicalScaleConnected;
    snapshot.profileName = state == IDLE ? profiles[getCurrentProfileIndex()].name : shot.profileName.c_str();
    if (state == PROFILE_RUN || state == HOTWATER_RUN ||
        state == CALIBRATION_RUN) {
      snapshot.elapsedS = (float)(millis() - shotStart) / 1000.0f;
    } else if (state == FLUSH_RUN) {
      snapshot.elapsedS = (float)(millis() - flushStartedMs) / 1000.0f;
    }
    snapshot.pumpPercent = shot.pumpPower;

    if (state == PROFILE_RUN && pfExec.running) {
      snapshot.currentStep = pfExec.currentStep + 1;
      snapshot.totalSteps = profiles[pfExec.profileIndex].stepCount;
      if (pfExec.currentStep < profiles[pfExec.profileIndex].stepCount) {
        snapshot.controlMode = profiles[pfExec.profileIndex].steps[pfExec.currentStep].mode;
        snapshot.phaseName = profiles[pfExec.profileIndex].steps[pfExec.currentStep].name;
        snapshot.stepMaxMs = (uint32_t)profiles[pfExec.profileIndex].steps[pfExec.currentStep].timeoutMs;
      }
    } else if (state == FLUSH_RUN) {
      snapshot.currentStep = 1;
      snapshot.totalSteps = 1;
      snapshot.controlMode = MODE_DUTY;
      snapshot.phaseName = "FLUSH";
      snapshot.stepMaxMs = flushStopMs > flushStartedMs
          ? flushStopMs - flushStartedMs : 5000;
    } else if (state == CALIBRATION_RUN) {
      snapshot.currentStep = 1;
      snapshot.totalSteps = 1;
      snapshot.controlMode = MODE_DUTY;
      snapshot.phaseName = "PRESSURE CAL";
      snapshot.stepMaxMs = CALIBRATION_MAX_MS;
    }
    if (shotPreparing) snapshot.phaseName = "SCALE PREP";
    return snapshot;
  }

  // ====== MAIN LOGIC LOOP ======
  void update() {
    if (g_flowCalNeedUpdate) {
      g_flowCalNeedUpdate = false;
      flow.begin();
      PumpPSM::reloadPumpModel();
    }

    ADS1115_Interface::updateFast(); 
    PumpPSM::service();

    static uint32_t lastTickMs = 0;
    if (millis() - lastTickMs >= 10) {
      lastTickMs = millis();
      
      isPhysicalScaleConnected = ScaleService::isOnline();
      SensorReader::update(shot, isShotActiveState(), shotStart);
      
      ValveEvent evt = ValveSense::update();
      if (evt != ValveEvent::NONE) {
        processValveEvent(evt);
      }
      
      controlPumpTick();
      StateMachine::update();
      // ====== IDLE PERSISTENCE (Adaptive Data) ======
      static bool needsSave = false;
      static uint32_t saveAfterMs = 0;
      static State previousPersistenceState = IDLE;
      
      if (state == SHOT_END && previousPersistenceState != SHOT_END) {
        needsSave = true;
        saveAfterMs = millis() + 3000U;
      }
      
      if (needsSave && (state == SHOT_END || state == IDLE) &&
          static_cast<int32_t>(millis() - saveAfterMs) >= 0) {
        saveLastShot();
        persistControlAdaptationIfNeeded();
        PredictiveGateLearning::persistIfNeeded();
        needsSave = false;
        Serial.println("[Adaptive] Control state saved to SPIFFS");
      }
      previousPersistenceState = state;
    }

    // Flush interrupt (van đóng vật lý)
    if (state == FLUSH_RUN && !ValveSense::isMachineValveActive()) {
      if (millis() - flushStartedMs > 1000) {
        state = IDLE;
        flushStopMs = 0;
        PumpPSM::setCPS(0.0f);
        Serial.println("[Logic] Flush interrupted physically (Opto OFF)");
      }
    }

    // Relay pulse handler
    RelayDriver::service();

    // Flush timer handler
    if (flushStopMs > 0 && millis() >= flushStopMs) {
      if (ValveSense::isMachineValveActive()) RelayDriver::pulse(200);
      flushStopMs = 0;
      if (state == FLUSH_RUN) state = IDLE;
    }

    if (state == CALIBRATION_RUN && calibrationStartedMs != 0 &&
        millis() - calibrationStartedMs >= CALIBRATION_MAX_MS) {
      stopPressureCalibration("timeout");
    }

  }

} // namespace Orchestrator

// ====== Global stopShotActively ======
void stopShotActively(const char* reason) {
  Orchestrator::stopActiveShot(reason);
}
