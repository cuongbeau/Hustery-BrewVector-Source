#include "scale/drivers/timemore_scale_driver.h"

namespace {
const NimBLEUUID TIMEMORE_SERVICE("FFE0");
const NimBLEUUID TIMEMORE_CHAR("FFE1");
}  // namespace

std::atomic<TimemoreScaleDriver*> TimemoreScaleDriver::instance_{nullptr};

bool TimemoreScaleDriver::matches(NimBLEAdvertisedDevice* device) {
  if (device->isAdvertisingService(TIMEMORE_SERVICE)) return true;
  const std::string name = device->getName();
  if (name.find("TIMEMORE") != std::string::npos ||
      name.find("TIMEM") != std::string::npos ||
      name.find("BLACK MIRROR") != std::string::npos ||
      name.find("TES010") != std::string::npos) {
    return true;
  }
  return false;
}

bool TimemoreScaleDriver::onConnect(NimBLEClient* client,
                                    ScaleSampleCallback callback) {
  instance_ = this;
  callback_ = callback;
  notifyChar_ = nullptr;
  writeChar_ = nullptr;

  BLERemoteService* pService = client->getService(TIMEMORE_SERVICE);
  if (!pService) return false;

  notifyChar_ = pService->getCharacteristic(TIMEMORE_CHAR);
  writeChar_ = notifyChar_;

  if (notifyChar_ && notifyChar_->canNotify()) {
    notifyChar_->subscribe(true, notifyCallback);
    Serial.println("[Timemore] Subscribed to notify");
    return true;
  }
  return false;
}

void TimemoreScaleDriver::onDisconnect() {
  notifyChar_ = nullptr;
  writeChar_ = nullptr;
  callback_ = nullptr;
}

bool TimemoreScaleDriver::sendTare(uint32_t token, uint32_t remainingMs) {
  (void)token; (void)remainingMs;
  if (!writeChar_) return false;
  // Timemore Tare Command
  uint8_t cmd[6] = {0xAA, 0x02, 0x01, 0x00, 0x00, 0x55};
  if (writeChar_->getRemoteService()->getClient()->getMTU() < sizeof(cmd) + 3) return false;
  return writeChar_->writeValue(cmd, sizeof(cmd), false);
}

void TimemoreScaleDriver::poll(uint32_t nowMs) {
  (void)nowMs;
}

void TimemoreScaleDriver::notifyCallback(BLERemoteCharacteristic* pChar,
                                         uint8_t* pData, size_t length,
                                         bool isNotify) {
  (void)pChar;
  (void)isNotify;
  auto* instance=instance_.load();
  const auto callback=instance?instance->callback_.load():nullptr;
  if (!callback || length < 7) return;

  if (pData[0] == 0xAA || pData[0] == 0x02) {
    // Timemore packet: Weight bytes 2..4 (int24 in 0.1g)
    int32_t raw = (pData[2] << 16) | (pData[3] << 8) | pData[4];
    if (raw & 0x800000) raw |= 0xFF000000;  // Sign extension
    const float weight = static_cast<float>(raw) / 10.0f;
    const bool stable = (pData[1] & 0x01) == 0;
    callback(HmiProtocol::ScaleSource::BleCommercial, weight,
                         stable, 0xFF, nullptr);
  }
}
