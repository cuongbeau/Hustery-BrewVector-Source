#include "scale/scale_manager.h"

#include <math.h>
#include <esp_system.h>
#include "scale_tare_timing.h"

namespace {
ScaleManager* gScaleManagerInstance = nullptr;


void globalScaleConnectionChanged() {
  if (gScaleManagerInstance) gScaleManagerInstance->connectionChanged();
}

void globalScaleSampleCallback(HmiProtocol::ScaleSource source, float weightG,
                               bool stable, uint8_t batteryPercent, const HusteryScaleV2::Sample* device) {
  if (gScaleManagerInstance && gScaleManagerInstance->bleClient().isConnected()) {
    gScaleManagerInstance->publish(source, weightG, stable, batteryPercent, device);
  }
}
}  // namespace

void ScaleManager::begin() {
  gScaleManagerInstance = this;
  bootId_ = esp_random();
  if (!bootId_) bootId_ = 1;
  bleClient_.begin(globalScaleSampleCallback, globalScaleConnectionChanged);
}

void ScaleManager::poll(uint32_t nowMs) {
  bleClient_.poll(nowMs);
  pollShotTare();

  if (simulatorEnabled_ && nowMs - lastSimulatorSampleMs_ >= 50U) {
    lastSimulatorSampleMs_ = nowMs;
    publish(HmiProtocol::ScaleSource::Simulator, simulatorWeightG_,
            simulatorStable_, 0xFF);
  }
  if (!simulatorEnabled_) {
    portENTER_CRITICAL(&sampleMux_);
    const uint32_t freshNowMs = millis();
    if (lastSampleMs_ != 0 &&
        static_cast<int32_t>(deviceAgeAtRx_ + freshNowMs - lastSampleMs_) >
            static_cast<int32_t>(SAMPLE_TIMEOUT_MS)) {
      source_ = HmiProtocol::ScaleSource::None;
      stable_ = false;
    }
    portEXIT_CRITICAL(&sampleMux_);
  }
}

void ScaleManager::connectionChanged() {
  portENTER_CRITICAL(&sampleMux_);
  if (++connectionGeneration_ == 0) ++connectionGeneration_;
  source_ = HmiProtocol::ScaleSource::None;
  lastSampleMs_ = 0;
  stable_ = false;
  deviceV2_ = false;
  device_ = {};
  portEXIT_CRITICAL(&sampleMux_);
}

bool ScaleManager::sendPhysicalTare(uint32_t token, uint32_t remainingMs) {
  // Invalidate the old observation even if a BLE write fails ambiguously.
  portENTER_CRITICAL(&sampleMux_);
  if (++tareGeneration_ == 0) ++tareGeneration_;
  source_ = HmiProtocol::ScaleSource::None;
  lastSampleMs_ = 0;
  stable_ = false;
  portEXIT_CRITICAL(&sampleMux_);
  if (simulatorEnabled_) {
    simulatorWeightG_ = 0.0f;
    return true;
  }
  if (!remainingMs || remainingMs > 700) return false;
  return bleClient_.tare(token, remainingMs);
}

bool ScaleManager::tare() {
  tareStage_ = 0;
  tareState_ = HmiProtocol::ScaleTareState::Cancelled;
  deviceTareToken_ = esp_random();
  if (!deviceTareToken_) deviceTareToken_ = 1;
  return sendPhysicalTare(deviceTareToken_, 700);
}

void ScaleManager::requestShotTare(const HmiProtocol::ScaleTareRequestPayload& request) {
  using HmiProtocol::ScaleTareState;
  if (!request.requestId || request.hmiBootId != bootId_) return;
  if (request.cancel) {
    // Remember a cancel even when its prepare was dropped in the UART queue.
    tareRequest_ = request;
    tareStage_ = 0;
    tareState_ = ScaleTareState::Cancelled;
    return;
  }
  if (request.requestId == tareRequest_.requestId) return; // Never repeat a tare.
  const auto current = packet(millis());
  if (!ScaleTareTiming::leaseValid(request.expiresAtHmiMs, millis()) ||
      request.connectionGeneration != current.connectionGeneration) return;
  tareRequest_ = request;
  tareStage_ = 0;
  tareState_ = ScaleTareState::Failed;
  if (!connected() || (!simulatorEnabled_ && !bleClient_.isConnected())) return;
  deviceTareToken_ = esp_random();
  if (!deviceTareToken_) deviceTareToken_ = 1;
  const uint32_t remaining = tareRequest_.expiresAtHmiMs - millis();
  if (!sendPhysicalTare(deviceTareToken_, remaining)) return;
  // Start the 50 ms interval after the write returns, never from a stale loop tick.
  tareAtMs_ = millis();
  expectedTareGeneration_ = packet(tareAtMs_).tareGeneration;
  tareStage_ = 1;
  tareState_ = ScaleTareState::Preparing;
}

