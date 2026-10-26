#include "web/web_jobs.h"
#include "web/hmi_web_internal.h"
#include <freertos/semphr.h>
#include <ArduinoJson.h>

namespace WebJobs {
namespace {
constexpr size_t CAPACITY = 4;
struct Job {
  uint32_t id=0, completedAt=0;
  bool done=false, observed=false;
  Work work;
  String body;
  int status=500;
} jobs[CAPACITY];
SemaphoreHandle_t mutex=nullptr;
QueueHandle_t queue=nullptr;
uint32_t nextId=0;
bool ready=false;

void run(void*) {
  for (;;) {
    uint8_t index=0;
    if(xQueueReceive(queue,&index,portMAX_DELAY)!=pdTRUE)continue;
    // This slot cannot be reused until completed, then observed or expired.
    String body; int status=500;
    jobs[index].work(body,status);
    xSemaphoreTake(mutex,portMAX_DELAY);
    auto& job=jobs[index];
    job.work=nullptr;job.body=std::move(body);job.status=status;
    job.completedAt=millis();job.done=true;
    xSemaphoreGive(mutex);
  }
}
void ticket(uint32_t id) {
  HmiWebInternal::server.sendHeader("Cache-Control","no-store");
  HmiWebInternal::server.send(202,"application/json",String("{\"job\":")+id+"}");
}
void poll() {
  auto& server=HmiWebInternal::server;
  const uint32_t id=static_cast<uint32_t>(server.arg("id").toInt());
  String body;int status=404;bool pending=false;
  if(ready) {
    xSemaphoreTake(mutex,portMAX_DELAY);
    for(auto& job:jobs)if(id && job.id==id){
      pending=!job.done;
      if(job.done){body=job.body;status=job.status;job.observed=true;}
      break;
    }
    xSemaphoreGive(mutex);
  }
  if(pending){ticket(id);return;}
  if(status==404)body="{\"ok\":false,\"error\":\"Job expired; result unknown. Reload state; do not repeat the command automatically.\"}";
  server.sendHeader("Cache-Control","no-store");
  server.send(status,"application/json",body);
}
}
void registerRoutes() {
  mutex=xSemaphoreCreateMutex();queue=xQueueCreate(CAPACITY,sizeof(uint8_t));
  ready=mutex && queue && xTaskCreate(run,"hmi-jobs",24576,nullptr,1,nullptr)==pdPASS;
  if(!ready){
    if(mutex){vSemaphoreDelete(mutex);mutex=nullptr;}
    if(queue){vQueueDelete(queue);queue=nullptr;}
  }
  HmiWebInternal::server.onWorker("/api/hmi/job",HTTP_GET,poll);
}
void submit(Work work) {
  auto& server=HmiWebInternal::server;
  if(!ready){server.send(503,"application/json","{\"error\":\"Job worker unavailable\"}");return;}
  uint32_t id=0;
  xSemaphoreTake(mutex,portMAX_DELAY);
  for(uint8_t i=0;i<CAPACITY;++i){
    auto& job=jobs[i];
    if(job.id && (!job.done || (!job.observed && millis()-job.completedAt<30000)))continue;
    id=++nextId;if(!id)id=++nextId;
    job.id=id;job.done=false;job.observed=false;job.body="";job.work=std::move(work);
    if(xQueueSend(queue,&i,0)!=pdTRUE){job.id=0;job.work=nullptr;id=0;}
    break;
  }
  xSemaphoreGive(mutex);
  if(id)ticket(id);
  else server.send(409,"application/json","{\"error\":\"Main jobs busy; wait for current operations\"}");
}
}
