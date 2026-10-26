#include "recovery_service.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "src/app/orchestrator.h"
#include "src/communication/hmi_link.h"
#include "src/hardware/valve_sense.h"
#include "src/recovery/recovery_portal.h"

WebServer recoveryServer(80);

namespace RecoveryService {
namespace {
constexpr uint32_t HMI_WARNING_DELAY_MS = 3000;
constexpr uint32_t RECOVERY_DELAY_MS = 60000;
constexpr uint32_t HMI_STABLE_EXIT_MS = 10000;
bool recoveryActive = false;
bool automaticRecovery = false;
bool mdnsActive = false;
uint32_t hmiLostSinceMs = 0;
uint32_t hmiCompatibleSinceMs = 0;
char recoveryName[32] = "BrewVector-Recovery";

bool safeToStartRecovery() {
  const State state = Orchestrator::getState();
  return (state == IDLE || state == SHOT_END) &&
         !ValveSense::isMachineValveActive();
}

void startRecovery(bool manual) {
  if (recoveryActive || !safeToStartRecovery()) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(WIFI_PS_NONE);
  if (!WiFi.softAP(recoveryName, RECOVERY_AP_PASS, 1, false, 2)) {
    Serial.println("[Recovery] AP start failed");
    return;
  }
  recoveryServer.begin();
  mdnsActive = MDNS.begin("brewvector-recovery");
  if (mdnsActive) MDNS.addService("http", "tcp", 80);
  recoveryActive = true;
  automaticRecovery = !manual;
  Serial.printf("[Recovery] %s AP started: %s, IP=%s\n",
                manual ? "manual" : "automatic", recoveryName,
                WiFi.softAPIP().toString().c_str());
}

void stopRecovery() {
  if (!recoveryActive || RecoveryPortal::updateActive()) return;
  recoveryServer.stop();
  if (mdnsActive) {
    MDNS.end();
    mdnsActive = false;
  }
  WiFi.softAPdisconnect(true);
  // Main owns no persistent network workload. Shut the radio down completely
  // after Recovery ends; the service will re-enable it if recovery is needed.
  WiFi.mode(WIFI_OFF);
  recoveryActive = false;
  automaticRecovery = false;
  if (!HmiLink::compatible()) hmiLostSinceMs = millis();
  Serial.println("[Recovery] AP stopped; Wi-Fi radio off");
}
}  // namespace

bool active() { return recoveryActive; }
const char* ssid() { return recoveryName; }

bool hmiWarningActive() {
  return !HmiLink::compatible() && hmiLostSinceMs != 0 &&
         millis() - hmiLostSinceMs >= HMI_WARNING_DELAY_MS;
}

bool hmiIncompatible() {
  return HmiLink::connected() && !HmiLink::compatible();
}

void begin() {
  RecoveryPortal::registerRoutes();
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(recoveryName, sizeof(recoveryName), "BrewVector-Recovery-%04X",
           static_cast<unsigned>(mac & 0xFFFF));
  hmiLostSinceMs = millis();
  Serial.println("[Recovery] routes ready; AP is OFF");
}

void toggle() {
  if (recoveryActive) stopRecovery();
  else startRecovery(true);
}

void poll() {
  const uint32_t nowMs = millis();
  const bool operational = HmiLink::compatible();
  if (operational) {
    hmiLostSinceMs = 0;
    if (hmiCompatibleSinceMs == 0) hmiCompatibleSinceMs = nowMs;
    if (recoveryActive && automaticRecovery &&
        nowMs - hmiCompatibleSinceMs >= HMI_STABLE_EXIT_MS)
      stopRecovery();
  } else {
    hmiCompatibleSinceMs = 0;
    if (hmiLostSinceMs == 0) hmiLostSinceMs = nowMs;
    if (!recoveryActive && nowMs - hmiLostSinceMs >= RECOVERY_DELAY_MS)
      startRecovery(false);
  }
  if (recoveryActive) recoveryServer.handleClient();
}
}  // namespace RecoveryService
