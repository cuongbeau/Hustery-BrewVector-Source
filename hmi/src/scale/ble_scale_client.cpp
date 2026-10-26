#include "scale/ble_scale_client.h"
#include <Preferences.h>

namespace {
constexpr char PREFS_NAMESPACE[] = "ble_scale";
constexpr char PREF_KEY_MAC[] = "target_mac";
}  // namespace

void BleScaleClient::begin(ScaleSampleCallback sampleCb, void (*sessionChanged)()) {
  sessionChanged_ = sessionChanged;
  sampleCallback_ = sampleCb;

  // Register supported scale drivers
  drivers_.clear();
  drivers_.push_back(&diyDriver_);
  drivers_.push_back(&acaiaDriver_);
  drivers_.push_back(&timemoreDriver_);

  loadSavedConfig();

  NimBLEDevice::init("BrewVector-HMI");
  NimBLEDevice::setMTU(64);

  pScan_ = NimBLEDevice::getScan();

  pScan_->setAdvertisedDeviceCallbacks(this, false);
  pScan_->setActiveScan(true);

  pScan_->setInterval(100);
  pScan_->setWindow(30);

  events_=xQueueCreate(12,sizeof(Event));
  driverMutex_=xSemaphoreCreateMutex();
  publishView(); view_=published_;
  if(!events_ || !driverMutex_ || xTaskCreate([](void* p){static_cast<BleScaleClient*>(p)->runWorker();},"hmi-ble",8192,this,1,&worker_)!=pdPASS){
    if(events_){vQueueDelete(events_);events_=nullptr;}
    if(driverMutex_){vSemaphoreDelete(driverMutex_);driverMutex_=nullptr;}
    Serial.println("[BLE] worker allocation failed");return;
  }
  // Initial auto-scan if target MAC is configured
  if (targetMac_[0] != '\0') {
    startScan(4);
  }
}

void BleScaleClient::loadSavedConfig() {
  Preferences prefs;
  if (prefs.begin(PREFS_NAMESPACE, true)) {
    String mac = prefs.getString(PREF_KEY_MAC, "");
    snprintf(targetMac_, sizeof(targetMac_), "%s", mac.c_str());
    prefs.end();
  }
}

void BleScaleClient::saveConfig(const char* mac) {
  Preferences prefs;
  if (prefs.begin(PREFS_NAMESPACE, false)) {
    prefs.putString(PREF_KEY_MAC, mac ? mac : "");
    prefs.end();
    snprintf(targetMac_, sizeof(targetMac_), "%s", mac ? mac : "");
  }
}

IScaleDriver* BleScaleClient::findMatchingDriver(NimBLEAdvertisedDevice* device) {
  for (auto* driver : drivers_) {
    if (driver->matches(device)) {
      return driver;
    }
  }
  return nullptr;
}

namespace {
void onScanComplete(NimBLEScanResults results) {
  (void)results;
}
}  // namespace

void BleScaleClient::startScan(uint32_t durationSeconds) {
  if(xTaskGetCurrentTaskHandle()!=worker_){Event e{};e.kind=EventKind::Scan;e.duration=durationSeconds;enqueue(e);return;}
  if (isScanning_ || isConnected_ || isConnecting_ || connectPending_) return;
  discoveredCount_ = 0;
  isScanning_ = true;
  pScan_->start(durationSeconds, onScanComplete, false);
}

void BleScaleClient::stopScan() {
  if(xTaskGetCurrentTaskHandle()!=worker_){Event e{};e.kind=EventKind::StopScan;enqueue(e);return;}
  if (isScanning_) {
    pScan_->stop();
    isScanning_ = false;
  }
}

bool BleScaleClient::isScanning() const {
  return scanningView_.load();
}

bool BleScaleClient::isConnected() const {
  return connectedView_.load();
}

const char* BleScaleClient::activeScaleName() const {
  return connectedView_ ? view_.name : "DISCONNECTED";
}

const char* BleScaleClient::activeScaleMac() const {
  return connectedView_ ? view_.mac : "--";
}

int BleScaleClient::activeScaleRssi() const {
  return connectedView_ ? view_.rssi : 0;
}

HmiProtocol::ScaleSource BleScaleClient::activeSourceType() const {
  return connectedView_ ? view_.source : HmiProtocol::ScaleSource::None;
}

