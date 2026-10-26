#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "hmi_protocol.h"
#include "hustery_scale_v2.h"

class ScaleManager;

// Callback signature for scale drivers to publish normalized samples
typedef void (*ScaleSampleCallback)(HmiProtocol::ScaleSource source, float weightG,
                                    bool stable, uint8_t batteryPercent,
                                    const HusteryScaleV2::Sample* device);

class IScaleDriver {
 public:
  virtual ~IScaleDriver() = default;

  virtual const char* name() const = 0;
  virtual HmiProtocol::ScaleSource sourceType() const = 0;

  // Returns true if the advertised device matches this scale brand
  virtual bool matches(NimBLEAdvertisedDevice* device) = 0;

  // Called after BLE connection is established; driver should discover GATT services & chars
  virtual bool onConnect(NimBLEClient* client, ScaleSampleCallback callback) = 0;

  // Called when BLE connection is terminated
  virtual void onDisconnect() = 0;

  // Sends Tare command to the scale hardware
  virtual bool sendTare(uint32_t token = 0, uint32_t remainingMs = 0) = 0;

  // Periodic loop (e.g. for heartbeat or keep-alive)
  virtual void poll(uint32_t nowMs) {}
};
