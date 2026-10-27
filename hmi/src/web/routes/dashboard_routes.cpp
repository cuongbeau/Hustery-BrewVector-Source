#include "web/hmi_web_internal.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <stdlib.h>
#include "thermal_calibration.h"
#include <string.h>
#include "web/web_root_gzip.h"
#include "web/owner_call.h"
#include "web/web_jobs.h"
#include "app/runtime_metrics.h"

#include "app/hmi_settings.h"
#include "communication/main_link.h"
#include "hardware/audio_service.h"
#include "hardware/hmi_display.h"
#include "network/ota_service.h"
#include "scale/scale_manager.h"
#include "storage/shot_store.h"
#include "storage/control_trace_store.h"
#include "web/calibration_web_bridge.h"
#include "web/profile_web_bridge.h"
#include "web/web_assets.h"
#include "web/legacy_hmi_extension.h"

using namespace HmiWebInternal;

namespace {
String pageHtml() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  String html;
  html.reserve(2300);
  html += F("<!doctype html><html><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>BrewVector HMI</title><style>");
  html += F("body{font-family:system-ui;background:#07111b;color:#f3f7fa;margin:0;padding:24px}");
  html += F("main{max-width:520px;margin:auto}.card{background:#0e2233;border:1px solid #294052;border-radius:12px;padding:18px;margin:14px 0}");
  html += F("h1{font-size:23px}h2{font-size:17px;color:#19cfe8}label{display:block;color:#91a4b5;margin-top:10px}");
  html += F("input{box-sizing:border-box;width:100%;padding:11px;margin-top:5px;border-radius:7px;border:1px solid #294052;background:#07111b;color:#fff}");
  html += F("button,a.btn{display:inline-block;padding:11px 16px;margin-top:14px;border:1px solid #19cfe8;border-radius:7px;background:#0e2233;color:#19cfe8;text-decoration:none;font-weight:700}");
  html += F(".ok{color:#4bc86a}.warn{color:#ffb21c}.muted{color:#91a4b5;font-size:13px}</style></head><body><main>");
  html += F("<h1>BrewVector HMI</h1><div class='card'><h2>Network</h2>");
  html += connected ? F("<div class='ok'>CONNECTED</div>")
                    : F("<div class='warn'>NOT CONNECTED</div>");
  html += F("<p>STA SSID: <b>");
  html += connected ? WiFi.SSID() : String("-");
  html += F("</b><br>STA IP: <b>");
  html += connected ? WiFi.localIP().toString() : String("-");
  html += F("</b><br>AP: <b>");
  html += HmiOta::apName();
  html += F("</b><br>AP IP: <b>192.168.4.1</b></p></div>");
  html += F("<div class='card'><h2>Configure Wi-Fi</h2><form method='POST' action='/wifi'>");
  html += F("<label>SSID</label><input name='ssid' maxlength='32' required>");
  html += F("<label>Password</label><input name='pass' type='password' maxlength='64'>");
  html += F("<button type='submit'>SAVE & CONNECT</button></form></div>");
  html += F("<div class='card'><h2>Firmware</h2><p class='muted'>Update either controller from this HMI.</p>");
  html += F("<a class='btn' href='/firmware'>OPEN FIRMWARE CENTER</a></div>");
  html += F("</main></body></html>");
  return html;
}

