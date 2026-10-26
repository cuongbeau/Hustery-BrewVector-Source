#pragma once

#include <Arduino.h>
#include "local_access.h"
#include "web/owner_web_server.h"

class MainLink;
class ShotStore;
class ControlTraceStore;
class HmiSettings;
class HmiDisplay;
class AudioService;
class ScaleManager;

namespace HmiWebInternal {

constexpr char OTA_USER[] = "admin";
constexpr char OTA_PASSWORD[] = BREWVECTOR_OTA_PASSWORD;

extern OwnerWebServer server;
extern bool mainUploadOk;
extern MainLink* webMainLink;
extern ShotStore* webShotStore;
extern ControlTraceStore* webControlTraceStore;
extern HmiSettings* webSettings;
extern HmiDisplay* webDisplay;
extern AudioService* webAudio;
extern ScaleManager* webScaleManager;

String htmlEscape(const char* source);
String jsonEscape(const char* source);
const char* machineStateName(uint8_t raw);
bool authorized();

void registerDashboardRoutes();
void registerPumpDiagnosticRoutes();
void registerPressureDiagnosticRoutes();
void registerAdcProbeRoutes();
void registerHeaterRoutes();
void registerHistoryRoutes();
void registerFirmwareRoutes();

}  // namespace HmiWebInternal
