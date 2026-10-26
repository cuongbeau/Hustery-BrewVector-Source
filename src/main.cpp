// Main owns control and persistence; all visual UI is on the separate HMI.
#include <Arduino.h>
#include "src/hardware/heater_control.h"
#include "src/hardware/pump_control.h"
#include "src/control/heater_flow_model.h"
#include "interfaces/system_state.h"
#include "src/model/flow_model.h"
#include "src/model/thermal_model.h"
#include "src/app/orchestrator.h"
#include "src/app/quick_yield.h"
#include "src/app/calibration_store.h"
#include "src/control/profile_engine.h"
#include "src/hardware/scale_service.h"
#include "src/communication/hmi_link.h"
#include "src/recovery/recovery_service.h"
#include "src/recovery/recovery_portal.h"

ShotData shot;
LastShotResult lastShot;
FlowEstimator flow;
ThermalModel thermal;
PressureCal gPress;
FlowCal gFlow;
bool g_flowCalNeedUpdate = false;
ProfileJson gProfiles[MAX_PROFILES_JSON];
uint8_t gProfilesCount = 0;
volatile float targetYieldGrams = 0.0f;

void setup() {
  Heater::begin();
  Serial.begin(115200);
  delay(200);
  Serial.println("[Main] HMI-only display; local encoder and Recovery enabled");
  ScaleService::begin();
  HmiLink::begin();
  loadPressureCal();
  loadFlowCal();
  loadThermalCal();
  loadProfilesJson();
  ProfilesRuntime::rebuild();
  RecoveryService::begin();
  Orchestrator::init();
  Serial.println("[Setup] DONE");
}

void loop() {
  const State heaterState = Orchestrator::getState();
  const auto pump = PumpPSM::getControllerTelemetry();
  const bool flowValid = millis()-pump.sampledAtMs <= 250U && pump.pressureFresh &&
      millis()-pump.pressureSampledAtMs <= 250U &&
      std::isfinite(pump.rawPressureBar) && pump.rawPressureBar >= 0 && pump.rawPressureBar < 9.8f &&
      std::isfinite(pump.appliedPowerPercent) && pump.appliedPowerPercent > 0 && pump.appliedPowerPercent <= 100;
  const bool brewingWater = heaterState == PROFILE_RUN || heaterState == FLUSH_RUN || heaterState == HOTWATER_RUN;
  Heater::heartbeat(brewingWater || heaterState == CALIBRATION_RUN,
                    HeaterFlowModel::estimate(pump.appliedPowerPercent, pump.rawPressureBar),
                    brewingWater && pump.valveOpen, flowValid,
                    pump.rawPressureBar, pump.appliedPowerPercent);
  if (HmiLink::firmwareUpdateActive() || RecoveryPortal::updateActive()) Heater::inhibit();
  else Heater::releaseInhibit();
  QuickYield::poll();
  if (!HmiLink::firmwareUpdateActive() && !RecoveryPortal::updateActive())
    Orchestrator::update();
  HmiLink::poll();
  // Keep recovery HTTP alive during OTA even when control is paused.
  RecoveryService::poll();
}
