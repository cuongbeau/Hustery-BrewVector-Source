#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>
#include <atomic>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "scale/scale_driver.h"
#include "scale/drivers/diy_ble_scale_driver.h"
#include "scale/drivers/acaia_scale_driver.h"
#include "scale/drivers/timemore_scale_driver.h"
#include "ui/ui_types.h"


class BleScaleClient : public NimBLEClientCallbacks, public NimBLEAdvertisedDeviceCallbacks {
 public:

  void begin(ScaleSampleCallback sampleCb, void (*sessionChanged)() = nullptr);
  void poll(uint32_t nowMs);

  bool isConnected() const;
  bool isScanning() const;
  uint32_t eventDrops() const { return eventDrops_.load(); }
  uint32_t maxWorkerUs() const { return maxWorkerUs_.load(); }
  void startScan(uint32_t durationSeconds = 5);
  void stopScan();

  bool connectTo(const NimBLEAddress& address);
  void disconnect();
  void pairWithMac(const char* mac);
  void unpair();
  bool tare(uint32_t token = 0, uint32_t remainingMs = 0);

  const char* activeScaleName() const;
  const char* activeScaleMac() const;
  const char* savedMac() const { return view_.target; }
  int activeScaleRssi() const;
  HmiProtocol::ScaleSource activeSourceType() const;

  uint8_t discoveredCount() const { return view_.count; }
  const BleDiscoveredScale* discoveredScales() const { return view_.list; }


  // NimBLE callbacks
  void onConnect(NimBLEClient* pClient) override;
  void onDisconnect(NimBLEClient* pClient) override;
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;

 private:
  enum class EventKind:uint8_t { Scan, StopScan, Pair, Unpair, Disconnect, Advertisement };
  struct Event {
    EventKind kind; uint32_t duration=0; char mac[18]={}; char name[32]={};
    int rssi=0; uint8_t addressType=0; IScaleDriver* driver=nullptr;
  };
  struct View {
    char target[18]={},mac[18]={},name[32]={}; int rssi=0;
    uint8_t count=0; BleDiscoveredScale list[6]={};
    HmiProtocol::ScaleSource source=HmiProtocol::ScaleSource::None;
  } published_,view_;
  mutable portMUX_TYPE viewMux_=portMUX_INITIALIZER_UNLOCKED;
  QueueHandle_t events_=nullptr;
  TaskHandle_t worker_=nullptr;
  SemaphoreHandle_t driverMutex_=nullptr;
  std::atomic<bool> connectedView_{false}, scanningView_{false}, disconnected_{false};
  std::atomic<uint32_t> eventDrops_{0}, maxWorkerUs_{0};
  void runWorker();
  void pollWorker(uint32_t nowMs);
  void publishView();
  void processAdvertisement(const Event& event);
  bool enqueue(const Event& event);
  NimBLEClient* pClient_ = nullptr;
  NimBLEScan* pScan_ = nullptr;
  ScaleSampleCallback sampleCallback_ = nullptr;
  void (*sessionChanged_)() = nullptr;

  std::vector<IScaleDriver*> drivers_;
  IScaleDriver* activeDriver_ = nullptr;

  DiyBleScaleDriver diyDriver_;
  AcaiaScaleDriver acaiaDriver_;
  TimemoreScaleDriver timemoreDriver_;

  bool isConnected_ = false;
  bool isConnecting_ = false;
  bool isScanning_ = false;
  bool connectPending_ = false;
  NimBLEAddress pendingAddress_;
  uint32_t lastScanEndMs_ = 0;
  uint32_t lastConnectAttemptMs_ = 0;

  char targetMac_[18] = {};
  char activeMac_[18] = {};
  char activeName_[32] = {};

  int activeRssi_ = 0;

  static constexpr uint8_t MAX_DISCOVERED = 6;
  BleDiscoveredScale discoveredList_[MAX_DISCOVERED] = {};
  uint8_t discoveredCount_ = 0;

  void loadSavedConfig();
  void saveConfig(const char* mac);
  IScaleDriver* findMatchingDriver(NimBLEAdvertisedDevice* device);
};
