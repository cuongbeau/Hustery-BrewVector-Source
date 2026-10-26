#include "web/hmi_web_internal.h"

#include <FS.h>
#include <SD_MMC.h>

#include "communication/main_link.h"
#include "web/owner_call.h"
#include <ArduinoJson.h>
#include "model/shot_save_policy.h"
#include "storage/control_trace_store.h"

using namespace HmiWebInternal;

namespace {
constexpr char TRACE_DIR[] = "/brewvector/traces";

void handleHistoryRedirect() {
  server.sendHeader("Location", "/#history", true);
  server.send(302, "text/plain", "History is integrated into the main page");
}

// Registered on the owner loop: copy the published RAM summaries, no TF I/O.
void handleTraceList() {
  if (!webControlTraceStore || !webControlTraceStore->ready()) {
    server.send(503,"application/json","{\"error\":\"TF card unavailable\"}");
    return;
  }
  JsonDocument doc;
  doc["supported"] = webMainLink && webMainLink->supportsControlTrace();
  doc["limit"] = ShotSavePolicy::RecentLimit;
  JsonArray rows = doc["traces"].to<JsonArray>();
  const size_t count = min<size_t>(ShotSavePolicy::RecentLimit,webControlTraceStore->recentCount());
  for (size_t i=0;i<count;++i) {
    const auto& summary=webControlTraceStore->recent()[i];
    JsonObject row=rows.add<JsonObject>();
    row["id"]=summary.id;row["profile"]=summary.profileName;
    row["duration"]=summary.durationS;row["peak"]=summary.peakPressureBar;
    row["samples"]=summary.sampleCount;row["resultFlags"]=summary.resultFlags;
    if(hasStopYield(summary))row["yield"]=summary.weightG;
    else row["yield"]=nullptr;
  }
  String body;serializeJson(doc,body);
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json",body);
}

void handleTraceDownload() {
  const uint32_t id = static_cast<uint32_t>(server.arg("id").toInt());
  if (!webControlTraceStore || !ownerValue([&](){return webControlTraceStore->ready();}) || id == 0) {
    server.send(400, "text/plain", "Invalid trace id or TF card unavailable");
    return;
  }
  char path[64], filename[40];
  snprintf(path, sizeof(path), "%s/trace_%06lu.csv", TRACE_DIR,
           static_cast<unsigned long>(id));
  snprintf(filename, sizeof(filename), "brewvector_trace_%06lu.csv",
           static_cast<unsigned long>(id));
  File file = SD_MMC.open(path, FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "Control trace not found");
    return;
  }
  if(server.hasArg("offset")) {
    const String offsetText=server.arg("offset");
    char* end=nullptr;const unsigned long offset=strtoul(offsetText.c_str(),&end,10);
    const size_t total=file.size();
    if(offsetText.isEmpty() || *end || offset>total){server.send(416,"text/plain","Invalid offset");return;}
    if(!file.seek(offset)){server.send(500,"text/plain","Seek failed");return;}
    const size_t count=min<size_t>(16384,total-offset);
    String data;data.reserve(count);
    char block[512];size_t remaining=count;
    while(remaining){const size_t n=file.readBytes(block,min(remaining,sizeof(block)));if(!n)break;data.concat(block,n);remaining-=n;}
    if(remaining){server.send(500,"text/plain","Trace read failed");return;}
    server.sendHeader("Cache-Control","no-store");
    server.sendHeader("X-Trace-Total",String(total));
    server.send(200,"text/csv",data);return;
  }
  server.sendHeader("Content-Disposition",
                    String("attachment; filename=\"") + filename + "\"");
  server.streamFile(file, "text/csv");
  file.close();
}

void handleLatestTraceDownload() {
  if (!webControlTraceStore || !ownerValue([&](){return webControlTraceStore->ready();})) {
    server.send(404, "text/plain", "Control trace storage unavailable");
    return;
  }
  char path[72];
  if (!ownerValue([&](){return webControlTraceStore->latestPath(path, sizeof(path));})) {
    server.send(404, "text/plain", "No control trace recorded yet");
    return;
  }
  File file = SD_MMC.open(path, FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "Control trace not found");
    return;
  }
  server.sendHeader("Content-Disposition",
                    "attachment; filename=\"brewvector_control_trace.csv\"");
  server.streamFile(file, "text/csv");
  file.close();
}

}  // namespace

namespace HmiWebInternal {
void registerHistoryRoutes() {
  server.on("/history", HTTP_GET, handleHistoryRedirect);
  server.on("/traces/list", HTTP_GET, handleTraceList);
  server.onWorker("/traces/download", HTTP_GET, handleTraceDownload);
  server.onWorker("/traces/latest.csv", HTTP_GET, handleLatestTraceDownload);
}
}  // namespace HmiWebInternal