void ScaleManager::pollShotTare() {
  using HmiProtocol::ScaleTareState;
  if (!tareStage_) return;
  const uint32_t now = millis();
  const auto current = packet(now);
  portENTER_CRITICAL(&sampleMux_);
  const bool deviceV2 = deviceV2_;
  const auto device = device_;
  portEXIT_CRITICAL(&sampleMux_);
  if (!ScaleTareTiming::leaseValid(tareRequest_.expiresAtHmiMs, now) ||
      current.connectionGeneration != tareRequest_.connectionGeneration ||
      (!deviceV2 && current.tareGeneration != expectedTareGeneration_) ||
      (!simulatorEnabled_ && !bleClient_.isConnected())) {
    tareStage_ = 0;
    tareState_ = ScaleTareState::Failed;
    return;
  }
  if (tareStage_ == 1 && ScaleTareTiming::secondDue(now, tareAtMs_)) {
    if (!sendPhysicalTare(deviceTareToken_, tareRequest_.expiresAtHmiMs - millis())) {
      tareStage_ = 0;
      tareState_ = ScaleTareState::Failed;
      return;
    }
    tareAtMs_ = millis();
    const auto after = packet(tareAtMs_);
    expectedTareGeneration_ = after.tareGeneration;
    finalTareSequence_ = after.sampleSequence;
    tareStage_ = 2;
  } else if (tareStage_ == 2 && ScaleTareTiming::settled(now, tareAtMs_) &&
             ScaleTareTiming::newer(current.sampleSequence, finalTareSequence_) &&
             connected() &&
             (!deviceV2 || (device.tareToken == deviceTareToken_ &&
               device.tareState == HusteryScaleV2::TareState::Done &&
               (device.flags & HusteryScaleV2::Valid)))) {
    tareStage_ = 0;
    // DIY v2 confirms offset commit plus a subsequent ADC observation.
    // Commercial protocols without completion status only confirm fresh data.
    tareState_ = ScaleTareState::Ready;
  }
}

void ScaleManager::startScan() {
  bleClient_.startScan(5);
}

bool ScaleManager::isScanning() const {
  return bleClient_.isScanning();
}


void ScaleManager::publish(HmiProtocol::ScaleSource source, float weightG,
                           bool stable, uint8_t batteryPercent, const HusteryScaleV2::Sample* device) {
  if (source == HmiProtocol::ScaleSource::None || !isfinite(weightG) ||
      weightG < -100.0f || weightG > 5000.0f)
    return;
  portENTER_CRITICAL(&sampleMux_);
  bool newObservation = true;
  if (device) {
    const bool previous = deviceV2_;
    if (!previous || device_.bootId != device->bootId) deviceSampleSequence_ = device->sampleSequence - 1;
    if (previous && device_.bootId != device->bootId) {
      if (++connectionGeneration_ == 0) ++connectionGeneration_;
    }
    if (!previous || device_.bootId != device->bootId ||
        device_.tareGeneration != device->tareGeneration) {
      if (++tareGeneration_ == 0) ++tareGeneration_;
    }
    newObservation = !previous || device_.bootId != device->bootId ||
        ScaleTareTiming::newer(device->sampleSequence, deviceSampleSequence_);
    const bool outOfOrder = previous && device_.bootId == device->bootId &&
        device->sampleSequence != deviceSampleSequence_ && !newObservation;
    deviceV2_ = true;
    device_ = *device;
    if (!(device->flags & HusteryScaleV2::Valid) || device->sampleAgeMs > SAMPLE_TIMEOUT_MS || outOfOrder) {
      source_ = HmiProtocol::ScaleSource::None;
      stable_ = false;
      portEXIT_CRITICAL(&sampleMux_);
      return;
    }
    if (!newObservation) {
      portEXIT_CRITICAL(&sampleMux_);
      return; // Status heartbeats never refresh ADC age.
    }
    deviceSampleSequence_ = device->sampleSequence;
    deviceAgeAtRx_ = device->sampleAgeMs;
  } else {
    deviceV2_ = false;
    deviceAgeAtRx_ = 0;
  }
  source_ = source;
  weightG_ = weightG;
  stable_ = stable;
  batteryPercent_ = batteryPercent <= 100 ? batteryPercent : 0xFF;
  if (++sampleSequence_ == 0) ++sampleSequence_;
  sourceTimestampMs_ = millis();
  lastSampleMs_ = sourceTimestampMs_;
  portEXIT_CRITICAL(&sampleMux_);
}

