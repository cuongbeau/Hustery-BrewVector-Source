// File: src/control/shot_aggregator.cpp
// Tách từ control_logic.h dòng 211-633
// Refactor: 2026-04-20
// Flow milestone: upstream owner totals; local predictive gate no longer runs.

#include "shot_aggregator.h"
#include "src/app/quick_yield.h"
#include "flow_shot_policy.h"
#include "yield_stop_policy.h"
#include "profile_engine.h"
#include "interfaces/system_state.h"
#include "src/hardware/scale_service.h"
#include "src/hardware/pump_control.h"

// Forward declaration cho stopShotActively (defined trong orchestrator)
extern void stopShotActively(const char* reason);

namespace ShotAggregator {

  static FlowShotPolicy policy;
  static float sumTemp = 0, sumFlow = 0;
  static unsigned long lastTickMs = 0;

  float resolveOutGrams(float flowIntegrated) {
    if (ScaleService::isOnline()) {
      float w = ScaleService::getWeight();
      if (isfinite(w) && w >= 0.0f) return w;
    }
    
    return flowIntegrated; // Display fallback only; never a yield exit input.
  }

  float modelYieldAfterGate() { return sumFlow; }

  // Legacy wire name: now indicates upstream virtual output has started.
  bool predictiveGateOpen() { return sumFlow > 0.0f; }

  float yieldControlWeight() {
    // Until the protocol can prove baseline continuity, reconnect is display
    // only after any observed loss. Never substitute the model for BLE here.
    if (!policy.allowScale(ScaleService::allowedForYield())) return NAN;
    const float weight = ScaleService::getWeight();
    return isfinite(weight) ? max(0.0f, weight) : NAN;
  }

  static void refreshModelTotals() {
    const auto model = PumpPSM::getControllerTelemetry();
    // Owner tare is asynchronous: never import totals from the previous shot.
    if (!policy.accepts(model.tareGeneration)) return;
    shot.waterPumped = model.pumpedWaterMl;
    sumFlow = model.coffeeOutputMl;
  }

  void reset(const char* profileName, bool enableScaleLearning) {
    shot.maxPressure = 0;
    shot.tempMax = -1e9; shot.tempMin = +1e9;
    shot.tempAvg = 0; shot.flowAvg = 0; 
    shot.outGrams = 0; shot.waterPumped = 0;
    sumTemp = 0; sumFlow = 0;
    (void)profileName;
    (void)enableScaleLearning; // Retained API; local predictive learning retired.
    policy.begin(PumpPSM::tare(), false);
    lastTickMs = millis();
  }

  void armScaleForShot() {
    policy.scaleAllowed = ScaleService::allowedForYield();
  }

  void finalize(unsigned long shotStart) {
    refreshModelTotals();
    unsigned long now = millis();
    float elapsed = (now - shotStart) / 1000.0f;
    if (elapsed > 0) {
      shot.tempAvg  = sumTemp / elapsed;
      shot.flowAvg  = sumFlow / elapsed;
      shot.outGrams = resolveOutGrams(sumFlow);
      
      // Bộ lọc "Shot thật": Chỉ lưu nếu áp suất > 1.0 bar và thời gian > 10s
      const bool realShot = shot.maxPressure > 1.0f && elapsed > 10.0f;
      if (realShot) {
        lastShot.weight = shot.outGrams;
        lastShot.time = elapsed;
        lastShot.peakPressure = shot.maxPressure;
        lastShot.profileName = shot.profileName;
        Serial.println("[ShotAggregator] Real shot recorded.");
      } else {
        Serial.printf("[ShotAggregator] Flush detected (P:%.1f, T:%.1f), not recording.\n", 
                      shot.maxPressure, elapsed);
      }
    }
  }

  void update(State currentState, unsigned long shotStart) {
    unsigned long now = millis();
    float dt = (now - lastTickMs) / 1000.0f;
    if (dt <= 0.0f) return;

    if (currentState == PROFILE_RUN || currentState == HOTWATER_RUN) {
      float shotTemp = shot.brewTempEstC > 0.0f ? shot.brewTempEstC : shot.temperature;
      sumTemp += shotTemp * dt;
      refreshModelTotals();
      lastTickMs = now;
      shot.outGrams = resolveOutGrams(sumFlow);
      const float scaleYield = yieldControlWeight();

      // Shot #490 exceeded 26 g in phase 0: quick yield must stop any brew
      // phase, not only Manual or the final profile phase. No model input.
      const float quick=QuickYield::active();
      const float stopThreshold=YieldStopPolicy::threshold(currentState == PROFILE_RUN,
          currentState == HOTWATER_RUN || isManualProfile() || isFullOutProfile(),
          quick, targetYieldGrams);
      if (YieldStopPolicy::reached(stopThreshold, scaleYield, ScaleService::preparingShot())) {
        stopShotActively("auto-yield");
        Serial.printf("[AutoYield] Threshold=%.1f -> Stop at %.1f\n", stopThreshold, scaleYield);
      }
    } else {
      lastTickMs = now;
    }

    // Peak follows the sensor-filtered display/history channel, not controller
    // internal pressure or the legacy profile/flow EMA.
    if ((currentState == PROFILE_RUN || currentState == HOTWATER_RUN) &&
        shot.visualPressure > shot.maxPressure) {
      shot.maxPressure = shot.visualPressure;
    }
    if (currentState == PROFILE_RUN) shot.shotTooLong = (shot.shotTime > 30000);
  }

} // namespace ShotAggregator
