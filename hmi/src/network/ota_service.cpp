#include "network/ota_service.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include "local_access.h"

#include "web/hmi_web_server.h"

namespace {
constexpr char HOSTNAME[] = "brewvector-hmi";
constexpr char AP_PASSWORD[] = BREWVECTOR_AP_PASSWORD;
constexpr uint32_t AP_CLOSE_DELAY_MS = 90000;
constexpr uint32_t AP_RESTORE_DELAY_MS = 15000;
constexpr uint32_t WIFI_RECONNECT_INITIAL_MS = 5000;
constexpr uint32_t WIFI_RECONNECT_MAX_MS = 60000;

Preferences preferences;
String accessPointName;
bool apActive = false;
bool mdnsActive = false;
uint32_t connectedSinceMs = 0;
uint32_t disconnectedSinceMs = 0;
HmiOta::NetworkInfo scanResults[10]{};
uint8_t scanResultCount = 0;
bool wifiScanRunning = false;
uint32_t wifiScanStartedMs = 0;
String savedSsid;
String savedPassword;
String pendingSsid;
String pendingPassword;
uint32_t pendingStartedMs = 0;
uint32_t lastConnectFailureMs = 0;
uint32_t nextReconnectMs = 0;
uint32_t reconnectDelayMs = WIFI_RECONNECT_INITIAL_MS;
volatile bool wifiDisconnectObserved = false;
volatile uint8_t wifiDisconnectReason = 0;
bool autoConnect = true;
bool clockStarted = false;

void scheduleReconnect(uint32_t nowMs) {
  nextReconnectMs = nowMs + reconnectDelayMs;
  reconnectDelayMs = min(reconnectDelayMs * 2, WIFI_RECONNECT_MAX_MS);
}

void startSavedStationConnection(uint32_t nowMs) {
  if (!autoConnect || savedSsid.isEmpty()) return;
  WiFi.disconnect(false, false);
  WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
  scheduleReconnect(nowMs);
}

void startStationConnection(const String& ssid, const String& password) {
  pendingSsid = ssid;
  pendingPassword = password;
  pendingStartedMs = millis();
  lastConnectFailureMs = 0;
  WiFi.disconnect(false, false);
  WiFi.begin(pendingSsid.c_str(), pendingPassword.c_str());
  disconnectedSinceMs = pendingStartedMs;
}

void saveConnectedNetwork() {
  savedSsid = pendingSsid;
  savedPassword = pendingPassword;
  preferences.begin("hmi_wifi", false);
  preferences.putString("ssid", savedSsid);
  preferences.putString("pass", savedPassword);
  preferences.end();
  pendingSsid = "";
  pendingPassword = "";
  pendingStartedMs = 0;
  nextReconnectMs = 0;
  reconnectDelayMs = WIFI_RECONNECT_INITIAL_MS;
}

void restoreSavedNetwork(uint32_t nowMs) {
  pendingSsid = "";
  pendingPassword = "";
  pendingStartedMs = 0;
  lastConnectFailureMs = nowMs;
  reconnectDelayMs = WIFI_RECONNECT_INITIAL_MS;
  startSavedStationConnection(nowMs);
  disconnectedSinceMs = nowMs;
}


void startAccessPoint() {
  if (apActive) return;
  WiFi.mode(WIFI_AP_STA);
  if (accessPointName.isEmpty()) {
    const uint64_t mac = ESP.getEfuseMac();
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%04X",
             static_cast<unsigned int>(mac & 0xFFFF));
    accessPointName = String("BrewVector-HMI-") + suffix;
  }
  apActive = WiFi.softAP(accessPointName.c_str(), AP_PASSWORD);
  Serial.printf("[HMI OTA] AP %s %s, IP=%s\n", accessPointName.c_str(),
                apActive ? "started" : "failed",
                WiFi.softAPIP().toString().c_str());
}


