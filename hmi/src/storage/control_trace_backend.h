#pragma once

#include <Arduino.h>
#include <FS.h>

#include "hmi_protocol.h"
#include "model/shot_record.h"
#include "ui/shot_chart.h"

// HMI-only TF writer for controller research traces. Main sends fixed binary
// snapshots; this module does the expensive text/TF work away from control.
class ControlTraceBackend {
 public:
  static constexpr size_t RECENT_CAPACITY = 40;
  bool begin();
  void append(const HmiProtocol::ControlTracePayload& trace,
              const char* profileName, const char* phaseName=nullptr);
  void poll(uint32_t nowMs);
  void finish();
  void finish(const HmiProtocol::ShotEndPayload& shotEnd);
  bool load(uint32_t id, ShotChart& chart);
  bool eraseAll();

  bool ready() const { return ready_; }
  bool active() const { return active_; }
  uint32_t latestId() const { return latestId_; }
  uint32_t samples() const { return samples_; }
  uint32_t receivedGaps() const { return receivedGaps_; }
  const ShotSummary* recent() const { return recent_; }
  size_t recentCount() const { return recentCount_; }
  const char* error() const { return error_; }
  bool latestPath(char* destination, size_t capacity) const;

 private:
  static constexpr size_t BUFFER_CAPACITY = 4096;
  bool initialized_ = false;
  bool ready_ = false;
  bool active_ = false;
  const char* error_ = "NOT INITIALIZED";
  File file_;
  char buffer_[BUFFER_CAPACITY] = {};
  size_t bufferLength_ = 0;
  uint32_t latestId_ = 0;
  uint32_t activeId_ = 0;
  uint32_t samples_ = 0;
  uint32_t receivedGaps_ = 0;
  uint32_t lastTraceSequence_ = 0;
  uint32_t activeSessionId_ = 0;
  float lastElapsedS_ = 0.0f;
  uint32_t lastFlushMs_ = 0;
  uint32_t lastAppendMs_ = 0;
  float peakPressureBar_ = 0.0f;
  float finalWeightG_ = 0.0f;
  uint8_t resultFlags_ = 0;
  uint32_t startedAtEpoch_ = 0;
  char activeProfile_[24] = {};
  bool stepNameWritten_[16] = {};
  ShotSummary recent_[RECENT_CAPACITY]{};
  size_t recentCount_ = 0;

  bool start(const HmiProtocol::ControlTracePayload& trace,
             const char* profileName);
  void appendText(const char* text, size_t length);
  void flush();
  void loadRecent();
  void pushRecent(const ShotSummary& summary);
  bool appendSummary();
  void finishInternal(const HmiProtocol::ShotEndPayload* shotEnd);
};
