#pragma once
#include <WebServer.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <atomic>
#include <vector>

// Socket/HTTP parsing belongs to the worker. Application callbacks belong to
// loopTask. The worker waits while a callback runs, so request buffers stay valid.
class OwnerWebServer : public WebServer {
 public:
  explicit OwnerWebServer(int port) : WebServer(port) {}
  void begin() override;
  void stop();
  void resume();
  void pollOwner();
  void on(const Uri& uri, HTTPMethod method, THandlerFunction fn);
  void on(const Uri& uri, HTTPMethod method, THandlerFunction fn, THandlerFunction upload);
  void onNotFound(THandlerFunction fn);
  // Only handlers explicitly written for worker execution may use this.
  void onWorker(const Uri& uri, HTTPMethod method, THandlerFunction fn);
  void callOwner(THandlerFunction fn);
  bool inOwner() const { return xTaskGetCurrentTaskHandle() == owner_; }
  void restartAfterResponse() { rebootAfterSend_=true; }
  size_t streamFile(fs::File& file, const String& contentType, const int code=200);
  uint32_t maxHandlerUs() const { return maxHandlerUs_; }
  uint32_t responseErrors() const { return responseErrors_.load(); }
 protected:
  size_t _currentClientWrite(const char* data, size_t size) override;
  size_t _currentClientWrite_P(PGM_P data, size_t size) override;
 private:
  struct Call { THandlerFunction fn; TaskHandle_t waiter; };
  struct Block { uint8_t* data; size_t size; };
  QueueHandle_t calls_ = nullptr;
  TaskHandle_t owner_ = nullptr, worker_ = nullptr;
  std::atomic<bool> enabled_{true};
  std::atomic<bool> reopen_{false};
  std::atomic<uint32_t> rebootAt_{0};
  bool rebootAfterSend_=false;
  std::atomic<uint32_t> responseErrors_{0};
  uint32_t maxHandlerUs_ = 0;
  std::vector<Block> response_;
  size_t responseBytes_ = 0;
  bool captureFailed_ = false;
  void dispatch(THandlerFunction fn);
  void flushResponse();
  void run();
};
