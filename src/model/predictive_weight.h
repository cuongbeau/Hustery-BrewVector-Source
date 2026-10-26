// File: src/model/predictive_weight.h
// Predictive Weight: Detect thời điểm nước xuyên qua puck
// Dùng khi không có cân khả dụng từ HMI.

#ifndef PREDICTIVE_WEIGHT_H
#define PREDICTIVE_WEIGHT_H

#include <Arduino.h>

class PredictiveWeight {
private:
  bool outputFlowStarted;
  // Resistance = pressure_bar * 1000 / flow_gps
  // Range thực tế: 1000-18000. Dùng để detect puck bão hòa.
  float puckResistance;
  float previousPuckResistance;
  float resistanceDelta;
  uint8_t plateauTicks;  // Đếm ticks ổn định cho debounce
  float learnedGateWaterG;
  uint32_t learnedGateTimeMs;
  bool learnedGateAvailable;

public:
  PredictiveWeight();
  bool isOutputFlow();
  void configureLearnedGate(float waterG, uint32_t timeMs, bool available);
  void update(float pSmooth, float fSmooth, float pChange, float fChange, 
              float waterPumped, uint32_t elapsedMs, bool inSoak,
              float pTarget);
  void reset();
};

#endif // PREDICTIVE_WEIGHT_H
