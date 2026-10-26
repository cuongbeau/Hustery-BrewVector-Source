#pragma once

#include "scale/scale_driver.h"
#include <atomic>

class DiyBleScaleDriver : public IScaleDriver {
 public:
  const char* name() const override { return "Hustery Scale"; }
  HmiProtocol::ScaleSource sourceType() const override {

    return HmiProtocol::ScaleSource::BleCustom;
  }

  bool matches(NimBLEAdvertisedDevice* device) override;
  bool onConnect(NimBLEClient* client, ScaleSampleCallback callback) override;
  void onDisconnect() override;
  bool sendTare(uint32_t token = 0, uint32_t remainingMs = 0) override;
  void poll(uint32_t nowMs) override;

 private:
  BLERemoteCharacteristic* notifyChar_ = nullptr;
  BLERemoteCharacteristic* writeChar_ = nullptr;
  std::atomic<ScaleSampleCallback> callback_{nullptr};
  std::atomic<bool> v2_{false};
  HusteryScaleV2::Sample device_{};
  mutable portMUX_TYPE deviceMux_ = portMUX_INITIALIZER_UNLOCKED;
  uint16_t lastSequence_ = 0;
  bool hasSequence_ = false;

  static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData,
                             size_t length, bool isNotify);
  static std::atomic<DiyBleScaleDriver*> instance_;
};
