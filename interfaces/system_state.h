// File: interfaces/system_state.h
// Nơi duy nhất gom tất cả extern declarations cho shared state
// Mọi module cần truy cập global state chỉ cần include file này
// Refactor: 2026-04-20

#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include "shot_types.h"
#include "calibration_types.h"
#include "profile_types.h"

// Forward declarations
struct FlowEstimator;
class ThermalModel;

// ====== Shot runtime data ======
extern ShotData shot;
extern LastShotResult lastShot;

// ====== Flow estimator instance ======
extern FlowEstimator flow;

// ====== Thermal estimator instance ======
extern ThermalModel thermal;

// ====== Calibration data ======
// (struct definitions in calibration_types.h)
// gPress, gFlow, g_flowCalNeedUpdate — already extern'd in calibration_types.h

// ====== Profile data ======
// (struct definitions in profile_types.h)
// gProfiles[], gProfilesCount, profiles[], profilesCount, isPhysicalScaleConnected
// — already extern'd in profile_types.h

// ====== Shot control state ======
extern volatile float targetYieldGrams;

#endif // SYSTEM_STATE_H
