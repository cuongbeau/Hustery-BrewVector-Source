#pragma once
#include "storage/control_trace_backend.h"
#include <freertos/queue.h>
#include <atomic>
class ShotStore;
// loopTask queues records and consumes snapshots; the storage task owns TF I/O.
class ControlTraceStore {
 public:
  static constexpr size_t RECENT_CAPACITY=ControlTraceBackend::RECENT_CAPACITY;
  bool begin();
  void attachLegacy(ShotStore& store){legacy_=&store;}
  void append(const HmiProtocol::ControlTracePayload&,const char*,const char* phaseName=nullptr);
  void finish();
  void finish(const HmiProtocol::ShotEndPayload&);
  void poll(uint32_t nowMs);
  bool load(uint32_t id,ShotChart& chart);
  bool eraseAll();
  bool ready()const{return view_.ready;}
  bool active()const{return view_.active;}
  uint32_t latestId()const{return view_.latest;}
  uint32_t samples()const{return view_.samples;}
  uint32_t receivedGaps()const{return view_.gaps;}
  uint32_t queueDrops()const{return drops_.load();}
  uint32_t maxIoUs()const{return view_.maxUs;}
  const ShotSummary* recent()const{return view_.recent;}
  size_t recentCount()const{return view_.count;}
  const char* error()const{return drops_?"TF QUEUE OVERFLOW":view_.error;}
  uint64_t totalBytes()const{return view_.total;}
  uint64_t usedBytes()const{return view_.used;}
  bool latestPath(char*,size_t)const;
 private:
  enum class Kind:uint8_t{Append,Finish,FinishResult,Load,Erase};
  struct Event {
    Kind kind; HmiProtocol::ControlTracePayload trace{};
    HmiProtocol::ShotEndPayload end{};char profile[36]={};char phase[16]={};uint32_t id=0,generation=0;
  };
  struct Snapshot {
    bool ready=false,active=false;uint32_t latest=0,samples=0,gaps=0,maxUs=0;
    uint64_t total=0,used=0;size_t count=0;const char* error="STARTING TF";
    ShotSummary recent[RECENT_CAPACITY]={};
  } published_,view_;
  ControlTraceBackend backend_;
  ShotStore* legacy_=nullptr;
  QueueHandle_t queue_=nullptr;
  TaskHandle_t worker_=nullptr;
  mutable portMUX_TYPE mux_=portMUX_INITIALIZER_UNLOCKED;
  std::atomic<uint32_t> drops_{0};
  ShotChart loadedChart_;
  ShotChart* destination_=nullptr;
  uint32_t loadGeneration_=0,completedGeneration_=0;
  bool loadReady_=false;
  bool enqueue(const Event&);
  void run();
};
