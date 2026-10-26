#pragma once
#include "hmi_protocol.h"
#include "scale_tare_timing.h"

#if __cplusplus >= 201402L
#define SCALE_CONSTEXPR14 constexpr
#else
#define SCALE_CONSTEXPR14
#endif

// Shared pure policy: test clock wrap, repeated samples and baseline loss on
// the host compiler without BLE, UART or a running pump.
class ScaleSampleTracker {
 public:
  HmiProtocol::ScaleSamplePayload latest{};

  constexpr bool online(uint32_t now) const {
    return hasSample_ && connected_ && ScaleTareTiming::fresh(now, receivedAt_, age_);
  }
  constexpr uint32_t age(uint32_t now) const { return age_ + now - receivedAt_; }
  SCALE_CONSTEXPR14 void accept(const HmiProtocol::ScaleSamplePayload& sample, uint32_t now) {
    const bool valid = (sample.flags & HmiProtocol::ScaleSampleConnected) &&
        sample.hmiBootId && sample.connectionGeneration &&
        sample.source != HmiProtocol::ScaleSource::None &&
        sample.weightG > -100.0f && sample.weightG < 5000.0f;
    if (shot_ && (!online(now) || !valid || sample.sampleAgeMs > ScaleTareTiming::FreshMs ||
        sample.hmiBootId != boot_ || sample.connectionGeneration != connection_ ||
        sample.tareGeneration != tare_)) allowed_ = false;
    const bool same = hasSample_ && sample.hmiBootId == latest.hmiBootId &&
        sample.connectionGeneration == latest.connectionGeneration;
    if (same && !ScaleTareTiming::newer(sample.sampleSequence, latest.sampleSequence)) {
      if (sample.sampleSequence != latest.sampleSequence ||
          sample.sourceTimestampMs != latest.sourceTimestampMs || sample.weightG != latest.weightG) {
        if (shot_) allowed_ = false;
        return;
      }
      // Accept metadata but never refresh age for a repeated observation.
    } else {
      receivedAt_ = now;
      age_ = sample.sampleAgeMs;
    }
    latest = sample;
    connected_ = valid;
    hasSample_ = true;
  }
  SCALE_CONSTEXPR14 void beginShot(bool allowed, uint32_t now) {
    shot_ = true;
    allowed_ = allowed && online(now);
    boot_ = latest.hmiBootId;
    connection_ = latest.connectionGeneration;
    tare_ = latest.tareGeneration;
  }
  SCALE_CONSTEXPR14 void endShot() { shot_ = false; allowed_ = false; }
  SCALE_CONSTEXPR14 bool allowed(uint32_t now) {
    if (!online(now)) allowed_ = false;
    return shot_ && allowed_;
  }
 private:
  uint32_t receivedAt_ = 0, age_ = 0;
  uint32_t boot_ = 0, connection_ = 0, tare_ = 0;
  bool hasSample_ = false, connected_ = false, shot_ = false, allowed_ = false;
};
#undef SCALE_CONSTEXPR14
