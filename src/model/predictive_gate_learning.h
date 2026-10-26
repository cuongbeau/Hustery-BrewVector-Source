#pragma once

#include <Arduino.h>

namespace PredictiveGateLearning {

struct Estimate {
  float waterG = 20.0f;
  uint32_t timeMs = 0;
  uint16_t sampleCount = 0;
  bool learned = false;
};

void begin();
Estimate estimateFor(const char* profileName);

// Learning is deliberately supervised by a real scale. Shots without a fresh
// scale observation may use prior learning, but never update it.
void startShot(const char* profileName, bool scaleOnline, float baselineWeightG);
void observeScale(bool scaleOnline, uint32_t sampleAgeMs, float weightG,
                  uint32_t elapsedMs, float waterPumpedG);
void finishShot(bool acceptObservation);
void persistIfNeeded();

}  // namespace PredictiveGateLearning
