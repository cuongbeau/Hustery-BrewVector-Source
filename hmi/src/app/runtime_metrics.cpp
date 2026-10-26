#include "app/runtime_metrics.h"
HmiRuntimeMetrics hmiRuntimeMetrics;
HmiLoopProbe::HmiLoopProbe():started_(micros()),previous_(started_){
  auto& m=hmiRuntimeMetrics;
  if(m.loops)m.maxGapUs=max(m.maxGapUs,started_-m.lastStartUs);
  m.lastStartUs=started_;++m.loops;
}
void HmiLoopProbe::stage(uint8_t index){
  const uint32_t now=micros();
  hmiRuntimeMetrics.maxStageUs[index]=max(hmiRuntimeMetrics.maxStageUs[index],now-previous_);
  previous_=now;
}
HmiLoopProbe::~HmiLoopProbe(){
  const uint32_t elapsed=micros()-started_;
  hmiRuntimeMetrics.maxLoopUs=max(hmiRuntimeMetrics.maxLoopUs,elapsed);
  if(elapsed>100000)++hmiRuntimeMetrics.over100ms;
}