void BleScaleClient::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
  IScaleDriver* driver = findMatchingDriver(advertisedDevice);
  if (!driver) return;

  const std::string mac = advertisedDevice->getAddress().toString();
  const std::string name = advertisedDevice->getName();

  Event event{};event.kind=EventKind::Advertisement;event.driver=driver;
  snprintf(event.mac,sizeof(event.mac),"%s",mac.c_str());
  snprintf(event.name,sizeof(event.name),"%s",name.c_str());
  event.rssi=advertisedDevice->getRSSI();event.addressType=advertisedDevice->getAddressType();
  enqueue(event);
}

void BleScaleClient::processAdvertisement(const Event& event) {
  const std::string mac=event.mac,name=event.name;
  auto* driver=event.driver;
  // Add to discovered list if not already present
  bool found = false;
  for (uint8_t i = 0; i < discoveredCount_; ++i) {
    if (strcmp(discoveredList_[i].mac, mac.c_str()) == 0) {
      found = true;
      break;
    }
  }

  if (!found && discoveredCount_ < MAX_DISCOVERED) {
    BleDiscoveredScale& s = discoveredList_[discoveredCount_++];
    snprintf(s.mac, sizeof(s.mac), "%s", mac.c_str());
    snprintf(s.name, sizeof(s.name), "%s",
             name.empty() ? driver->name() : name.c_str());
    s.rssi = event.rssi;
    s.sourceType = driver->sourceType();
  }

  // A manual scan is discovery-only.  Connect only after the user has saved
  // a target MAC; this prevents silently pairing with the first scale nearby.
  if (!isConnected_ && !isConnecting_ && !connectPending_ &&
      targetMac_[0] != '\0' && strcmp(targetMac_, mac.c_str()) == 0) {
    pendingAddress_ = NimBLEAddress(event.mac,event.addressType);
    activeDriver_ = driver;
    connectPending_ = true;
    pScan_->stop();
    isScanning_ = false;
  }
}


bool BleScaleClient::connectTo(const NimBLEAddress& address) {
  if (isConnected_ || isConnecting_) return false;
  isConnecting_ = true;
  lastConnectAttemptMs_ = millis();

  if (!pClient_) {
    pClient_ = NimBLEDevice::createClient();
    pClient_->setClientCallbacks(this, false);
    pClient_->setConnectionParams(12, 24, 0, 200);  // Low latency (15-30ms)
    // Keep a failed reconnect below Main's 3.5 s HMI-link timeout.
    pClient_->setConnectTimeout(2);
  }

  Serial.printf("[BLE Scale] Connecting to %s...\n", address.toString().c_str());
  if (!pClient_->connect(address)) {
    Serial.println("[BLE Scale] Connection failed");
    isConnecting_ = false;
    return false;
  }

  return true;
}

void BleScaleClient::disconnect() {
  if(xTaskGetCurrentTaskHandle()!=worker_){connectedView_=false;if(sessionChanged_)sessionChanged_();Event e{};e.kind=EventKind::Disconnect;enqueue(e);return;}
  connectedView_=false;
  if (sessionChanged_) sessionChanged_();
  if (activeDriver_) {
    activeDriver_->onDisconnect();
    activeDriver_ = nullptr;
  }
  if (pClient_ && pClient_->isConnected()) {
    pClient_->disconnect();
  }
  isConnected_ = false;
  isConnecting_ = false;
  connectPending_ = false;
}

void BleScaleClient::pairWithMac(const char* mac) {
  if(xTaskGetCurrentTaskHandle()!=worker_){Event e{};e.kind=EventKind::Pair;snprintf(e.mac,sizeof(e.mac),"%s",mac?mac:"");enqueue(e);return;}
  if (!mac || mac[0] == '\0') return;
  saveConfig(mac);
  disconnect();
  // The driver is identified from the advertisement.  Connecting directly
  // from a bare MAC leaves activeDriver_ null and produces a false connection.
  lastConnectAttemptMs_ = millis();
  startScan(5);
}


void BleScaleClient::unpair() {
  if(xTaskGetCurrentTaskHandle()!=worker_){connectedView_=false;if(sessionChanged_)sessionChanged_();Event e{};e.kind=EventKind::Unpair;enqueue(e);return;}
  saveConfig("");
  targetMac_[0] = '\0';
  disconnect();
  Serial.println("[BLE Scale] Unpaired scale.");
}


bool BleScaleClient::tare(uint32_t token, uint32_t remainingMs) {
  // Driver objects and handles remain worker-owned; this no-response, <=MTU
  // write is the sole short operation borrowed by loopTask. Never wait for GATT.
  if(!driverMutex_ || !connectedView_ || xSemaphoreTake(driverMutex_,0)!=pdTRUE)return false;
  const bool sent=isConnected_ && activeDriver_ && activeDriver_->sendTare(token,remainingMs);
  xSemaphoreGive(driverMutex_);
  return sent;
}

