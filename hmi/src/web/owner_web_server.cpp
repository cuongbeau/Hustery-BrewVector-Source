#include "web/owner_web_server.h"
#include <esp_heap_caps.h>
#include <lwip/sockets.h>
#include <errno.h>

void OwnerWebServer::begin() {
  if (worker_) { resume(); return; }
  owner_ = xTaskGetCurrentTaskHandle();
  calls_ = xQueueCreate(1, sizeof(Call*));
  if (!calls_) { Serial.println("[Web] owner queue allocation failed"); return; }
  WebServer::begin();
  if (xTaskCreate([](void* self){static_cast<OwnerWebServer*>(self)->run();},
                  "hmi-http", 16384, this, 1, &worker_) != pdPASS) {
    WebServer::stop(); Serial.println("[Web] task allocation failed");
    vQueueDelete(calls_);calls_=nullptr;
  }
}
void OwnerWebServer::stop() { enabled_ = false; }
void OwnerWebServer::resume() { reopen_=true; enabled_ = true; }
void OwnerWebServer::callOwner(THandlerFunction fn) {
  if (inOwner()) { fn(); return; }
  Call call{fn, xTaskGetCurrentTaskHandle()};
  Call* ptr = &call;
  // Never expire this stack-owned message while loopTask may still execute it.
  xQueueSend(calls_, &ptr, portMAX_DELAY);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}
void OwnerWebServer::pollOwner() {
  const uint32_t reboot=rebootAt_.load();
  if(reboot && static_cast<int32_t>(millis()-reboot)>=0)ESP.restart();
  if (!calls_) return;
  Call* call = nullptr;
  if (xQueueReceive(calls_, &call, 0) != pdTRUE) return;
  const uint32_t started = micros();
  call->fn();
  const uint32_t elapsed = micros() - started;
  if (elapsed > maxHandlerUs_) maxHandlerUs_ = elapsed;
  xTaskNotifyGive(call->waiter);
}
void OwnerWebServer::dispatch(THandlerFunction fn) {
  callOwner(fn);
  flushResponse();
}
void OwnerWebServer::on(const Uri& uri, HTTPMethod method, THandlerFunction fn) {
  WebServer::on(uri, method, [this,fn](){dispatch(fn);});
}
void OwnerWebServer::on(const Uri& uri, HTTPMethod method, THandlerFunction fn,
                        THandlerFunction upload) {
  WebServer::on(uri, method, [this,fn](){dispatch(fn);},
                [this,upload](){dispatch(upload);});
}
void OwnerWebServer::onNotFound(THandlerFunction fn) {
  WebServer::onNotFound([this,fn](){dispatch(fn);});
}
void OwnerWebServer::onWorker(const Uri& uri, HTTPMethod method, THandlerFunction fn) {
  WebServer::on(uri, method, fn);
}
size_t OwnerWebServer::_currentClientWrite(const char* data, size_t size) {
  if (!inOwner()) {
    _currentClient.setNoDelay(true);
    // Total response progress is bounded; never wait in the UI task.
    size_t sent = 0;
    uint32_t progress = millis();
    while (sent < size && enabled_) {
      const int n = ::send(_currentClient.fd(), data + sent,
                           min<size_t>(size-sent, 1460), MSG_DONTWAIT);
      if (n > 0) { sent += n; progress = millis(); }
      else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) break;
      else if (millis() - progress >= 3000) break;
      else vTaskDelay(1);
    }
    if (sent != size) { ++responseErrors_; _currentClient.stop(); }
    return sent;
  }
  if (!size) return 0;
  // Normal API responses are small; root is compressed. Avoid unbounded RAM.
  if (captureFailed_ || size > 262144 - responseBytes_) { captureFailed_=true; return 0; }
  auto* copy = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!copy) copy = static_cast<uint8_t*>(malloc(size));
  if (!copy) { captureFailed_=true; return 0; }
  memcpy(copy, data, size);
  response_.push_back({copy,size}); responseBytes_ += size;
  return size;
}
size_t OwnerWebServer::_currentClientWrite_P(PGM_P data, size_t size) {
  return _currentClientWrite(data,size);
}
size_t OwnerWebServer::streamFile(fs::File& file, const String& type, const int code) {
  // File::close closes the underlying handle even when a File copy exists.
  // All downloads must therefore execute entirely on the HTTP worker.
  if(inOwner()){send(500,"text/plain","File streaming requires HTTP worker");return 0;}
  const size_t size=file.size();
  _streamFileCore(size,file.name(),type,code);
  {
    char block[1024];
    while(file.available()) {
      const size_t n=file.readBytes(block,sizeof(block));
      if (!n || _currentClientWrite(block,n)!=n) break;
    }
  }
  return size;
}
void OwnerWebServer::flushResponse() {
  if(captureFailed_) { ++responseErrors_; _currentClient.stop(); }
  for(auto& block:response_) {
    if(!captureFailed_) _currentClientWrite(reinterpret_cast<char*>(block.data),block.size);
    free(block.data);
  }
  response_.clear(); responseBytes_=0; captureFailed_=false;
  if(rebootAfterSend_){rebootAfterSend_=false;rebootAt_=millis()+300;}
}
void OwnerWebServer::run() {
  bool listening=true;
  using PendingClient = decltype(_server.available());
  PendingClient waiting[4];uint32_t acceptedAt[4]={};
  for(;;) {
    if(reopen_.exchange(false)){WebServer::stop();listening=false;for(auto& c:waiting)c.stop();}
    if(!enabled_) {
      if(listening){WebServer::stop();listening=false;for(auto& c:waiting)c.stop();}
    } else {
      if(!listening){WebServer::begin();listening=true;}
      // Browsers preconnect idle sockets. Core WebServer waits up to 5 s on
      // the first one, holding ready requests behind it. Select readable ones.
      for(size_t i=0;i<4;++i){
        if(waiting[i] && (!waiting[i].connected() || millis()-acceptedAt[i]>1500))waiting[i].stop();
        if(!waiting[i]){waiting[i]=_server.available();if(waiting[i])acceptedAt[i]=millis();}
        if(_currentStatus==HC_NONE && waiting[i] && waiting[i].available()){
          _currentClient=waiting[i];waiting[i]=PendingClient();
          _currentStatus=HC_WAIT_READ;_statusChange=millis();
        }
      }
      if(_currentStatus!=HC_NONE)WebServer::handleClient();
    }
    vTaskDelay(1);
  }
}
