// File: src/control/profile_engine.h
// Tách từ profiles.h — Profile data + Executor engine
// Refactor: 2026-04-20

#ifndef PROFILE_ENGINE_H
#define PROFILE_ENGINE_H

#include <Arduino.h>
#include "interfaces/profile_types.h"

// ====== Profile Runtime (definitions in .cpp) ======
// profiles[], profilesCount — defined in profile_engine.cpp

namespace ProfilesRuntime {
  void rebuild();
}

// ====== Profile Executor ======
extern ProfileExecutorState pfExec;

enum ProfileStepExitReason : uint8_t {
  PROFILE_EXIT_NONE = 0,
  PROFILE_EXIT_TIMEOUT,
  PROFILE_EXIT_PRESSURE_ABOVE,
  PROFILE_EXIT_PRESSURE_BELOW,
  PROFILE_EXIT_FLOW_ABOVE,
  PROFILE_EXIT_FLOW_BELOW,
  PROFILE_EXIT_WEIGHT,
  PROFILE_EXIT_WATER,
  PROFILE_EXIT_GLOBAL_WEIGHT,
  PROFILE_EXIT_GLOBAL_TIME,
  PROFILE_EXIT_MANUAL_STOP
};

struct ProfileStepExit {
  bool valid = false;
  uint8_t step = 0;
  StepMode mode = MODE_PRESSURE;
  ProfileStepExitReason reason = PROFILE_EXIT_NONE;
  float actual = 0.0f;
  float threshold = 0.0f;
  float stepTimeS = 0.0f;
  float timeoutS = 0.0f;
  char name[16] = "";
};

void profileExecutorStart(int idx, float currentP, float currentFlow, float currentDuty);
void profileExecutorStop();
bool profileExecutorTick(float currentP, float currentWeight, float currentFlow, float currentWaterPumped,
                         float currentDuty, float &outTarget, StepMode &outMode,
                         float &outPLim, float &outFLim);
ProfileStepExit profileExecutorLastExit();
void profileExecutorMarkManualStop();
float profileExecutorTemperature(); // Schema hook only; no heater SSR output.

TargetSnap getTargetSnapshot(int pIdx, float tsec);
float profileTargetBar(int pIdx, float tsec);

// ====== Profile helpers (used by orchestrator) ======
int  getCurrentProfileIndex();
void setCurrentProfileIndex(int idx);
bool isManualProfile();
bool isFullOutProfile();
bool isPressureProfile();
bool showTargetLine();

#endif // PROFILE_ENGINE_H