void beginNetworkServices() {
  if (!mdnsActive) {
    mdnsActive = MDNS.begin(HOSTNAME);
    if (mdnsActive) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[HMI OTA] Ready: http://%s.local/update (%s)\n",
                    HOSTNAME, WiFi.localIP().toString().c_str());
    }
  }
}

void updateWifiScan() {
  if (!wifiScanRunning) return;
  const int result = WiFi.scanComplete();
  if (result == WIFI_SCAN_RUNNING) {
    if (wifiScanStartedMs != 0 && millis() - wifiScanStartedMs > 12000U) {
      Serial.println("[HMI WiFi] scan timeout");
      WiFi.scanDelete();
      wifiScanRunning = false;
      wifiScanStartedMs = 0;
    }
    return;
  }
  wifiScanRunning = false;
  wifiScanStartedMs = 0;
  scanResultCount = 0;
  if (result <= 0) {
    WiFi.scanDelete();
    return;
  }
  const int limit = min(result, 10);
  for (int i = 0; i < limit; ++i) {
    const String name = WiFi.SSID(i);
    if (name.isEmpty()) continue;
    bool duplicate = false;
    for (uint8_t j = 0; j < scanResultCount; ++j) {
      if (name.equals(scanResults[j].ssid)) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;
    auto& item = scanResults[scanResultCount++];
    snprintf(item.ssid, sizeof(item.ssid), "%s", name.c_str());
    item.rssi = WiFi.RSSI(i);
    item.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  Serial.printf("[HMI WiFi] scan complete: %u network(s)\n",
                scanResultCount);
}
}  // namespace

namespace HmiOta {

void attachDataSources(MainLink& mainLink, ShotStore& shotStore,
                       ControlTraceStore& controlTraceStore,
                       HmiSettings& settings, HmiDisplay& display,
                       AudioService& audio, ScaleManager& scaleManager) {
  HmiWeb::attachDataSources(mainLink, shotStore, controlTraceStore, settings,
                            display, audio, scaleManager);
}

void begin() {
  WiFi.persistent(false);
  // ESP32-S3 Wi-Fi/BLE coexistence requires modem sleep. WIFI_PS_NONE
  // aborts in the Wi-Fi driver when Bluetooth is enabled (boot loop).
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t info) {
    wifiDisconnectReason = info.wifi_sta_disconnected.reason;
    wifiDisconnectObserved = true;
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.setHostname(HOSTNAME);
  startAccessPoint();


  preferences.begin("hmi_wifi", true);
  savedSsid = preferences.getString("ssid", "");
  savedPassword = preferences.getString("pass", "");
  autoConnect = preferences.getBool("auto", true);
  preferences.end();
  startSavedStationConnection(millis());

  HmiWeb::begin();
}

void poll() {
  HmiWeb::poll();
  updateWifiScan();
  const uint32_t nowMs = millis();
  if (wifiDisconnectObserved) {
    wifiDisconnectObserved = false;
    Serial.printf("[HMI WiFi] disconnected: %u (%s)\n", wifiDisconnectReason,
                  WiFi.disconnectReasonName(
                      static_cast<wifi_err_reason_t>(wifiDisconnectReason)));
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (!clockStarted) {
      // Background SNTP only. Shot durations still use Main's monotonic clock.
      configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
      clockStarted = true;
    }
    if (pendingStartedMs != 0) saveConnectedNetwork();
    if (connectedSinceMs == 0) connectedSinceMs = nowMs;
    disconnectedSinceMs = 0;
    nextReconnectMs = 0;
    reconnectDelayMs = WIFI_RECONNECT_INITIAL_MS;
    beginNetworkServices();
    if (apActive && nowMs - connectedSinceMs >= AP_CLOSE_DELAY_MS) {
      if (mdnsActive) {
        MDNS.end();
        mdnsActive = false;
      }
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      apActive = false;
    }
  } else {
    if (pendingStartedMs != 0 && nowMs - pendingStartedMs >= 15000U)
      restoreSavedNetwork(nowMs);
    else if (pendingStartedMs == 0 && autoConnect && !savedSsid.isEmpty() &&
             static_cast<int32_t>(nowMs - nextReconnectMs) >= 0)
      startSavedStationConnection(nowMs);
    connectedSinceMs = 0;
    if (disconnectedSinceMs == 0) disconnectedSinceMs = nowMs;
    if (!apActive && nowMs - disconnectedSinceMs >= AP_RESTORE_DELAY_MS)
      startAccessPoint();
  }
}

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }

bool wifiConnecting() { return pendingStartedMs != 0; }

const char* wifiStatusText() {
  if (wifiConnected()) return "CONNECTED";
  if (wifiConnecting()) return "CONNECTING...";
  if (lastConnectFailureMs != 0 && millis() - lastConnectFailureMs < 30000U)
    return "CONNECT FAILED";
  return "NOT CONNECTED";
}

bool autoConnectEnabled() { return autoConnect; }

void setAutoConnect(bool enabled) {
  autoConnect = enabled;
  preferences.begin("hmi_wifi", false);
  preferences.putBool("auto", autoConnect);
  preferences.end();
  if (autoConnect && !wifiConnected() && !wifiConnecting() &&
      !savedSsid.isEmpty()) {
    reconnectDelayMs = WIFI_RECONNECT_INITIAL_MS;
    startSavedStationConnection(millis());
    disconnectedSinceMs = millis();
  }
}

String localIp() {
  return wifiConnected() ? WiFi.localIP().toString() : String("0.0.0.0");
}

String apName() { return accessPointName; }
String apIp() { return apActive ? WiFi.softAPIP().toString() : String(); }

String ssid() {
  if (wifiConnected()) return WiFi.SSID();
  if (wifiConnecting()) return pendingSsid;
  return savedSsid;
}

int32_t rssi() { return wifiConnected() ? WiFi.RSSI() : -127; }

bool scanNetworks() {
  // A previous asynchronous scan can remain stuck after a connection attempt.
  // RESCAN always cancels it and starts a fresh scan so the button is
  // deterministic.
  if (wifiScanRunning || WiFi.scanComplete() == WIFI_SCAN_RUNNING)
    WiFi.scanDelete();
  wifiScanRunning = false;
  wifiScanStartedMs = 0;
  WiFi.scanDelete();
  scanResultCount = 0;
  const int result = WiFi.scanNetworks(true, true);
  wifiScanRunning = result == WIFI_SCAN_RUNNING;
  if (wifiScanRunning) {
    wifiScanStartedMs = millis();
    Serial.println("[HMI WiFi] scan requested");
  } else if (result >= 0) {
    // Defensive synchronous-result path for Arduino core variants.
    const int limit = min(result, 10);
    for (int i = 0; i < limit; ++i) {
      auto& item = scanResults[scanResultCount++];
      snprintf(item.ssid, sizeof(item.ssid), "%s", WiFi.SSID(i).c_str());
      item.rssi = WiFi.RSSI(i);
      item.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
  } else {
    Serial.printf("[HMI WiFi] scan start failed: %d\n", result);
  }
  return wifiScanRunning || result >= 0;
}

bool scanRunning() { return wifiScanRunning; }

const NetworkInfo* networks() { return scanResults; }

uint8_t networkCount() { return scanResultCount; }

bool connectWifi(const char* networkSsid, const char* password) {
  if (!networkSsid || !networkSsid[0]) return false;
  startStationConnection(String(networkSsid), String(password ? password : ""));
  return true;
}

void forgetWifi() {
  preferences.begin("hmi_wifi", false);
  preferences.clear();
  preferences.putBool("auto", autoConnect);
  preferences.end();
  WiFi.disconnect(true, true);
  savedSsid = "";
  savedPassword = "";
  pendingSsid = "";
  pendingPassword = "";
  pendingStartedMs = 0;
  connectedSinceMs = 0;
  disconnectedSinceMs = millis();
  startAccessPoint();
}

}  // namespace HmiOta
