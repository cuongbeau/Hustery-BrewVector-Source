#pragma once

#include <Arduino.h>

#include "scale/ble_scale_client.h"

// One normalized scale front-end for BLE, commercial drivers and the
// temporary simulator. Drivers publish samples here; UART transport remains
// independent of the radio implementation.
class ScaleManager {
 public:
  void begin();
  void poll(uint32_t nowMs);

  void publish(HmiProtocol::ScaleSource source, float weightG, bool stable,
               uint8_t batteryPercent = 0xFF, const HusteryScaleV2::Sample* device = nullptr);
  HmiProtocol::ScaleSamplePayload packet(uint32_t nowMs) const;
  bool connected() const;
  HmiProtocol::ScaleSource source() const;
  float weightG() const;
  bool stable() const;
  uint8_t batteryPercent() const;
  uint32_t sampleAgeMs() const;


  bool tare();
  void connectionChanged();
  void requestShotTare(const HmiProtocol::ScaleTareRequestPayload& request);
  void startScan();
  void stopScan() { bleClient_.stopScan(); }
  bool isScanning() const;
  void pairWithMac(const char* mac) { bleClient_.pairWithMac(mac); }
  void unpair() { bleClient_.unpair(); }

  const char* activeScaleName() const { return bleClient_.activeScaleName(); }
  const char* activeScaleMac() const { return bleClient_.activeScaleMac(); }
  const char* savedMac() const { return bleClient_.savedMac(); }
  int activeScaleRssi() const { return bleClient_.activeScaleRssi(); }
  uint8_t discoveredCount() const { return bleClient_.discoveredCount(); }
  const BleDiscoveredScale* discoveredScales() const { return bleClient_.discoveredScales(); }


  bool setSimulator(bool enabled, float weightG = 0.0f,
                    bool stable = true);
  bool simulatorEnabled() const { return simulatorEnabled_; }

  const BleScaleClient& bleClient() const { return bleClient_; }
  BleScaleClient& bleClient() { return bleClient_; }

 private:
  static constexpr uint32_t SAMPLE_TIMEOUT_MS = 750;
  void pollShotTare();
  bool sendPhysicalTare(uint32_t token, uint32_t remainingMs);
  BleScaleClient bleClient_;
  HusteryScaleV2::Sample device_{};
  bool deviceV2_ = false;
  uint32_t deviceAgeAtRx_ = 0;
  uint32_t deviceSampleSequence_ = 0;
  uint32_t bootId_ = 0;
  uint32_t connectionGeneration_ = 1;
  uint32_t tareGeneration_ = 0;
  HmiProtocol::ScaleTareRequestPayload tareRequest_{};
  HmiProtocol::ScaleTareState tareState_ = HmiProtocol::ScaleTareState::None;
  uint8_t tareStage_ = 0;
  uint32_t tareAtMs_ = 0;
  uint32_t finalTareSequence_ = 0;
  uint32_t expectedTareGeneration_ = 0;
  uint32_t deviceTareToken_ = 0; // Unique per attempt, also across Main/HMI reboot.
  HmiProtocol::ScaleSource source_ = HmiProtocol::ScaleSource::None;
  float weightG_ = 0.0f;
  bool stable_ = false;
  uint8_t batteryPercent_ = 0xFF;
  uint32_t sampleSequence_ = 0;
  uint32_t sourceTimestampMs_ = 0;
  uint32_t lastSampleMs_ = 0;
  bool simulatorEnabled_ = false;
  float simulatorWeightG_ = 0.0f;
  bool simulatorStable_ = true;
  uint32_t lastSimulatorSampleMs_ = 0;
  // BLE notifications run from NimBLE's task while packet()/poll() run from
  // the Arduino loop. Keep the normalized sample and its timestamp atomic so
  // a callback cannot update lastSampleMs_ after the loop captured an older
  // nowMs, which would make unsigned age arithmetic underflow.
  mutable portMUX_TYPE sampleMux_ = portMUX_INITIALIZER_UNLOCKED;
};
