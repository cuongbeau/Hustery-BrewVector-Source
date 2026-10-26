#include "scale/drivers/diy_ble_scale_driver.h"

namespace {
// DIY Scale custom GATT UUIDs
const NimBLEUUID DIY_SERVICE_UUID("FF50");
const NimBLEUUID DIY_NOTIFY_UUID("FF51");
const NimBLEUUID DIY_WRITE_UUID("FF52");

// Standard 8-byte payload structure
struct __attribute__((packed)) DiyWeightPacket {
  uint8_t header;          // 0xA5
  int16_t weight_x100;     // 100x grams (e.g. 1850 = 18.50g)
  uint8_t flags;           // Bit 0: Stable, Bit 1: Charging
  uint8_t batteryPercent;  // 0 - 100%
  uint16_t sequence;
  uint8_t checksum;
};
}  // namespace

std::atomic<DiyBleScaleDriver*> DiyBleScaleDriver::instance_{nullptr};

bool DiyBleScaleDriver::matches(NimBLEAdvertisedDevice* device) {
  if (device->isAdvertisingService(DIY_SERVICE_UUID)) return true;
  const std::string name = device->getName();
  if (name.find("Hustery") != std::string::npos ||
      name.find("HUSTERY") != std::string::npos ||
      name.find("BrewVector") != std::string::npos ||
      name.find("DIY Scale") != std::string::npos ||
      name.find("Espresso Scale") != std::string::npos) {
    return true;
  }
  return false;
}


bool DiyBleScaleDriver::onConnect(NimBLEClient* client,
                                  ScaleSampleCallback callback) {
  instance_ = this;
  callback_ = callback;
  notifyChar_ = nullptr;
  writeChar_ = nullptr;
  portENTER_CRITICAL(&deviceMux_);hasSequence_ = false;portEXIT_CRITICAL(&deviceMux_);
  v2_ = false;
  portENTER_CRITICAL(&deviceMux_);
  device_ = {};
  portEXIT_CRITICAL(&deviceMux_);

  BLERemoteService* pService = client->getService(DIY_SERVICE_UUID);
  if (!pService) {
    Serial.println("[DIY Scale] Service FF50 not found");
    return false;
  }

  auto capability = pService->getCharacteristic(NimBLEUUID("FF53"));
  if (capability && capability->canRead()) {
    const auto version = capability->readValue();
    v2_ = version.size() == 1 && uint8_t(version[0]) == HusteryScaleV2::Version;
  }
  if (v2_) {
    if (client->getMTU() < sizeof(HusteryScaleV2::Sample) + 3) return false;
  }
  notifyChar_ = pService->getCharacteristic(DIY_NOTIFY_UUID);
  if (!notifyChar_ || !notifyChar_->canNotify() ||
      !notifyChar_->subscribe(true, notifyCallback)) {
    Serial.println("[DIY Scale] FF51 notify subscription failed");
    return false;
  }
  Serial.println("[DIY Scale] Subscribed to FF51 notify");

  writeChar_ = pService->getCharacteristic(DIY_WRITE_UUID);

  return (notifyChar_ != nullptr);
}

void DiyBleScaleDriver::onDisconnect() {
  notifyChar_ = nullptr;
  writeChar_ = nullptr;
  callback_ = nullptr;
  portENTER_CRITICAL(&deviceMux_);hasSequence_ = false;portEXIT_CRITICAL(&deviceMux_);
}

bool DiyBleScaleDriver::sendTare(uint32_t token, uint32_t remainingMs) {
  if (!writeChar_) return false;
  // Legacy DIY tare is a blocking 25-conversion operation with no completion
  // status. Never auto-tare it from a shot; manual standalone controls remain.
  if (!v2_) return false;
  HusteryScaleV2::Sample device;
  portENTER_CRITICAL(&deviceMux_);
  device = device_;
  portEXIT_CRITICAL(&deviceMux_);
  if (!device.bootId || !token || !remainingMs || remainingMs > 700) return false;
  HusteryScaleV2::TareRequest request;
  request.token = token;
  request.bootId = device.bootId;
  request.deadlineMs = device.transmittedAtMs + remainingMs;
  if (writeChar_->getRemoteService()->getClient()->getMTU() < sizeof(request) + 3) return false;
  return writeChar_->writeValue(reinterpret_cast<uint8_t*>(&request), sizeof(request), false);
}

void DiyBleScaleDriver::poll(uint32_t nowMs) {
  (void)nowMs;
}

void DiyBleScaleDriver::notifyCallback(BLERemoteCharacteristic* pChar,
                                       uint8_t* pData, size_t length,
                                       bool isNotify) {
  (void)pChar;
  (void)isNotify;
  auto* instance=instance_.load();
  const auto callback=instance?instance->callback_.load():nullptr;
  if (!callback || length < 4) return;

  if (instance->v2_) {
    if (length != sizeof(HusteryScaleV2::Sample)) return;
    HusteryScaleV2::Sample packet;
    memcpy(&packet, pData, sizeof(packet));
    if (packet.magic != 0xA6 || packet.version != HusteryScaleV2::Version ||
        packet.checksum != HusteryScaleV2::checksum(packet)) return;
    portENTER_CRITICAL(&instance->deviceMux_);
    instance->device_ = packet;
    portEXIT_CRITICAL(&instance->deviceMux_);
    callback(HmiProtocol::ScaleSource::BleCustom, packet.weightG,
        (packet.flags & HusteryScaleV2::Stable) != 0, packet.battery, &packet);
    return;
  }
  if (length == sizeof(DiyWeightPacket) && pData[0] == 0xA5) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < length - 1; ++i) checksum ^= pData[i];
    if (checksum != pData[length - 1]) return;

    DiyWeightPacket pkt{};
    memcpy(&pkt, pData, sizeof(pkt));
    portENTER_CRITICAL(&instance->deviceMux_);
    const bool duplicate=instance->hasSequence_ &&
        static_cast<int16_t>(pkt.sequence-instance->lastSequence_)<=0;
    if(!duplicate){instance->lastSequence_=pkt.sequence;instance->hasSequence_=true;}
    portEXIT_CRITICAL(&instance->deviceMux_);
    if(duplicate)return;

    const float weight = pkt.weight_x100 / 100.0f;
    const bool stable = (pkt.flags & 0x01) != 0;
    const uint8_t battery = pkt.batteryPercent;
    callback(HmiProtocol::ScaleSource::BleCustom, weight, stable,
                         battery, nullptr);
  } else if (length == 4) {
    // Simple 4-byte float fallback
    float weight = 0.0f;
    memcpy(&weight, pData, 4);
    callback(HmiProtocol::ScaleSource::BleCustom, weight, true, 0xFF, nullptr);
  }
}
