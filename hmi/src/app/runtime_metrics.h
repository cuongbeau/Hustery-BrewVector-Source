#pragma once
#include <Arduino.h>

// Written/read only on loopTask, including diagnostic HTTP snapshot callbacks.
struct HmiRuntimeMetrics {
  uint32_t loops=0,lastStartUs=0,maxGapUs=0,maxLoopUs=0,over100ms=0;
  uint32_t maxStageUs[6]={}; // scale, UART, trace/update, Web, touch/UI, render
};
extern HmiRuntimeMetrics hmiRuntimeMetrics;
class HmiLoopProbe {
 public:
  HmiLoopProbe();
  ~HmiLoopProbe();
  void stage(uint8_t index);
 private: uint32_t started_,previous_;
};
