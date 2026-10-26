#pragma once

#include "scale/scale_driver.h"
#include <atomic>

class AcaiaScaleDriver : public IScaleDriver {
 public:
  const char* name() const override { return "Acaia / Felicita"; }
  HmiProtocol::ScaleSource sourceType() const override {
    return HmiProtocol::ScaleSource::BleCommercial;
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
  uint32_t lastHeartbeatMs_ = 0;

  void sendHeartbeat();
  static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData,
                             size_t length, bool isNotify);
  static std::atomic<AcaiaScaleDriver*> instance_;
};
