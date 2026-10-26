// File: src/model/flow_model.h
// Version: 2026-09-18_Q1Q9Affine
// Refactor: Tách declaration — implementation trong flow_model.cpp

#ifndef FLOW_MODEL_H
#define FLOW_MODEL_H

#include <Arduino.h>

struct FlowEstimator {
  void  begin();
  void  reset();
  float getPumpFlowPerClick(float pressure, float cps, float pStallOverride = -1.0f);
  float getPumpFlow(float cps, float pressure);
  float getFlowPerClickAtZeroBar(float cps);
  float getRawFPCAtPressure(float p, float cps);
  float getClicksPerSecondForFlow(float targetFlow, float pressure, float pStallOverride = -1.0f);
  float update(float pressureBar);
};

#endif // FLOW_MODEL_H
