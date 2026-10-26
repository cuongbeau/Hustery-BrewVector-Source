#include "recovery_portal.h"
#include "src/hardware/heater_control.h"

#include <Arduino.h>
#include <Update.h>

#include "interfaces/system_state.h"
#include "protocol/hmi_protocol.h"
#include "src/app/orchestrator.h"
#include "src/communication/hmi_link.h"
#include "src/hardware/scale_service.h"
#include "src/hardware/valve_sense.h"
#include "src/hardware/pump_control.h"
#include "src/recovery/recovery_service.h"

#ifndef BUILD_VERSION
#define BUILD_VERSION "Unknown"
#endif

namespace {
bool otaStarted = false;
bool otaFailed = false;

bool safeForMaintenance() {
  const State state = Orchestrator::getState();
  return (state == IDLE || state == SHOT_END) &&
         !ValveSense::isMachineValveActive() && !HmiLink::firmwareUpdateActive();
}

void handleRoot() {
  String page;
  page.reserve(4200);
  page += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>BrewVector Recovery</title><style>:root{color-scheme:dark;--bg:#07111b;--card:#0e2233;--line:#294052;--cyan:#21d4ea;--red:#ff665b;--muted:#91a4b5}*{box-sizing:border-box}body{font-family:system-ui;background:var(--bg);color:#f3f7fa;margin:0;padding:20px}main{max-width:620px;margin:auto}.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:18px;margin:14px 0}h1{font-size:24px;margin:0 0 6px}h2{font-size:17px;color:var(--cyan)}.muted{color:var(--muted)}button{padding:11px 15px;border-radius:8px;border:1px solid var(--cyan);background:#102a3c;color:var(--cyan);font-weight:750;margin-top:12px}button.red{border-color:var(--red);color:var(--red)}input{max-width:100%;padding:10px;border:1px dashed var(--line);border-radius:8px}.ok{color:#53d578}.bad{color:var(--red)}code{color:var(--cyan)}</style></head><body><main>");
  page += F("<div class='muted'>ESP32 MAIN CONTROLLER</div><h1>BrewVector Recovery</h1><p class='muted'>Emergency maintenance only. Brew control remains on the physical controller.</p>");
  page += F("<section class='card'><h2>Status</h2><div id='status'>Loading...</div></section>");
  page += F("<section class='card'><h2>Update Main firmware</h2><p class='muted'>Machine must be idle and the valve must be off. Upload the Main <code>firmware.bin</code>.</p><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin' required><br><button type='submit'>UPLOAD & UPDATE</button></form></section>");
  page += F("<section class='card'><h2>Connection checklist</h2><ol class='muted'><li>Check common GND.</li><li>HMI TX must connect to Main RX.</li><li>HMI RX must connect to Main TX.</li><li>Restart HMI, then Main.</li></ol><form method='POST' action='/reboot'><button class='red'>REBOOT MAIN</button></form></section>");
  page += F("<script>async function poll(){try{const r=await fetch('/status',{cache:'no-store'}),s=await r.json();document.getElementById('status').innerHTML='<b class=\"'+(s.safe?'ok':'bad')+'\">'+(s.safe?'READY FOR OTA':'OTA LOCKED')+'</b><br>Firmware: '+s.firmware+'<br>Protocol: '+s.protocol+' / HMI '+s.hmiProtocol+'<br>HMI: '+s.hmi+'<br>State: '+s.state+'<br>Uptime: '+s.uptime+' s';}catch(e){document.getElementById('status').textContent='Status unavailable';}setTimeout(poll,1000)}poll()</script></main></body></html>");
  recoveryServer.send(200, "text/html; charset=utf-8", page);
}

const char* stateName(State state) {
  switch (state) {
    case IDLE: return "IDLE";
    case PROFILE_RUN: return "BREW";
    case FLUSH_RUN: return "FLUSH";
    case SHOT_END: return "SHOT END";
    case HOTWATER_RUN: return "HOT WATER";
    case SETTINGS_MENU: return "SETTINGS";
    case CALIBRATION_RUN: return "PRESSURE CAL";
    default: return "UNKNOWN";
  }
}

void handleStatus() {
  String json;
  json.reserve(480);
  json += F("{\"safe\":");
  json += safeForMaintenance() ? F("true") : F("false");
  json += F(",\"firmware\":\""); json += BUILD_VERSION;
  json += F("\",\"protocol\":"); json += HmiProtocol::SCHEMA_REVISION;
  json += F(",\"hmiProtocol\":"); json += HmiLink::remoteSchemaRevision();
  json += F(",\"hmi\":\"");
  json += HmiLink::compatible() ? F("ONLINE")
          : HmiLink::connected() ? F("INCOMPATIBLE") : F("OFFLINE");
  json += F("\",\"hmiFrameAgeMs\":"); json += HmiLink::lastFrameAgeMs();
  json += F(",\"uartValidFrames\":"); json += HmiLink::validFrameCount();
  json += F(",\"uartCrcErrors\":"); json += HmiLink::crcErrorCount();
  json += F(",\"uartFramingErrors\":");
  json += HmiLink::framingErrorCount();
  json += F(",\"uartStaleResets\":");
  json += HmiLink::staleFrameResetCount();
  json += F(",\"uartReconnects\":"); json += HmiLink::reconnectCount();
  json += F(",\"uartLastErrorAgeMs\":"); json += HmiLink::lastErrorAgeMs();
  json += F(",\"scaleSource\":\"");
  json += HmiProtocol::scaleSourceName(ScaleService::source());
  json += '"';
  json += F(",\"scaleOnline\":");
  json += ScaleService::isOnline() ? F("true") : F("false");
  json += F(",\"state\":\""); json += stateName(Orchestrator::getState());
  json += F("\",\"uptime\":"); json += millis() / 1000U;
  json += '}';
  recoveryServer.sendHeader("Cache-Control", "no-store");
  recoveryServer.send(200, "application/json", json);
}

void handleUpload() {
  HTTPUpload& upload = recoveryServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    otaStarted = false;
    otaFailed = false;
    if (!safeForMaintenance()) {
      otaFailed = true;
      return;
    }
    PumpPSM::cancelStoppedReset();
    Heater::inhibit();
    otaStarted = Update.begin(UPDATE_SIZE_UNKNOWN);
    otaFailed = !otaStarted;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (otaStarted && !otaFailed &&
        Update.write(upload.buf, upload.currentSize) != upload.currentSize)
      otaFailed = true;
  } else if (upload.status == UPLOAD_FILE_END) {
    if (otaStarted && !otaFailed) otaFailed = !Update.end(true);
    else if (Update.isRunning()) Update.abort();
    otaStarted = false;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (Update.isRunning()) Update.abort();
    otaStarted = false;
    otaFailed = true;
  }
}

void handleUploadDone() {
  if (otaFailed || Update.hasError()) {
    recoveryServer.send(409, "text/plain",
                "OTA rejected or failed. Machine must be idle.");
    return;
  }
  recoveryServer.send(200, "text/plain", "OTA OK. Rebooting Main...");
  delay(350);
  ESP.restart();
}

void handleReboot() {
  if (!safeForMaintenance()) {
    recoveryServer.send(409, "text/plain", "Reboot locked while machine is active");
    return;
  }
  recoveryServer.send(200, "text/plain", "Rebooting Main...");
  delay(250);
  ESP.restart();
}
}  // namespace

namespace RecoveryPortal {

void registerRoutes() {
  recoveryServer.on("/", HTTP_GET, handleRoot);
  recoveryServer.on("/status", HTTP_GET, handleStatus);
  recoveryServer.on("/update", HTTP_GET, handleRoot);
  recoveryServer.on("/update", HTTP_POST, handleUploadDone, handleUpload);
  recoveryServer.on("/reboot", HTTP_POST, handleReboot);
  recoveryServer.onNotFound(handleRoot);
}

bool updateActive() { return otaStarted || Update.isRunning(); }

}  // namespace RecoveryPortal
