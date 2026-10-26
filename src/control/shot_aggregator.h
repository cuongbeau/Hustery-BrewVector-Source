// File: src/control/shot_aggregator.h
// Shot stats + upstream virtual output + BLE-only auto-yield.
// Refactor: 2026-04-20

#ifndef SHOT_AGGREGATOR_H
#define SHOT_AGGREGATOR_H

#include <Arduino.h>
#include "interfaces/shot_types.h"

namespace ShotAggregator {
  void  reset(const char* profileName, bool enableScaleLearning);
  void armScaleForShot();
  void  update(State currentState, unsigned long shotStart);
  void  finalize(unsigned long shotStart);
  float resolveOutGrams(float flowIntegrated);
  // Legacy API names retained for trace compatibility. Now expose upstream
  // virtual output, without the local first-drip gate or learned correction.
  float modelYieldAfterGate();
  bool predictiveGateOpen();
  float yieldControlWeight(); // BLE only; NAN disables every yield exit.
}

#endif // SHOT_AGGREGATOR_H
