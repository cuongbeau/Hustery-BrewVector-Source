#include "src/hardware/scale_service.h"

#include <math.h>
#include <esp_system.h>
#include "src/communication/hmi_link.h"
#include "protocol/scale_tare_timing.h"
#include "protocol/scale_sample_tracker.h"

namespace {
ScaleSampleTracker tracker;
const auto& hmiSample = tracker.latest;
bool preparing = false;
bool leaseOutstanding = false;
uint32_t prepareStarted = 0;
HmiProtocol::ScaleTareRequestPayload request{};

bool onlineAt(uint32_t now) { return tracker.online(now); }
} // namespace

namespace ScaleService {
void begin() {}

void acceptHmiSample(const HmiProtocol::ScaleSamplePayload& sample, uint32_t now) {
  tracker.accept(sample, now);
}

void beginShotTare() {
  preparing = false;
  tracker.endShot();
  const uint32_t now = millis();
  // A cancelled request can still be in transit. Preserve its lease quarantine.
  if (leaseOutstanding && now - prepareStarted >= ScaleTareTiming::MainWaitMs)
    leaseOutstanding = false;
  if (!HmiLink::compatible() || !onlineAt(now)) {
    preparing = leaseOutstanding;
    if (!preparing) tracker.beginShot(false, now);
    return;
  }
  request = {};
  request.requestId = esp_random();
  if (!request.requestId) request.requestId = 1;
  request.hmiBootId = hmiSample.hmiBootId;
  request.connectionGeneration = hmiSample.connectionGeneration;
  // HMI uptime, sampled at transmission, avoids comparing two boot clocks.
  // Queuing can only shorten this lease, never extend it beyond Main's wait.
  request.expiresAtHmiMs = hmiSample.transmittedAtMs + ScaleTareTiming::LeaseMs;
  if (!HmiLink::requestScaleTare(request)) {
    preparing = leaseOutstanding;
    if (!preparing) tracker.beginShot(false, now);
    return;
  }
  prepareStarted = now;
  preparing = true;
  leaseOutstanding = true;
  Serial.printf("[ScaleShot] Prepare %lu\n", static_cast<unsigned long>(request.requestId));
}

bool shotTarePending() {
  if (!preparing) return false;
  const uint32_t now = millis();
  const bool matching = hmiSample.hmiBootId == request.hmiBootId &&
      hmiSample.connectionGeneration == request.connectionGeneration &&
      hmiSample.tareRequestId == request.requestId;
  if (!request.cancel && matching && onlineAt(now) &&
      hmiSample.tareState == HmiProtocol::ScaleTareState::Ready) {
    tracker.beginShot(true, now);
    preparing = false;
    leaseOutstanding = false; // HMI has finished both writes and settling.
    Serial.println("[ScaleShot] Post-tare sample ready (zero not verified)");
  } else if (now - prepareStarted >= ScaleTareTiming::MainWaitMs) {
    preparing = false;
    leaseOutstanding = false;
    tracker.beginShot(false, now);
    Serial.println("[ScaleShot] Prepare timeout; time fallback");
  } else {
    // Even after an early failure retain the bounded wait. No late tare may
    // run after fallback starts, including a lost cancel on either UART path.
    return true;
  }
  return false;
}

void endShot() {
  if (leaseOutstanding) {
    request.cancel = 1;
    HmiLink::requestScaleTare(request);
  }
  preparing = false;
  tracker.endShot();
}

bool preparingShot() { return preparing; }

bool allowedForYield() {
  return !preparing && tracker.allowed(millis());
}

bool usingHmi() { return onlineAt(millis()); }
bool isOnline() { return usingHmi(); }
bool isStable() { return usingHmi() && (hmiSample.flags & HmiProtocol::ScaleSampleStable); }
float getWeight() { return usingHmi() ? hmiSample.weightG : 0.0f; }
HmiProtocol::ScaleSource source() { return usingHmi() ? hmiSample.source : HmiProtocol::ScaleSource::None; }
uint8_t batteryPercent() {
  return usingHmi() && (hmiSample.flags & HmiProtocol::ScaleSampleBatteryValid)
      ? hmiSample.batteryPercent : 0xFF;
}
uint32_t sampleAgeMs() {
  return usingHmi() ? tracker.age(millis()) : UINT32_MAX;
}
} // namespace ScaleService