void BleScaleClient::onConnect(NimBLEClient* pClient) {
  // Invalidate before driver subscription can publish its first sample.
  if (sessionChanged_) sessionChanged_();
  isConnecting_ = false;
  if (!activeDriver_) {
    Serial.println("[BLE Scale] Connected without a matching driver");
    if (pClient) pClient->disconnect();
    return;
  }

  NimBLEAddress addr = pClient->getPeerAddress();
  snprintf(activeMac_, sizeof(activeMac_), "%s", addr.toString().c_str());
  activeRssi_ = pClient->getRssi();
  snprintf(activeName_, sizeof(activeName_), "%s", activeDriver_->name());
  if (!activeDriver_->onConnect(pClient, sampleCallback_)) {
    Serial.println("[BLE Scale] Driver onConnect failed");
    activeDriver_->onDisconnect();
    activeDriver_ = nullptr;
    if (pClient) pClient->disconnect();
    return;
  }
  isConnected_ = !disconnected_.load();
  connectedView_=isConnected_;
  saveConfig(activeMac_);
  Serial.printf("[BLE Scale] Connected to %s (%s)\n", activeName_, activeMac_);
}

void BleScaleClient::onDisconnect(NimBLEClient* pClient) {
  (void)pClient;
  connectedView_=false;disconnected_=true;
  if(sessionChanged_)sessionChanged_();
}

void BleScaleClient::pollWorker(uint32_t nowMs) {
  if (isScanning_ && pScan_ && !pScan_->isScanning()) {
    isScanning_ = false;
  }

  if (connectPending_ && !isConnected_ && !isConnecting_) {
    connectPending_ = false;
    connectTo(pendingAddress_);
  }

  if (isConnected_ && activeDriver_) {
    activeDriver_->poll(nowMs);
  }

  // Auto-reconnect periodic check if disconnected
  if (!isConnected_ && !isConnecting_ && !isScanning_ && !connectPending_) {
    if (targetMac_[0] != '\0' && (nowMs - lastConnectAttemptMs_ >= 8000U)) {
      lastConnectAttemptMs_ = nowMs;
      startScan(3);
    }
  }
}

bool BleScaleClient::enqueue(const Event& event) {
  if(!events_ || (event.kind==EventKind::Advertisement && uxQueueSpacesAvailable(events_)<=3) ||
      xQueueSend(events_,&event,0)!=pdTRUE){++eventDrops_;return false;}
  return true;
}
void BleScaleClient::publishView() {
  View state{};
  snprintf(state.target,sizeof(state.target),"%s",targetMac_);
  snprintf(state.mac,sizeof(state.mac),"%s",activeMac_);
  snprintf(state.name,sizeof(state.name),"%s",activeName_);
  state.rssi=activeRssi_;state.count=discoveredCount_;
  if(isConnected_ && activeDriver_)state.source=activeDriver_->sourceType();
  memcpy(state.list,discoveredList_,sizeof(state.list));
  portENTER_CRITICAL(&viewMux_);published_=state;portEXIT_CRITICAL(&viewMux_);
  scanningView_=isScanning_;
}
void BleScaleClient::poll(uint32_t nowMs) {
  (void)nowMs;
  portENTER_CRITICAL(&viewMux_);view_=published_;portEXIT_CRITICAL(&viewMux_);
}
void BleScaleClient::runWorker() {
  for(;;) {
    xSemaphoreTake(driverMutex_,portMAX_DELAY);
    const uint32_t started=micros();
    if(disconnected_.exchange(false)) {
      isConnected_=false;isConnecting_=false;
      if(activeDriver_){activeDriver_->onDisconnect();activeDriver_=nullptr;}
    }
    Event event{};
    if(xQueueReceive(events_,&event,0)==pdTRUE) {
      switch(event.kind) {
        case EventKind::Scan:startScan(event.duration);break;
        case EventKind::StopScan:stopScan();break;
        case EventKind::Pair:pairWithMac(event.mac);break;
        case EventKind::Unpair:unpair();break;
        case EventKind::Disconnect:disconnect();break;
        case EventKind::Advertisement:processAdvertisement(event);break;
      }
    }
    pollWorker(millis());publishView();
    const uint32_t elapsed=micros()-started;
    if(elapsed>maxWorkerUs_)maxWorkerUs_=elapsed;
    xSemaphoreGive(driverMutex_);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
