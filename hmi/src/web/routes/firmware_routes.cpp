#include "web/hmi_web_internal.h"



#include <Update.h>
#include "communication/main_link.h"



#include "update/main_firmware_updater.h"



using namespace HmiWebInternal;



namespace {
bool hmiUploadAccepted = false, hmiUploadComplete = false;
String hmiUploadError;
void handleUpdatePage() {

  server.send(200, "text/html; charset=utf-8",

      "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"

      "<style>body{font-family:system-ui;background:#07111b;color:#fff;padding:24px}"

      "main{max-width:500px;margin:auto;background:#0e2233;padding:20px;border-radius:12px}"

      "button{padding:10px;margin-top:14px}</style></head><body><main>"

      "<h2>HMI Firmware Update</h2><form method='POST' action='/update' enctype='multipart/form-data'>"

      "<input type='file' name='firmware' accept='.bin' required><br>"

      "<button type='submit'>UPLOAD & REBOOT</button></form></main></body></html>");

}



void handleUpdateUpload() {

  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {

    hmiUploadAccepted = false; hmiUploadComplete = false;
    hmiUploadError = "";
    // Old non-heater Main remains updatable; a heater-capable Main must
    // acknowledge OFF (or a latched, output-OFF fault) before HMI OTA.
    if (!webMainLink) { hmiUploadError="Main link unavailable"; return; }
    webMainLink->poll();
    if (!webMainLink->compatible()) { hmiUploadError="Main offline or application schema incompatible"; return; }
    if (MainFirmwareUpdater::busy()) { hmiUploadError="Main firmware update still busy"; return; }
    if (webMainLink->supportsHeater()) {
      if (!webMainLink->heaterFresh()) { hmiUploadError="No fresh compatible heater status. Check Main/HMI heater protocol versions"; return; }
      if (webMainLink->heaterCommandPending()) { hmiUploadError="Waiting for Main heater command acknowledgement"; return; }
      const auto& heater=webMainLink->heaterStatus();
      if (heater.relay || heater.power!=0 ||
          (heater.mode!=HeaterWire::Mode::Off && heater.mode!=HeaterWire::Mode::Fault)) {
        hmiUploadError="Heater active. Press OFF and wait for Main acknowledgement before HMI OTA"; return;
      }
    }
    hmiUploadAccepted = true;
    Serial.printf("[HMI OTA] Start %s\n", upload.filename.c_str());

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { hmiUploadAccepted = false; hmiUploadError=Update.errorString(); Update.printError(Serial); }

  } else if (upload.status == UPLOAD_FILE_WRITE) {

    if (!hmiUploadAccepted) return;
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      hmiUploadError=Update.errorString(); hmiUploadAccepted=false;
      Update.printError(Serial); Update.abort();
    }

  } else if (upload.status == UPLOAD_FILE_END) {

    if (!hmiUploadAccepted) return;
    hmiUploadComplete = Update.end(true);
    if (!hmiUploadComplete) { hmiUploadError=Update.errorString(); Update.printError(Serial); }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {

    hmiUploadAccepted = hmiUploadComplete = false;
    hmiUploadError="Upload aborted before completion";
    Update.abort();

  }

}



void handleUpdateDone() {

  if (!hmiUploadAccepted || !hmiUploadComplete || Update.hasError()) {

    server.send(500, "text/plain", String("HMI OTA rejected/failed: ") +
        (hmiUploadError.length() ? hmiUploadError : String("Image incomplete or update failure")));

    return;

  }

  server.send(200, "text/plain", "HMI OTA OK. Rebooting...");

  server.restartAfterResponse();

}



String firmwarePageHtml() {

  String html;

  html.reserve(5200);

  html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");

  html += F("<title>BrewVector Firmware</title><style>");

  html += F("body{font-family:system-ui;background:#07111b;color:#f3f7fa;margin:0;padding:24px}main{max-width:680px;margin:auto}");

  html += F(".card{background:#0e2233;border:1px solid #294052;border-radius:12px;padding:20px;margin:14px 0}h1{font-size:24px}h2{font-size:18px;color:#19cfe8}");

  html += F("button,a.btn{display:inline-block;padding:11px 16px;margin:12px 8px 0 0;border:1px solid #19cfe8;border-radius:7px;background:#0e2233;color:#19cfe8;text-decoration:none;font-weight:700}");

  html += F("button.danger{border-color:#ff5b4d;color:#ff7569}button:disabled{opacity:.4}.muted{color:#91a4b5}.bar{height:13px;background:#07111b;border-radius:8px;overflow:hidden;margin-top:12px}.fill{height:100%;width:0;background:#19cfe8}.err{color:#ff7569}</style></head><body><main>");

  html += F("<a class='btn' href='/'>BACK</a><h1>Firmware Center</h1>");

  html += F("<div class='card'><h2>ESP32 Main Controller</h2><p class='muted'>Machine must be READY/IDLE. The file is staged on TF, verified, then sent over UART with retry and SHA-256 verification.</p>");

  html += F("<form method='POST' action='/main-update' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin' required><br><button type='submit'>1. UPLOAD MAIN .BIN TO TF</button></form>");

  html += F("<div class='bar'><div class='fill' id='fill'></div></div><p id='status'>Loading status...</p><p class='err' id='error'></p>");

  html += F("<button id='install' onclick='installMain()'>2. INSTALL ON MAIN</button><button class='danger' onclick='cancelMain()'>CANCEL</button></div>");

  html += F("<div class='card'><h2>ESP32-S3 HMI</h2><p class='muted'>Updates this touch display directly. It will reboot when complete.</p><a class='btn' href='/update'>UPDATE HMI FIRMWARE</a></div>");

  html += F("<script>async function refresh(){try{let r=await fetch('/main-update/status',{cache:'no-store'}),s=await r.json();document.getElementById('fill').style.width=s.progress+'%';document.getElementById('status').textContent=s.state+' | '+s.progress+'% | '+Math.round(s.size/1024)+' KiB';document.getElementById('error').textContent=s.error||'';document.getElementById('install').disabled=!s.ready||s.busy;}catch(e){document.getElementById('error').textContent='Status unavailable';}}async function installMain(){await fetch('/main-update/start',{method:'POST'});refresh();}async function cancelMain(){await fetch('/main-update/cancel',{method:'POST'});refresh();}setInterval(refresh,700);refresh();</script>");

  html += F("</main></body></html>");

  return html;

}



void handleFirmwarePage() {

  server.send(200, "text/html; charset=utf-8", firmwarePageHtml());

}



void handleMainUpdateUpload() {

  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {

    mainUploadOk = MainFirmwareUpdater::beginUpload(upload.filename);

  } else if (upload.status == UPLOAD_FILE_WRITE) {

    if (mainUploadOk)

      mainUploadOk = MainFirmwareUpdater::writeUpload(upload.buf, upload.currentSize);

  } else if (upload.status == UPLOAD_FILE_END) {

    if (mainUploadOk) mainUploadOk = MainFirmwareUpdater::finishUpload();

    else MainFirmwareUpdater::abortUpload();

  } else if (upload.status == UPLOAD_FILE_ABORTED) {

    mainUploadOk = false;

    MainFirmwareUpdater::abortUpload();

  }

}



void handleMainUpdateDone() {

  if (!mainUploadOk) {

    server.send(400, "text/plain", MainFirmwareUpdater::errorText());

    return;

  }

  server.sendHeader("Location", "/firmware", true);

  server.send(303, "text/plain", "Firmware staged");

}



void handleMainUpdateStatus() {

  server.sendHeader("Cache-Control", "no-store");

  server.send(200, "application/json", MainFirmwareUpdater::statusJson());

}



void handleMainUpdateStart() {

  const bool started = MainFirmwareUpdater::startTransfer();

  server.send(started ? 202 : 409, "application/json",

              MainFirmwareUpdater::statusJson());

}



void handleMainUpdateCancel() {

  MainFirmwareUpdater::cancelTransfer();

  server.send(200, "application/json", MainFirmwareUpdater::statusJson());

}



}  // namespace



namespace HmiWebInternal {



void registerFirmwareRoutes() {

  server.on("/update", HTTP_GET, handleUpdatePage);

  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);

  server.on("/firmware", HTTP_GET, handleFirmwarePage);

  server.on("/main-update", HTTP_POST, handleMainUpdateDone,

            handleMainUpdateUpload);

  server.on("/main-update/status", HTTP_GET, handleMainUpdateStatus);

  server.on("/main-update/start", HTTP_POST, handleMainUpdateStart);

  server.on("/main-update/cancel", HTTP_POST, handleMainUpdateCancel);

}



}  // namespace HmiWebInternal
