#pragma once
#include "hmi_protocol.h"

// Arduino builds use C++11; C++14+ also allows compile-time ring verification.
#if __cplusplus >= 201402L
#define BREW_STREAM_MUTABLE_CONSTEXPR constexpr
#else
#define BREW_STREAM_MUTABLE_CONSTEXPR
#endif

// Single owner: MainLink/loopTask. Bounded history for independent Web cursors.
class BrewStream {
 public:
  static constexpr size_t Capacity=512;
  struct Sample { uint32_t seq; float t,p,pt,flow,wt,temp; uint8_t mode; };
  BREW_STREAM_MUTABLE_CONSTEXPR void attach(Sample* storage){samples_=storage;}
  constexpr bool available()const{return samples_!=nullptr;}
  constexpr size_t size()const{return count_;}
  constexpr uint32_t session()const{return session_;}
  constexpr const Sample& at(size_t index)const{return samples_[(head_+index)%Capacity];}
  BREW_STREAM_MUTABLE_CONSTEXPR void push(const HmiProtocol::TelemetryPayload& p){
    const auto state=static_cast<HmiProtocol::MachineState>(p.machineState);
    const bool active=state==HmiProtocol::MachineState::Brew || state==HmiProtocol::MachineState::HotWater;
    const bool newSession=active && (!active_ || p.timestampMs<lastTimestamp_ || p.elapsedS<lastElapsed_);
    if(newSession){
      head_=count_=0;if(++session_==0)++session_;
    }
    if(samples_ && active && (newSession || p.timestampMs!=lastTimestamp_)){
      if(count_==Capacity){head_=(head_+1)%Capacity;--count_;}
      samples_[(head_+count_++)%Capacity]={++sequence_,p.elapsedS,p.pressureBar,p.targetValue,
          p.flowGps,p.weightG,p.brewTempC,p.controlMode};
    }
    active_=active;lastTimestamp_=p.timestampMs;lastElapsed_=p.elapsedS;
  }
 private:
  Sample* samples_=nullptr;
  size_t head_=0,count_=0;
  uint32_t sequence_=0,session_=0,lastTimestamp_=0;
  float lastElapsed_=0;
  bool active_=false;
};
#undef BREW_STREAM_MUTABLE_CONSTEXPR
