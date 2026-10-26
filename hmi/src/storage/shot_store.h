#pragma once

#include <Arduino.h>

#include "model/shot_record.h"
#include "ui/shot_chart.h"

class ShotStore {
 public:
  static constexpr size_t RECENT_CAPACITY = 40;

  bool begin();
  bool save(const ShotChart& chart, const char* profileName);
  bool load(uint32_t id, ShotChart& chart);
  bool eraseAll();
  bool ready() const { return ready_; }
  const char* error() const { return error_; }
  const ShotSummary* recent() const { return recent_; }
  size_t recentCount() const { return recentCount_; }
  uint64_t totalBytes() const;
  uint64_t usedBytes() const;

 private:
  bool initialized_ = false;
  bool ready_ = false;
  const char* error_ = "NOT INITIALIZED";
  ShotSummary recent_[RECENT_CAPACITY]{};
  size_t recentCount_ = 0;
  uint32_t lastSavedSourceTimestampMs_ = 0;

  void loadRecent();
  void pushRecent(const ShotSummary& summary);
};