String dashboardPageHtml() {
  String html;
  html.reserve(9000);
  html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>BrewVector</title><style>");
  html += F(":root{--bg:#07111b;--panel:#0e2233;--line:#294052;--cyan:#21d4ea;--amber:#ffb21c;--text:#f3f7fa;--muted:#91a4b5;--green:#4bc86a}*{box-sizing:border-box}body{font-family:system-ui;background:var(--bg);color:var(--text);margin:0;padding:18px}main{max-width:1040px;margin:auto}.top{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}.nav a{display:inline-block;color:var(--cyan);text-decoration:none;border:1px solid var(--line);border-radius:8px;padding:9px 12px;margin-left:6px}.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin:16px 0}.metric,.card{background:var(--panel);border:1px solid var(--line);border-radius:12px}.metric{padding:14px}.label{font-size:11px;color:var(--muted);letter-spacing:.08em}.value{font-size:27px;font-weight:750;margin-top:3px}.unit{font-size:13px;color:var(--muted)}.card{padding:16px}.status{color:var(--green);font-weight:750}.offline{color:#ff7569}.phase{display:flex;justify-content:space-between;gap:10px;align-items:center;margin-bottom:10px}.badge{background:#163247;border-radius:999px;padding:6px 11px;color:var(--cyan);font-weight:700}canvas{width:100%;height:420px;display:block;background:#081722;border-radius:8px}.legend{display:flex;gap:18px;flex-wrap:wrap;color:var(--muted);font-size:12px;margin:10px 0 0}.dot{display:inline-block;width:18px;border-top:3px solid;margin-right:6px}.cyan{border-color:var(--cyan)}.amber{border-color:var(--amber)}.dashed{border-top-style:dashed}@media(max-width:720px){.grid{grid-template-columns:repeat(2,1fr)}canvas{height:310px}.nav a{margin:4px 0 0 4px}.value{font-size:23px}}</style></head><body><main>");
  html += F("<div class='top'><div><div class='label'>ESPRESSO CONTROLLER</div><h1 style='margin:2px 0'>BrewVector HMI</h1></div><nav class='nav'><a href='/history'>SHOT HISTORY</a><a href='/network'>WI-FI</a><a href='/heater'>HEATER</a><a href='/firmware'>FIRMWARE</a></nav></div>");
  html += F("<div class='grid'><div class='metric'><div class='label'>PRESSURE</div><div class='value'><span id='pressure'>0.0</span> <span class='unit'>bar</span></div></div><div class='metric'><div class='label'>PUMP FLOW</div><div class='value'><span id='flow'>0.0</span> <span class='unit'>ml/s</span></div></div><div class='metric'><div class='label'>WEIGHT</div><div class='value'><span id='weight'>0.0</span> <span class='unit'>g</span></div></div><div class='metric'><div class='label'>TIME</div><div class='value'><span id='time'>0.0</span> <span class='unit'>s</span></div></div></div>");
  html += F("<section class='card'><div class='phase'><div><span id='link' class='offline'>MAIN OFFLINE</span><div id='profile' style='font-size:20px;font-weight:750;margin-top:5px'>OFF</div><div id='phase' class='label'>IDLE</div></div><div id='state' class='badge'>BOOT</div></div><canvas id='chart'></canvas><div class='legend'><span><i class='dot cyan'></i>Pressure actual</span><span><i class='dot cyan dashed'></i>Pressure target</span><span><i class='dot amber'></i>Pump Flow</span><span><i class='dot amber dashed'></i>Flow target</span></div></section>");
  html += F("<script>const c=document.getElementById('chart'),x=c.getContext('2d');let pts=[],lastTs=-1,lastElapsed=0;function fit(){let d=devicePixelRatio||1,r=c.getBoundingClientRect();c.width=Math.max(320,r.width*d);c.height=Math.max(220,r.height*d);x.setTransform(d,0,0,d,0,0);draw()}addEventListener('resize',fit);function line(key,color,dash,maxY,w,h,l,t){x.beginPath();x.setLineDash(dash?[6,5]:[]);x.strokeStyle=color;x.lineWidth=dash?1.5:2;pts.forEach((p,i)=>{let px=l+(p.t/Math.max(35,lastElapsed))*w,py=t+(1-Math.max(0,Math.min(maxY,p[key]))/maxY)*h;i?x.lineTo(px,py):x.moveTo(px,py)});x.stroke()}function draw(){let d=devicePixelRatio||1,W=c.width/d,H=c.height/d,l=42,r=42,t=16,b=34,w=W-l-r,h=H-t-b;x.clearRect(0,0,W,H);x.font='11px system-ui';x.fillStyle='#91a4b5';x.strokeStyle='#294052';x.lineWidth=1;x.setLineDash([]);for(let i=0;i<=4;i++){let y=t+h*i/4;x.beginPath();x.moveTo(l,y);x.lineTo(l+w,y);x.stroke();x.fillText((12-i*3).toString(),8,y+4);x.fillText((6-i*1.5).toFixed(i==4?0:1),W-32,y+4)}for(let i=0;i<=7;i++){let xx=l+w*i/7;x.fillText(Math.round(Math.max(35,lastElapsed)*i/7),xx-5,H-10)}line('p','#21d4ea',false,12,w,h,l,t);line('pt','#21d4ea',true,12,w,h,l,t);line('f','#ffb21c',false,6,w,h,l,t);line('ft','#ffb21c',true,6,w,h,l,t);x.fillStyle='#91a4b5';x.fillText('bar',8,12);x.fillText('ml/s',W-32,12)}async function poll(){try{let q=await fetch('/api/live',{cache:'no-store'}),s=await q.json();for(let k of ['pressure','flow','weight','time'])document.getElementById(k).textContent=s[k].toFixed(1);document.getElementById('profile').textContent=s.profile;document.getElementById('phase').textContent=s.phase+' | STEP '+s.step+'/'+s.steps;document.getElementById('state').textContent=s.state;let e=document.getElementById('link');e.textContent=s.online?'MAIN ONLINE':'MAIN OFFLINE';e.className=s.online?'status':'offline';if(s.elapsed+0.3<lastElapsed||(['READY','DONE','BOOT'].includes(s.state)&&lastElapsed>0))pts=[];lastElapsed=s.elapsed;if(s.online&&s.timestamp!==lastTs&&(s.state==='BREW'||s.state==='FLUSH'||s.state==='HOT WATER')){lastTs=s.timestamp;pts.push({t:s.elapsed,p:s.pressure,pt:s.targetPressure,f:s.flow,ft:s.targetFlow});if(pts.length>720)pts.shift()}draw()}catch(e){document.getElementById('link').textContent='WEB DATA ERROR'}setTimeout(poll,250)}fit();poll();</script>");
  html += F("</main></body></html>");
  return html;
}

void handleRoot() {
  server.sendHeader("ETag", HMI_WEB_ETAG);
  server.sendHeader("Cache-Control", "no-cache");
  if(server.header("If-None-Match")==HMI_WEB_ETAG){server.send(304);return;}
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200,"text/html; charset=utf-8",reinterpret_cast<const char*>(HMI_WEB_GZIP),sizeof(HMI_WEB_GZIP));
}
void handleNewDashboard() {
  server.send(200, "text/html; charset=utf-8", dashboardPageHtml());
}

void handleNetworkPage() {
  server.send(200, "text/html; charset=utf-8", pageHtml());
}

