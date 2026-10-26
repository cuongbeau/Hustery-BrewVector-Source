#include "storage/control_trace_store.h"
#include "storage/shot_store.h"
#include <SD_MMC.h>

bool ControlTraceStore::begin() {
  if(worker_)return true;
  queue_=xQueueCreate(64,sizeof(Event));
  if(!queue_ || xTaskCreate([](void* p){static_cast<ControlTraceStore*>(p)->run();},
      "hmi-tf",8192,this,1,&worker_)!=pdPASS){
    if(queue_){vQueueDelete(queue_);queue_=nullptr;}
    view_.error=published_.error="TF TASK FAILED";return false;
  }
  return true;
}
bool ControlTraceStore::enqueue(const Event& event){
  if(!queue_ || xQueueSend(queue_,&event,0)!=pdTRUE){++drops_;return false;}
  return true;
}
void ControlTraceStore::append(const HmiProtocol::ControlTracePayload& trace,const char* profile,const char* phaseName){
  if(!queue_ || uxQueueSpacesAvailable(queue_)<=2){++drops_;return;}
  Event event{};event.kind=Kind::Append;event.trace=trace;
  snprintf(event.profile,sizeof(event.profile),"%s",profile?profile:"OFF");
  snprintf(event.phase,sizeof(event.phase),"%s",phaseName?phaseName:"");enqueue(event);
}
void ControlTraceStore::finish(){Event e{};e.kind=Kind::Finish;enqueue(e);}
void ControlTraceStore::finish(const HmiProtocol::ShotEndPayload& end){
  Event e{};e.kind=Kind::FinishResult;e.end=end;enqueue(e);
}
bool ControlTraceStore::load(uint32_t id,ShotChart& chart){
  Event e{};e.kind=Kind::Load;e.id=id;e.generation=++loadGeneration_;
  destination_=&chart;return enqueue(e);
}
bool ControlTraceStore::eraseAll(){Event e{};e.kind=Kind::Erase;return enqueue(e);}
bool ControlTraceStore::latestPath(char* path,size_t size)const{
  if(!view_.latest)return false;
  snprintf(path,size,"/brewvector/traces/trace_%06lu.csv",static_cast<unsigned long>(view_.latest));return true;
}
void ControlTraceStore::poll(uint32_t nowMs){
  (void)nowMs;
  portENTER_CRITICAL(&mux_);view_=published_;
  if(loadReady_){
    if(destination_ && completedGeneration_==loadGeneration_)destination_->swap(loadedChart_);
    loadReady_=false;
  }
  portEXIT_CRITICAL(&mux_);
}
void ControlTraceStore::run(){
  backend_.begin();loadedChart_.begin();
  uint32_t spaceAt=0,maxUs=0;uint64_t total=0,used=0;
  const char* actionError=nullptr;
  for(;;){
    Event e{};const bool received=xQueueReceive(queue_,&e,pdMS_TO_TICKS(20))==pdTRUE;
    const uint32_t started=micros();
    if(received){
      switch(e.kind){
        case Kind::Append:backend_.append(e.trace,e.profile,e.phase);actionError=nullptr;break;
        case Kind::Finish:backend_.finish();break;
        case Kind::FinishResult:backend_.finish(e.end);break;
        case Kind::Erase:
          if(backend_.active()){actionError="BUSY LOGGING";break;}
          {const bool traceOk=backend_.eraseAll();const bool legacyOk=!legacy_ || legacy_->eraseAll();
           actionError=traceOk&&legacyOk?"ERASED":"ERASE FAILED";spaceAt=0;}
          break;
        case Kind::Load:
          if(backend_.active()){actionError="BUSY LOGGING";break;}
          for(;;){
            portENTER_CRITICAL(&mux_);const bool pending=loadReady_;portEXIT_CRITICAL(&mux_);
            if(!pending)break;vTaskDelay(1);
          }
          loadedChart_.reset();
          if(!backend_.load(e.id,loadedChart_)){loadedChart_.reset();actionError="TF LOAD FAILED";}
          else actionError="LOADED";
          portENTER_CRITICAL(&mux_);completedGeneration_=e.generation;loadReady_=true;portEXIT_CRITICAL(&mux_);
          break;
      }
    }
    backend_.poll(millis());
    if(!spaceAt || millis()-spaceAt>=10000){
      if(backend_.ready()){total=SD_MMC.totalBytes();used=SD_MMC.usedBytes();}
      spaceAt=millis();
    }
    const uint32_t elapsed=micros()-started;if(elapsed>maxUs)maxUs=elapsed;
    Snapshot state{};state.ready=backend_.ready();state.active=backend_.active();
    state.latest=backend_.latestId();state.samples=backend_.samples();state.gaps=backend_.receivedGaps();
    state.count=backend_.recentCount();state.error=actionError?actionError:backend_.error();
    state.maxUs=maxUs;state.total=total;state.used=used;
    memcpy(state.recent,backend_.recent(),sizeof(state.recent));
    portENTER_CRITICAL(&mux_);published_=state;portEXIT_CRITICAL(&mux_);
  }
}
