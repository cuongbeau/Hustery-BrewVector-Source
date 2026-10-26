#include "web/hmi_web_server.h"

#include <Arduino.h>
#include <WebServer.h>

#include "communication/main_link.h"
#include "storage/control_trace_store.h"
#include "web/hmi_web_internal.h"
#include "web/web_jobs.h"

namespace HmiWebInternal {

OwnerWebServer server(80);
bool mainUploadOk = false;
MainLink* webMainLink = nullptr;
ShotStore* webShotStore = nullptr;
ControlTraceStore* webControlTraceStore = nullptr;
HmiSettings* webSettings = nullptr;
HmiDisplay* webDisplay = nullptr;
AudioService* webAudio = nullptr;
ScaleManager* webScaleManager = nullptr;
String htmlEscape(const char* source) {
  String value(source ? source : "");
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  return value;
}

String jsonEscape(const char* source) {
  String value(source ? source : "");
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\r", "");
  value.replace("\n", "\\n");
  return value;
}

const char* machineStateName(uint8_t raw) {
  switch (static_cast<HmiProtocol::MachineState>(raw)) {
    case HmiProtocol::MachineState::Boot: return "BOOT";
    case HmiProtocol::MachineState::Heating: return "HEATING";
    case HmiProtocol::MachineState::Ready: return "READY";
    case HmiProtocol::MachineState::Brew: return "BREW";
    case HmiProtocol::MachineState::Flush: return "FLUSH";
    case HmiProtocol::MachineState::Ending: return "ENDING";
    case HmiProtocol::MachineState::Done: return "DONE";
    case HmiProtocol::MachineState::HotWater: return "HOT WATER";
    case HmiProtocol::MachineState::Settings: return "SETTINGS";
    case HmiProtocol::MachineState::Calibration: return "CALIBRATION";
    case HmiProtocol::MachineState::Fault: return "FAULT";
    default: return "UNKNOWN";
  }
}


bool authorized() {
  if (server.authenticate(OTA_USER, OTA_PASSWORD)) return true;
  server.requestAuthentication();
  return false;
}


}  // namespace HmiWebInternal

namespace HmiWeb {

void attachDataSources(MainLink& mainLink, ShotStore& shotStore,
                       ControlTraceStore& controlTraceStore,
                       HmiSettings& settings, HmiDisplay& display,
                       AudioService& audio, ScaleManager& scaleManager) {
  HmiWebInternal::webMainLink = &mainLink;
  HmiWebInternal::webShotStore = &shotStore;
  HmiWebInternal::webControlTraceStore = &controlTraceStore;
  HmiWebInternal::webSettings = &settings;
  HmiWebInternal::webDisplay = &display;
  HmiWebInternal::webAudio = &audio;
  HmiWebInternal::webScaleManager = &scaleManager;
}

void begin() {
  const char* headers[]={"If-None-Match"};
  HmiWebInternal::server.collectHeaders(headers,1);
  HmiWebInternal::registerDashboardRoutes();
  WebJobs::registerRoutes();
  HmiWebInternal::registerPressureDiagnosticRoutes();
  HmiWebInternal::registerAdcProbeRoutes();
  HmiWebInternal::registerHeaterRoutes();
  HmiWebInternal::registerHistoryRoutes();
  HmiWebInternal::registerFirmwareRoutes();
  HmiWebInternal::server.begin();
}

void poll() { HmiWebInternal::server.pollOwner(); }
void stop() { HmiWebInternal::server.stop(); }
void resume() { HmiWebInternal::server.resume(); }

}  // namespace HmiWeb
