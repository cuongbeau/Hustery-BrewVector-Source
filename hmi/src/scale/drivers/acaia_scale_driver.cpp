#include "scale/drivers/acaia_scale_driver.h"

namespace {
// Acaia GATT UUIDs
const NimBLEUUID ACAIA_SERVICE_OLD("1820");
const NimBLEUUID ACAIA_CHAR_OLD("2A80");

const NimBLEUUID ACAIA_SERVICE_LUNAR("00004953-0779-f1ab-6440-0e0a97796a60");
const NimBLEUUID ACAIA_CHAR_LUNAR("00004953-0779-f1ab-6440-0e0a97796a61");

const NimBLEUUID FELICITA_SERVICE("FFE0");
const NimBLEUUID FELICITA_CHAR("FFE1");
}  // namespace

std::atomic<AcaiaScaleDriver*> AcaiaScaleDriver::instance_{nullptr};

bool AcaiaScaleDriver::matches(NimBLEAdvertisedDevice* device) {
  if (device->isAdvertisingService(ACAIA_SERVICE_OLD) ||
      device->isAdvertisingService(ACAIA_SERVICE_LUNAR) ||
      device->isAdvertisingService(FELICITA_SERVICE)) {
    return true;
  }
  const std::string name = device->getName();
  if (name.find("ACAIA") != std::string::npos ||
      name.find("PROCH") != std::string::npos ||
      name.find("LUNAR") != std::string::npos ||
      name.find("PEARL") != std::string::npos ||
      name.find("PYXIS") != std::string::npos ||
      name.find("FELICITA") != std::string::npos ||
      name.find("ARC") != std::string::npos) {
    return true;
  }
  return false;
}

bool AcaiaScaleDriver::onConnect(NimBLEClient* client,
                                 ScaleSampleCallback callback) {
  instance_ = this;
  callback_ = callback;
  notifyChar_ = nullptr;
  writeChar_ = nullptr;
  lastHeartbeatMs_ = 0;

  // Try Lunar UUID first, then Old UUID, then Felicita
  BLERemoteService* pService = client->getService(ACAIA_SERVICE_LUNAR);
  if (pService) {
    notifyChar_ = pService->getCharacteristic(ACAIA_CHAR_LUNAR);
    writeChar_ = notifyChar_;
  } else {
    pService = client->getService(ACAIA_SERVICE_OLD);
    if (pService) {
      notifyChar_ = pService->getCharacteristic(ACAIA_CHAR_OLD);
      writeChar_ = notifyChar_;
    } else {
      pService = client->getService(FELICITA_SERVICE);
      if (pService) {
        notifyChar_ = pService->getCharacteristic(FELICITA_CHAR);
        writeChar_ = notifyChar_;
      }
    }
  }

  if (notifyChar_ && notifyChar_->canNotify()) {
    notifyChar_->subscribe(true, notifyCallback);
    Serial.println("[Acaia] Subscribed to notify");
    sendHeartbeat();
    return true;
  }

  return false;
}

void AcaiaScaleDriver::onDisconnect() {
  notifyChar_ = nullptr;
  writeChar_ = nullptr;
  callback_ = nullptr;
}

bool AcaiaScaleDriver::sendTare(uint32_t token, uint32_t remainingMs) {
  (void)token; (void)remainingMs;
  if (!writeChar_) return false;
  // Acaia Tare Command: [0xEF, 0xDD, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00]
  uint8_t cmd[8] = {0xEF, 0xDD, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
  if (writeChar_->getRemoteService()->getClient()->getMTU() < sizeof(cmd) + 3) return false;
  return writeChar_->writeValue(cmd, sizeof(cmd), false);
}

void AcaiaScaleDriver::sendHeartbeat() {
  if (!writeChar_) return;
  // Acaia Heartbeat Ping: [0xEF, 0xDD, 0x00, 0x02, 0x00, 0x02, 0x00]
  uint8_t ping[7] = {0xEF, 0xDD, 0x00, 0x02, 0x00, 0x02, 0x00};
  writeChar_->writeValue(ping, sizeof(ping), false);
  lastHeartbeatMs_ = millis();
}

void AcaiaScaleDriver::poll(uint32_t nowMs) {
  if (writeChar_ && (nowMs - lastHeartbeatMs_ >= 2000U)) {
    sendHeartbeat();
  }
}

void AcaiaScaleDriver::notifyCallback(BLERemoteCharacteristic* pChar,
                                      uint8_t* pData, size_t length,
                                      bool isNotify) {
  (void)pChar;
  (void)isNotify;
  auto* instance=instance_.load();
  const auto callback=instance?instance->callback_.load():nullptr;
  if (!callback || length < 7) return;

  // Verify Acaia Magic Header 0xEF 0xDD
  if (pData[0] == 0xEF && pData[1] == 0xDD) {
    const uint8_t msgType = pData[2];
    if (msgType == 0x05 && length >= 10) {  // Weight message
      // Bytes 4-5: weight in 0.1g or 0.01g
      const uint16_t rawWeight = (pData[5] << 8) | pData[4];
      const uint8_t unit = pData[6];
      const float scaling = (unit == 1) ? 10.0f : 100.0f;
      float weight = static_cast<float>(rawWeight) / scaling;
      if (pData[7] & 0x02) weight = -weight;  // Sign bit
      const bool stable = (pData[7] & 0x01) == 0;
      callback(HmiProtocol::ScaleSource::BleCommercial, weight,
                           stable, 0xFF, nullptr);
    } else if (msgType == 0x08 && length >= 5) {  // Battery message
      const uint8_t battery = pData[4];
      callback(HmiProtocol::ScaleSource::BleCommercial, NAN,
                           false, battery, nullptr);
    }
  }
}