void handleLiveApi() {
  HmiProtocol::TelemetryPayload data{};
  const bool online = webMainLink && webMainLink->connected() &&
                      webMainLink->copyTelemetry(data);
  String json;
  json.reserve(760);
  json += F("{\"online\":"); json += online ? F("true") : F("false");
  json += F(",\"compatible\":");
  json += webMainLink && webMainLink->compatible() ? F("true") : F("false");
  json += F(",\"mainSchema\":");
  json += webMainLink ? webMainLink->remoteSchemaRevision() : 0;
  json += F(",\"controlTraceSupported\":");
  json += webMainLink && webMainLink->supportsControlTrace() ? F("true")
                                                              : F("false");
  json += F(",\"frameAgeMs\":");
  json += webMainLink ? webMainLink->lastFrameAgeMs() : UINT32_MAX;
  json += F(",\"mainSeesHmi\":");
  json += (data.flags & HmiProtocol::TelemetryFlags::HmiPeerConnected)
              ? F("true") : F("false");
  json += F(",\"mainSeesHmiCompatible\":");
  json += (data.flags & HmiProtocol::TelemetryFlags::HmiPeerCompatible)
              ? F("true") : F("false");
  json += F(",\"uartValidFrames\":");
  json += webMainLink ? webMainLink->validFrameCount() : 0;
  json += F(",\"uartCrcErrors\":");
  json += webMainLink ? webMainLink->crcErrorCount() : 0;
  json += F(",\"uartFramingErrors\":");
  json += webMainLink ? webMainLink->framingErrorCount() : 0;
  json += F(",\"uartStaleResets\":");
  json += webMainLink ? webMainLink->staleFrameResetCount() : 0;
  json += F(",\"uartReconnects\":");
  json += webMainLink ? webMainLink->reconnectCount() : 0;
  json += F(",\"uartLastErrorAgeMs\":");
  json += webMainLink ? webMainLink->lastErrorAgeMs() : UINT32_MAX;
  json += F(",\"timestamp\":"); json += data.timestampMs;
  json += F(",\"state\":\""); json += machineStateName(data.machineState); json += '"';
  json += F(",\"pressure\":"); json += String(data.pressureBar, 3);
  json += F(",\"flow\":"); json += String(data.flowGps, 3);
  json += F(",\"weight\":"); json += String(data.weightG, 3);
  json += F(",\"scaleConnected\":");
  json += (data.flags & HmiProtocol::TelemetryFlags::ScaleConnected)
              ? F("true") : F("false");
  json += F(",\"scaleStable\":");
  json += (data.flags & HmiProtocol::TelemetryFlags::ScaleStable)
              ? F("true") : F("false");
  json += F(",\"scaleSource\":\"");
  json += HmiProtocol::scaleSourceName(data.scaleSource);
  json += '"';
  json += F(",\"scaleBattery\":");
  json += data.scaleBatteryPercent <= 100
              ? static_cast<int>(data.scaleBatteryPercent) : -1;
  json += F(",\"scaleSimulator\":");
  json += webScaleManager && webScaleManager->simulatorEnabled()
              ? F("true") : F("false");
  json += F(",\"time\":"); json += String(data.elapsedS, 3);
  json += F(",\"elapsed\":"); json += String(data.elapsedS, 3);
  json += F(",\"targetPressure\":");
  json += String(data.controlMode == 0 ? data.targetValue : 0.0f, 3);
  json += F(",\"targetFlow\":");
  json += String(data.controlMode == 1 ? data.targetValue : 0.0f, 3);
  json += F(",\"step\":"); json += static_cast<unsigned>(data.stepIndex + 1);
  json += F(",\"steps\":"); json += static_cast<unsigned>(data.stepCount);
  json += F(",\"profile\":\""); json += jsonEscape(data.profileName); json += '"';
  json += F(",\"phase\":\""); json += jsonEscape(data.phaseName); json += F("\"}");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

const char* legacyStateName(uint8_t raw) {
  switch (static_cast<HmiProtocol::MachineState>(raw)) {
    case HmiProtocol::MachineState::Brew: return "PROFILE_RUN";
    case HmiProtocol::MachineState::Flush: return "FLUSH_RUN";
    case HmiProtocol::MachineState::Done:
    case HmiProtocol::MachineState::Ending: return "SHOT_END";
    case HmiProtocol::MachineState::HotWater: return "HOTWATER_RUN";
    case HmiProtocol::MachineState::Settings: return "SETTINGS_MENU";
    default: return "IDLE";
  }
}

void handleLegacyDashLive() {
  HmiProtocol::TelemetryPayload data{};
  const bool online = webMainLink && webMainLink->connected() &&
                      webMainLink->copyTelemetry(data);
  const auto state = static_cast<HmiProtocol::MachineState>(data.machineState);
  const bool running = state == HmiProtocol::MachineState::Brew ||
                       state == HmiProtocol::MachineState::Flush ||
                       state == HmiProtocol::MachineState::HotWater;
  JsonDocument doc;
  uint8_t currentProfile = 0;
  if (webMainLink && webMainLink->profileCount() > 0)
    currentProfile = webMainLink->profiles()[0].currentIndex;
  const float pressure = isfinite(data.pressureBar) && fabsf(data.pressureBar) > 0.0001f
                             ? data.pressureBar : 0.0f;
  const float flow = isfinite(data.flowGps) && fabsf(data.flowGps) > 0.0001f
                         ? data.flowGps : 0.0f;
  doc["ok"] = online;
  doc["boilerTemp"] = data.brewTempC;
  doc["blockTempC"] = data.blockTempC;
  doc["pathTempEstC"] = data.brewTempC;
  doc["brewTempEstC"] = data.brewTempC;
  doc["thermalLoadG"] = 0.0f;
  doc["state"] = legacyStateName(data.machineState);
  doc["machineState"] = machineStateName(data.machineState);
  doc["isEnding"] = state == HmiProtocol::MachineState::Ending;
  doc["isDone"] = state == HmiProtocol::MachineState::Done;
  doc["frameAgeMs"] = webMainLink ? webMainLink->lastFrameAgeMs() : UINT32_MAX;
  doc["heaterFresh"] = webMainLink && webMainLink->heaterFresh();
  doc["heaterReady"] = webMainLink && webMainLink->heaterFresh() && webMainLink->heaterStatus().ready;
  doc["heaterMode"] = webMainLink ? static_cast<int>(webMainLink->heaterStatus().mode) : 0;
  doc["compatible"] = webMainLink && webMainLink->compatible();
  doc["isBrewing"] = running;
  doc["pressure"] = pressure;
  doc["target"] = data.targetValue;
  doc["flow"] = flow;
  doc["weight"] = data.weightG;
  doc["hasScale"] =
      (data.flags & HmiProtocol::TelemetryFlags::ScaleConnected) != 0;
  doc["profileName"] = data.profileName;
  doc["t"] = data.elapsedS;
  doc["pumpPower"] = data.pumpPercent;
  doc["profileIdx"] = currentProfile;
  doc["manualDuty"] = static_cast<int>(data.pumpPercent);
  doc["autoYield"] = data.quickYieldG;
  doc["curStep"] = static_cast<unsigned>(data.stepIndex + 1);
  doc["totStep"] = static_cast<unsigned>(max<uint8_t>(1, data.stepCount));
  doc["sMode"] = data.controlMode;
  doc["sName"] = data.phaseName;
  doc["sTime"] = static_cast<uint32_t>(data.elapsedS * 1000.0f);
  doc["sMax"] = 0;
  if(webMainLink && webMainLink->brewStream().available()){
    const auto& stream=webMainLink->brewStream();
    doc["streamBoot"]=webMainLink->streamBoot();doc["streamSession"]=stream.session();
    const bool same=server.arg("boot")==String(webMainLink->streamBoot()) &&
        server.arg("session")==String(stream.session());
    const uint32_t after=same?strtoul(server.arg("after").c_str(),nullptr,10):0;
    JsonArray rows=doc["samples"].to<JsonArray>();
    uint32_t cursor=after;size_t sent=0;bool more=false;
    doc["streamGap"]=stream.size() && after && stream.at(0).seq>after+1;
    for(size_t i=0;i<stream.size();++i){
      const auto& s=stream.at(i);if(s.seq<=after)continue;
      if(sent==64){more=true;break;}
      JsonArray row=rows.add<JsonArray>();
      row.add(s.seq);row.add(s.t);row.add(s.p);row.add(s.pt);row.add(s.flow);
      row.add(s.wt);row.add(s.temp);row.add(s.mode);cursor=s.seq;++sent;
    }
    doc["streamCursor"]=cursor;doc["streamMore"]=more;
  }
  String output;
  serializeJson(doc, output);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", output);
}

void handleLegacyDashProfiles() {
  if (webMainLink && webMainLink->connected() &&
      webMainLink->profileCount() == 0) {
    webMainLink->requestProfiles();
  }
  JsonDocument doc;
  doc["ok"] = webMainLink && webMainLink->connected();
  JsonArray items = doc["profiles"].to<JsonArray>();
  if (webMainLink) {
    const auto* profiles = webMainLink->profiles();
    for (uint8_t i = 0; i < webMainLink->profileCount(); ++i) {
      JsonObject item = items.add<JsonObject>();
      item["idx"] = profiles[i].index;
      item["name"] = profiles[i].name;
      item["steps"] = profiles[i].stepCount;
      item["tW"] = static_cast<int>(profiles[i].targetWeightG);
      item["tT"] = static_cast<int>(profiles[i].maxShotTimeS);
      item["type"] = profiles[i].type;
    }
  }
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleLegacySelectProfile() {
  if (!webMainLink || !server.hasArg("idx")) {
    server.send(400, "text/plain", "Missing idx or Main offline");
    return;
  }
  const int index = server.arg("idx").toInt();
  const bool sent = index >= 0 && index < webMainLink->profileCount() &&
                    webMainLink->sendCommand(
                        HmiProtocol::Command::SelectProfile,
                        static_cast<uint32_t>(index));
  server.send(sent ? 200 : 409, "text/plain", sent ? "OK" : "REJECTED");
}

void handleLegacyCommand(HmiProtocol::Command command, uint32_t argument = 0) {
  const bool sent = webMainLink && webMainLink->sendCommand(command, argument);
  server.send(sent ? 200 : 409, "text/plain", sent ? "OK" : "REJECTED");
}

void sendConnectionError(int status, const char* error);

void handleLegacyWifiScan() {
  JsonDocument doc;
  JsonArray items = doc["networks"].to<JsonArray>();
  if(!server.hasArg("poll") && !HmiOta::scanRunning()) {
    if (webScaleManager && webScaleManager->isScanning()) {
      sendConnectionError(409, "Wait for scale scan to finish");
      return;
    }
    HmiOta::scanNetworks();
  }
  doc["scanning"]=HmiOta::scanRunning();
  const auto* networks=HmiOta::networks();
  for(uint8_t i=0;i<HmiOta::networkCount();++i){
    JsonObject item=items.add<JsonObject>();
    item["ssid"]=networks[i].ssid;item["rssi"]=networks[i].rssi;
    item["enc"]=networks[i].secure?1:0;
  }
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleLegacyWifiSave() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  const char* networkSsid = doc["ssid"] | "";
  const char* password = doc["pass"] | "";
  const bool accepted = HmiOta::connectWifi(networkSsid, password);
  server.send(accepted ? 200 : 400, "application/json",
              accepted ? "{\"ok\":true}" : "{\"ok\":false}");
}

void sendConnectionError(int status, const char* error) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = error;
  String output;
  serializeJson(doc, output);
  server.send(status, "application/json", output);
}

void handleConnectionsGet() {
  if (!webScaleManager) {
    sendConnectionError(503, "Scale manager unavailable");
    return;
  }
  JsonDocument doc;
  doc["ok"] = true;
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["connected"] = HmiOta::wifiConnected();
  wifi["connecting"] = HmiOta::wifiConnecting();
  wifi["status"] = HmiOta::wifiStatusText();
  wifi["autoConnect"] = HmiOta::autoConnectEnabled();
  wifi["ssid"] = HmiOta::ssid();
  wifi["ip"] = HmiOta::localIp();
  wifi["rssi"] = HmiOta::rssi();
  wifi["apName"] = HmiOta::apName();
  wifi["apIp"] = HmiOta::apIp();

  JsonObject main = doc["main"].to<JsonObject>();
  main["online"] = webMainLink && webMainLink->connected();
  main["compatible"] = webMainLink && webMainLink->compatible();

  JsonObject scale = doc["scale"].to<JsonObject>();
  scale["connected"] = webScaleManager->connected();
  scale["scanning"] = webScaleManager->isScanning();
  scale["name"] = webScaleManager->activeScaleName();
  scale["mac"] = webScaleManager->activeScaleMac();
  scale["savedMac"] = webScaleManager->savedMac();
  scale["rssi"] = webScaleManager->activeScaleRssi();
  scale["weight"] = webScaleManager->weightG();
  scale["stable"] = webScaleManager->stable();
  const uint8_t battery = webScaleManager->batteryPercent();
  if (battery <= 100) scale["battery"] = battery;
  else scale["battery"] = nullptr;
  JsonArray devices = scale["devices"].to<JsonArray>();
  const auto* discovered = webScaleManager->discoveredScales();
  for (uint8_t i = 0; i < webScaleManager->discoveredCount(); ++i) {
    JsonObject item = devices.add<JsonObject>();
    item["name"] = discovered[i].name;
    item["mac"] = discovered[i].mac;
    item["rssi"] = discovered[i].rssi;
  }
  String output;
  serializeJson(doc, output);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", output);
}

void handleConnectionsAction() {
  if (!webScaleManager) {
    sendConnectionError(503, "Scale manager unavailable");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendConnectionError(400, "Invalid JSON");
    return;
  }
  const char* op = doc["op"] | "";
  if (!strcmp(op, "wifi_auto")) {
    if (!doc["enabled"].is<bool>()) {
      sendConnectionError(400, "Missing auto-connect state");
      return;
    }
    HmiOta::setAutoConnect(doc["enabled"].as<bool>());
  } else if (!strcmp(op, "wifi_forget")) {
    HmiOta::forgetWifi();
  } else if (!strcmp(op, "scale_scan")) {
    if (HmiOta::scanRunning()) {
      sendConnectionError(409, "Wait for Wi-Fi scan to finish");
      return;
    }
    webScaleManager->startScan();
  } else if (!strcmp(op, "scale_pair")) {
    const char* mac = doc["mac"] | "";
    if (strlen(mac) != 17) {
      sendConnectionError(400, "Invalid scale address");
      return;
    }
    webScaleManager->pairWithMac(mac);
  } else if (!strcmp(op, "scale_forget")) {
    webScaleManager->unpair();
  } else if (!strcmp(op, "scale_tare")) {
    HmiProtocol::TelemetryPayload telemetry{};
    const bool ready = webMainLink && webMainLink->copyTelemetry(telemetry) &&
        telemetry.machineState ==
            static_cast<uint8_t>(HmiProtocol::MachineState::Ready);
    if (!ready || !webScaleManager->connected()) {
      sendConnectionError(409, "Scale must be connected and machine idle");
      return;
    }
    if (!webScaleManager->tare()) {
      sendConnectionError(409, "Scale rejected tare");
      return;
    }
  } else {
    sendConnectionError(400, "Unknown connection action");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleLegacyUnavailable() {
  server.send(501, "application/json",
              "{\"ok\":false,\"error\":\"API migration pending on HMI\"}");
}

void sendProfileBridgeError(int status, const String& error) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = error;
  String output;
  serializeJson(doc, output);
  server.send(status, "application/json", output);
}

void submitProfiles(uint8_t operation, int detailIndex = -1) {
  if (!webMainLink) { sendProfileBridgeError(503,"Main link unavailable"); return; }
  const String request=server.arg("plain");
  WebJobs::submit([operation,request,detailIndex](String& response,int& status){
    String error; bool ok=false;
    switch(operation){
      case 0:ok=ProfileWebBridge::readSummaries(*webMainLink,response,error);break;
      case 1:ok=ProfileWebBridge::replaceAll(*webMainLink,request,response,error);break;
      case 2:ok=ProfileWebBridge::saveOne(*webMainLink,request,response,error);break;
      case 3:ok=ProfileWebBridge::deleteOne(*webMainLink,request,response,error);break;
      case 4:ok=ProfileWebBridge::readOne(*webMainLink,
                 static_cast<uint8_t>(detailIndex),response,error);break;
    }
    status=ok?200:409;
    if(!ok){JsonDocument doc;doc["ok"]=false;doc["error"]=error;serializeJson(doc,response);}
  });
}
void handleLegacyProfilesGet(){submitProfiles(0);}
void handleLegacyProfilesPost(){submitProfiles(1);}
void handleProfileSave(){submitProfiles(2);}
void handleProfileDelete(){submitProfiles(3);}
void handleProfileDetail(){
  if(!server.hasArg("index")){
    sendProfileBridgeError(400,"Profile index is required");return;
  }
  const String value=server.arg("index");
  char* end=nullptr;
  const long index=strtol(value.c_str(),&end,10);
  if(!end || *end!='\0' || index<0 || index>=12){
    sendProfileBridgeError(400,"Invalid profile index");return;
  }
  submitProfiles(4,static_cast<int>(index));
}

void calibrationError(String& jobBody,int& jobStatus,int status, const String& error) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = error;
  String output;
  serializeJson(doc, output);
  jobStatus=status;jobBody=output;
}

bool readCalibration(HmiProtocol::CalibrationOperation operation,
                     HmiProtocol::CalibrationReplyPayload& reply, String& jobBody,int& jobStatus) {
  if (!webMainLink) {
    calibrationError(jobBody,jobStatus,503, "Main link unavailable");
    return false;
  }
  String error;
  if (!CalibrationWebBridge::execute(*webMainLink, operation, reply, error)) {
    calibrationError(jobBody,jobStatus,409, error);
    return false;
  }
  return true;
}

void buildhandleLegacyPressureGet(const String& requestBody,String& jobBody,int& jobStatus) {
  HmiProtocol::CalibrationReplyPayload reply{};
  if (!readCalibration(HmiProtocol::CalibrationOperation::ReadAll, reply, jobBody,jobStatus))
    return;
  JsonDocument doc;
  doc["ok"] = true;
  doc["version"] = "hmi-uart-v1";
  doc["offset"] = reply.pressureOffset;
  doc["scale"] = reply.pressureScale;
  String output;
  serializeJson(doc, output);
  jobStatus=200;jobBody=output;
}

void buildhandleLegacyPressurePost(const String& requestBody,String& jobBody,int& jobStatus) {
  JsonDocument request;
  if (deserializeJson(request, requestBody)) {
    calibrationError(jobBody,jobStatus,400, "Invalid pressure calibration JSON");
    return;
  }
  const char* command = request["cmd"] | "";
  HmiProtocol::CalibrationOperation operation;
  float value = 0.0f;
  if (strcmp(command, "zero") == 0) {
    operation = HmiProtocol::CalibrationOperation::ZeroPressure;
  } else if (strcmp(command, "p_at") == 0) {
    operation = HmiProtocol::CalibrationOperation::SetPressureAt;
    value = request["bar"] | 0.0f;
  } else {
    calibrationError(jobBody,jobStatus,400, "Unknown pressure calibration command");
    return;
  }
  if (!webMainLink) {
    calibrationError(jobBody,jobStatus,503, "Main link unavailable");
    return;
  }
  HmiProtocol::CalibrationReplyPayload reply{};
  String error;
  if (!CalibrationWebBridge::execute(*webMainLink, operation, reply, error,
                                     value)) {
    calibrationError(jobBody,jobStatus,409, error);
    return;
  }
  JsonDocument response;
  response["ok"] = true;
  response["offset"] = reply.pressureOffset;
  response["scale"] = reply.pressureScale;
  String output;
  serializeJson(response, output);
  jobStatus=200;jobBody=output;
}

void buildhandleLegacyFlowGet(const String& requestBody,String& jobBody,int& jobStatus) {
  HmiProtocol::CalibrationReplyPayload reply{};
  if (!readCalibration(HmiProtocol::CalibrationOperation::ReadAll, reply, jobBody,jobStatus))
    return;
  JsonDocument doc;
  doc["ok"] = true;
  doc["version"] = "brewvector-q1-q9";
  doc["q1_ml_s"] = reply.flowQ1MlPerS;
  doc["q9_ml_s"] = reply.flowQ9MlPerS;
  doc["raw_cps"] = reply.pumpCps;
  doc["actual_ml_s"] = reply.actualPumpFlowMlPerS;
  char diagnostic[112];
  snprintf(diagnostic, sizeof(diagnostic),
           "Pump model at %.1f bar: %.2f ml/s (%.1f CPS)",
           reply.pressureBar, reply.actualPumpFlowMlPerS, reply.pumpCps);
  doc["diag"] = diagnostic;
  String output;
  serializeJson(doc, output);
  jobStatus=200;jobBody=output;
}

void buildhandleLegacyFlowPost(const String& requestBody,String& jobBody,int& jobStatus) {
  JsonDocument request;
  if (deserializeJson(request, requestBody)) {
    calibrationError(jobBody,jobStatus,400, "Invalid flow calibration JSON");
    return;
  }
  const float q1 = request["q1_ml_s"] | 0.0f;
  const float q9 = request["q9_ml_s"] | 0.0f;
  if (!webMainLink) {
    calibrationError(jobBody,jobStatus,503, "Main link unavailable");
    return;
  }
  HmiProtocol::CalibrationReplyPayload reply{};
  String error;
  if (!CalibrationWebBridge::execute(
          *webMainLink, HmiProtocol::CalibrationOperation::SetFlow, reply,
          error, q1, q9)) {
    calibrationError(jobBody,jobStatus,409, error);
    return;
  }
  jobStatus=200;jobBody="{\"ok\":true}";
}

void buildhandleLegacyStatus(const String& requestBody,String& jobBody,int& jobStatus) {
  HmiProtocol::CalibrationReplyPayload reply{};
  if (!readCalibration(HmiProtocol::CalibrationOperation::ReadAll, reply, jobBody,jobStatus))
    return;
  HmiProtocol::TelemetryPayload telemetry{};
  server.callOwner([&](){if (webMainLink) webMainLink->copyTelemetry(telemetry);});
  JsonDocument doc;
  doc["version"] = "hmi-uart-v1";
  doc["adcVolt"] = reply.pressureAdcVolts;
  doc["bar"] = reply.pressureBar;
  doc["offset"] = reply.pressureOffset;
  doc["scale"] = reply.pressureScale;
  doc["tempC"] = telemetry.brewTempC;
  doc["blockTempC"] = telemetry.blockTempC;
  doc["pathTempEstC"] = telemetry.brewTempC;
  doc["brewTempEstC"] = telemetry.brewTempC;
  doc["thermalLoadG"] = 0.0f;
  doc["ZCHz"] = reply.zeroCrossHz;
  doc["CPS"] = reply.pumpCps;
  doc["audit_cps"] = reply.pumpCps;
  doc["audit_fpc"] = reply.pumpFlowPerClickMl;
  doc["audit_base"] = reply.freeFlowPerClickMl;
  char diagnostic[80];
  snprintf(diagnostic, sizeof(diagnostic), "%.1f CPS * %.3f FPC = %.2f ml/s",
           reply.pumpCps, reply.pumpFlowPerClickMl,
           reply.actualPumpFlowMlPerS);
  doc["audit_diag"] = diagnostic;
  String output;
  serializeJson(doc, output);
  jobStatus=200;jobBody=output;
}
void handleThermalGet() {
  WebJobs::submit([](String& body, int& status) {
    HmiProtocol::CalibrationReplyPayload reply{};
    if (!readCalibration(HmiProtocol::CalibrationOperation::ReadAll, reply, body, status)) return;
    JsonDocument doc;
    doc["ok"] = true;
    doc["offset_c"] = reply.thermalOffsetC;
    serializeJson(doc, body);
    status = 200;
  });
}

void handleThermalPost() {
  const String request = server.arg("plain");
  WebJobs::submit([request](String& body, int& status) {
    JsonDocument doc;
    if (deserializeJson(doc, request) || !doc["offset_c"].is<float>() ||
        !doc["expected_offset_c"].is<float>() ||
        !ThermalCalibration::valid(doc["offset_c"].as<float>()) ||
        !ThermalCalibration::valid(doc["expected_offset_c"].as<float>())) {
      calibrationError(body, status, 400, "Offset must be a number from 0 to 60 C; reload first");
      return;
    }
    if (!webMainLink) {
      calibrationError(body, status, 503, "Main link unavailable");
      return;
    }
    HmiProtocol::CalibrationReplyPayload reply{};
    String error;
    if (!CalibrationWebBridge::execute(*webMainLink, HmiProtocol::CalibrationOperation::SetThermalOffset,
        reply, error, doc["offset_c"].as<float>(), doc["expected_offset_c"].as<float>())) {
      calibrationError(body, status, 409, error);
      return;
    }
    doc.clear();
    doc["ok"] = true;
    doc["offset_c"] = reply.thermalOffsetC;
    serializeJson(doc, body);
    status = 200;
  });
}

void handleLegacyPressureGet(){
  const String request=server.arg("plain");
  WebJobs::submit([request](String& body,int& status){buildhandleLegacyPressureGet(request,body,status);});
}

void handleLegacyPressurePost(){
  const String request=server.arg("plain");
  WebJobs::submit([request](String& body,int& status){buildhandleLegacyPressurePost(request,body,status);});
}

void handleLegacyFlowGet(){
  const String request=server.arg("plain");
  WebJobs::submit([request](String& body,int& status){buildhandleLegacyFlowGet(request,body,status);});
}

void handleLegacyFlowPost(){
  const String request=server.arg("plain");
  WebJobs::submit([request](String& body,int& status){buildhandleLegacyFlowPost(request,body,status);});
}

void handleLegacyStatus(){
  const String request=server.arg("plain");
  WebJobs::submit([request](String& body,int& status){buildhandleLegacyStatus(request,body,status);});
}


void handleHmiSettingsGet() {
  if (!webSettings) {
    server.send(503, "application/json", "{\"ok\":false}");
    return;
  }
  const HmiSettingsData& data = webSettings->data();
  JsonDocument doc;
  doc["ok"] = true;
  doc["brightness"] = data.brightness;
  doc["volume"] = data.volume;
  doc["autoDimMinutes"] = data.autoDimMinutes;
  doc["touchSounds"] = data.touchSounds;
  doc["autoSaveShots"] = data.autoSaveShots;
  doc["storageReady"] = webShotStore && webShotStore->ready();
  doc["storageTotalMB"] = webShotStore
                               ? webControlTraceStore->totalBytes() / 1048576.0
                               : 0.0;
  doc["storageUsedMB"] = webShotStore
                              ? webControlTraceStore->usedBytes() / 1048576.0
                              : 0.0;
  String output;
  serializeJson(doc, output);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", output);
}

void handleHmiSettingsPost() {
  if (!webSettings) {
    server.send(503, "application/json", "{\"ok\":false}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  if (doc["brightness"].is<int>()) {
    webSettings->setBrightness(
        constrain(doc["brightness"].as<int>(), 10, 100));
    if (webDisplay)
      webDisplay->setBacklight(webSettings->data().brightness);
  }
  if (doc["volume"].is<int>()) {
    webSettings->setVolume(constrain(doc["volume"].as<int>(), 0, 100));
    if (webAudio) webAudio->setVolume(webSettings->data().volume);
  }
  if (doc["autoDimMinutes"].is<int>()) {
    const int minutes = doc["autoDimMinutes"].as<int>();
    if (minutes == 0 || minutes == 1 || minutes == 2 || minutes == 5 ||
        minutes == 10)
      webSettings->setAutoDimMinutes(minutes);
  }
  if (doc["touchSounds"].is<bool>())
    webSettings->setTouchSounds(doc["touchSounds"].as<bool>());
  if (doc["autoSaveShots"].is<bool>())
    webSettings->setAutoSaveShots(doc["autoSaveShots"].as<bool>());
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleHmiTestSound() {
  if (!webAudio || !webAudio->ready()) {
    server.send(503, "application/json", "{\"ok\":false}");
    return;
  }
  webAudio->testTone();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleScaleSimulator() {
  if (!webScaleManager) {
    server.send(503, "application/json", "{\"ok\":false,\"error\":\"Scale manager unavailable\"}");
    return;
  }
  JsonDocument request;
  if (deserializeJson(request, server.arg("plain"))) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }
  const bool enabled = request["enabled"] | false;
  if (enabled && webMainLink) {
    HmiProtocol::TelemetryPayload telemetry{};
    if (webMainLink->copyTelemetry(telemetry)) {
      const auto state =
          static_cast<HmiProtocol::MachineState>(telemetry.machineState);
      if (state == HmiProtocol::MachineState::Brew ||
          state == HmiProtocol::MachineState::Flush ||
          state == HmiProtocol::MachineState::HotWater) {
        server.send(409, "application/json",
                    "{\"ok\":false,\"error\":\"Simulator locked while machine is running\"}");
        return;
      }
    }
  }
  const float grams = request["grams"] | 0.0f;
  const bool stable = request["stable"] | true;
  if (!webScaleManager->setSimulator(enabled, grams, stable)) {
    server.send(422, "application/json",
                "{\"ok\":false,\"error\":\"Invalid simulator weight\"}");
    return;
  }
  server.send(200, "application/json", enabled
      ? "{\"ok\":true,\"source\":\"SIMULATOR\"}"
      : "{\"ok\":true,\"source\":\"OFFLINE\"}");
}


void handleWifiSave() {
  const String ssid = server.arg("ssid");
  const String password = server.arg("pass");
  if (ssid.isEmpty()) {
    server.send(400, "text/plain", "SSID required");
    return;
  }
  HmiOta::connectWifi(ssid.c_str(), password.c_str());
  server.send(200, "text/html; charset=utf-8",
              "<html><body><h2>Testing connection...</h2>"
              "<p>Reload <a href='/'>status</a> after 10 seconds.</p></body></html>");
}

}  // namespace

namespace HmiWebInternal {

void registerDashboardRoutes() {
  server.on("/api/hmi/diagnostics",HTTP_GET,[](){
    JsonDocument doc;
    doc["build"]=__DATE__ " " __TIME__;
    doc["uptimeMs"]=millis();doc["freeHeap"]=ESP.getFreeHeap();
    doc["minFreeHeap"]=ESP.getMinFreeHeap();doc["freePsram"]=ESP.getFreePsram();
    doc["wifiConnected"]=WiFi.status()==WL_CONNECTED;doc["wifiRssi"]=WiFi.RSSI();
    const auto& m=hmiRuntimeMetrics;
    doc["loops"]=m.loops;doc["maxLoopUs"]=m.maxLoopUs;
    doc["maxLoopGapUs"]=m.maxGapUs;doc["loopsOver100ms"]=m.over100ms;
    const char* names[]={"scale","uart","traceAndUpdate","web","touchAndUi","render"};
    for(uint8_t i=0;i<6;++i)doc["maxStageUs"][names[i]]=m.maxStageUs[i];
    doc["maxWebHandlerUs"]=server.maxHandlerUs();doc["httpWriteErrors"]=server.responseErrors();
    if(webScaleManager){doc["bleQueueDrops"]=webScaleManager->bleClient().eventDrops();doc["maxBleWorkerUs"]=webScaleManager->bleClient().maxWorkerUs();}
    if(webMainLink){doc["mainOnline"]=webMainLink->connected();doc["uartBufferDrops"]=webMainLink->rxOverflowCount();}
    if(webControlTraceStore){
      doc["tfQueueDrops"]=webControlTraceStore->queueDrops();
      doc["maxTfIoUs"]=webControlTraceStore->maxIoUs();doc["tfStatus"]=webControlTraceStore->error();
    }
    String body;serializeJson(doc,body);server.sendHeader("Cache-Control","no-store");
    server.send(200,"application/json",body);
  });
  server.on("/api/hmi/restart",HTTP_POST,[](){
    server.sendHeader("Cache-Control","no-store");
    server.send(200,"application/json","{\"ok\":true}");
    server.restartAfterResponse();
  });
  registerPumpDiagnosticRoutes();
  server.onWorker("/", HTTP_GET, handleRoot);
  server.on("/dashboard-v2", HTTP_GET, handleNewDashboard);
  server.on("/network", HTTP_GET, handleNetworkPage);
  server.on("/api/live", HTTP_GET, handleLiveApi);
  server.on("/dash/live", HTTP_GET, handleLegacyDashLive);
  server.on("/dash/profiles", HTTP_GET, handleLegacyDashProfiles);
  server.on("/dash/set_profile", HTTP_GET, handleLegacySelectProfile);
  server.on("/dash/start", HTTP_GET, []() {
    handleLegacyCommand(HmiProtocol::Command::StartBrew);
  });
  server.on("/dash/stop", HTTP_GET, []() {
    handleLegacyCommand(HmiProtocol::Command::Stop);
  });
  server.on("/dash/flush", HTTP_GET, []() {
    handleLegacyCommand(HmiProtocol::Command::StartFlush, 5000);
  });
  server.on("/dash/set_yield", HTTP_GET, handleLegacyUnavailable);
  server.on("/dash/set_manual", HTTP_GET, handleLegacyUnavailable);
  server.onWorker("/status", HTTP_GET, handleLegacyStatus);
  server.onWorker("/cal/pressure", HTTP_GET, handleLegacyPressureGet);
  server.onWorker("/cal/pressure", HTTP_POST, handleLegacyPressurePost);
  server.onWorker("/cal/thermal", HTTP_GET, handleThermalGet);
  server.onWorker("/cal/thermal", HTTP_POST, handleThermalPost);
  server.onWorker("/cal/flow", HTTP_GET, handleLegacyFlowGet);
  server.onWorker("/cal/flow", HTTP_POST, handleLegacyFlowPost);
  server.onWorker("/profiles", HTTP_GET, handleLegacyProfilesGet);
  server.onWorker("/profiles", HTTP_POST, handleLegacyProfilesPost);
  server.onWorker("/profile/detail", HTTP_GET, handleProfileDetail);
  server.onWorker("/profile/save", HTTP_POST, handleProfileSave);
  server.onWorker("/profile/delete", HTTP_POST, handleProfileDelete);
  server.on("/sys/wifi_scan", HTTP_GET, handleLegacyWifiScan);
  server.on("/sys/wifi_save", HTTP_POST, handleLegacyWifiSave);
  server.on("/api/connections", HTTP_GET, handleConnectionsGet);
  server.on("/api/connections/action", HTTP_POST, handleConnectionsAction);
  server.on("/hmi/settings", HTTP_GET, handleHmiSettingsGet);
  server.on("/hmi/settings", HTTP_POST, handleHmiSettingsPost);
  server.on("/hmi/test-sound", HTTP_POST, handleHmiTestSound);
  server.on("/api/scale/simulator", HTTP_POST, handleScaleSimulator);
  server.on("/wifi", HTTP_POST, handleWifiSave);
  server.onWorker("/favicon.ico", HTTP_GET, [](){server.send(204);});
  server.onNotFound([](){server.send(404,"text/plain","Not found");});
}

}  // namespace HmiWebInternal
