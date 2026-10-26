#pragma once

#include <Arduino.h>

class MainLink;
class ShotStore;
class ControlTraceStore;
class HmiSettings;
class HmiDisplay;
class AudioService;
class ScaleManager;

namespace HmiOta {

struct NetworkInfo {
  char ssid[33] = {};
  int32_t rssi = -127;
  bool secure = false;
};

void begin();
void attachDataSources(MainLink& mainLink, ShotStore& shotStore,
                       ControlTraceStore& controlTraceStore,
                       HmiSettings& settings, HmiDisplay& display,
                       AudioService& audio, ScaleManager& scaleManager);
void poll();
bool wifiConnected();
bool wifiConnecting();
const char* wifiStatusText();
bool autoConnectEnabled();
void setAutoConnect(bool enabled);
String localIp();
String apName();
String apIp();
String ssid();
int32_t rssi();
bool scanNetworks();
bool scanRunning();
const NetworkInfo* networks();
uint8_t networkCount();
bool connectWifi(const char* ssid, const char* password);
void forgetWifi();

}  // namespace HmiOta
