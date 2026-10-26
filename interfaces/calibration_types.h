// File: interfaces/calibration_types.h
// Tách từ calibration_store.h — chỉ chứa struct definitions
// Refactor: 2026-04-20

#ifndef CALIBRATION_TYPES_H
#define CALIBRATION_TYPES_H

#include <Arduino.h>

// ====== Pressure Cal (ADC) ======
struct PressureCal {
  float offset = 0.50f;  // V ở 0 bar
  float scale  = 4.00f;  // bar per Volt
  float divK   = 1.00f;  // (giữ lại, nếu có mạch chia áp thì dùng)
};

// ====== Flow Calibration ======
struct FlowCal {
  // Full-drive pump flow at 1 and 9 bar. Defaults preserve the previous
  // 7.8 ml/s free-flow-scaled two-point flow model.
  float q1MlPerS = 7.3768f;
  float q9MlPerS = 3.9914f;
};

// ====== Extern declarations ======
extern PressureCal gPress;
extern FlowCal gFlow;
extern bool g_flowCalNeedUpdate;

#endif // CALIBRATION_TYPES_H