HmiProtocol::ScaleSamplePayload ScaleManager::packet(uint32_t nowMs) const {
  (void)nowMs;
  HmiProtocol::ScaleSamplePayload result{};
  portENTER_CRITICAL(&sampleMux_);
  const uint32_t freshNowMs = millis();
  const uint32_t age = lastSampleMs_ == 0
                           ? UINT16_MAX
                           : deviceAgeAtRx_ + freshNowMs - lastSampleMs_;
  const bool sampleConnected =
      source_ != HmiProtocol::ScaleSource::None && lastSampleMs_ != 0 &&
      static_cast<int32_t>(age) <= static_cast<int32_t>(SAMPLE_TIMEOUT_MS);
  result.hmiBootId = bootId_;
  result.connectionGeneration = connectionGeneration_;
  result.tareGeneration = tareGeneration_;
  result.transmittedAtMs = freshNowMs;
  result.tareRequestId = tareRequest_.requestId;
  result.tareState = tareState_;
  result.sourceTimestampMs = sourceTimestampMs_;
  result.sampleSequence = sampleSequence_;
  result.weightG = weightG_;
  result.source = source_;
  result.sampleAgeMs = static_cast<uint16_t>(min<uint32_t>(age, UINT16_MAX));
  if (sampleConnected) result.flags |= HmiProtocol::ScaleSampleConnected;
  if (sampleConnected && stable_)
    result.flags |= HmiProtocol::ScaleSampleStable;
  if (sampleConnected && batteryPercent_ <= 100) {
    result.flags |= HmiProtocol::ScaleSampleBatteryValid;
    result.batteryPercent = batteryPercent_;
  }
  if (simulatorEnabled_) result.flags |= HmiProtocol::ScaleSampleSimulated;
  portEXIT_CRITICAL(&sampleMux_);
  return result;
}

bool ScaleManager::connected() const {
  portENTER_CRITICAL(&sampleMux_);
  const uint32_t freshNowMs = millis();
  const bool result =
      source_ != HmiProtocol::ScaleSource::None && lastSampleMs_ != 0 &&
      static_cast<int32_t>(deviceAgeAtRx_ + freshNowMs - lastSampleMs_) <=
          static_cast<int32_t>(SAMPLE_TIMEOUT_MS);
  portEXIT_CRITICAL(&sampleMux_);
  return result;
}

uint32_t ScaleManager::sampleAgeMs() const {
  portENTER_CRITICAL(&sampleMux_);
  const uint32_t result =
      lastSampleMs_ == 0 ? UINT32_MAX : deviceAgeAtRx_ + millis() - lastSampleMs_;
  portEXIT_CRITICAL(&sampleMux_);
  return result;
}

HmiProtocol::ScaleSource ScaleManager::source() const {
  portENTER_CRITICAL(&sampleMux_);
  const HmiProtocol::ScaleSource result = source_;
  portEXIT_CRITICAL(&sampleMux_);
  return result;
}

float ScaleManager::weightG() const {
  portENTER_CRITICAL(&sampleMux_);
  const float result = weightG_;
  portEXIT_CRITICAL(&sampleMux_);
  return result;
}

bool ScaleManager::stable() const {
  portENTER_CRITICAL(&sampleMux_);
  const bool result = stable_;
  portEXIT_CRITICAL(&sampleMux_);
  return result;
}

uint8_t ScaleManager::batteryPercent() const {
  portENTER_CRITICAL(&sampleMux_);
  const uint8_t result = batteryPercent_;
  portEXIT_CRITICAL(&sampleMux_);
  return result;
}

bool ScaleManager::setSimulator(bool enabled, float weightG, bool stable) {
  if (!isfinite(weightG) || weightG < -100.0f || weightG > 5000.0f)
    return false;
  if (simulatorEnabled_ != enabled) connectionChanged();
  simulatorEnabled_ = enabled;
  simulatorWeightG_ = weightG;
  simulatorStable_ = stable;
  lastSimulatorSampleMs_ = 0;
  if (!enabled && source_ == HmiProtocol::ScaleSource::Simulator) {
    source_ = HmiProtocol::ScaleSource::None;
    stable_ = false;
    lastSampleMs_ = 0;
  }
  return true;
}
