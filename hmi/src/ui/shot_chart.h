#pragma once

#include <Arduino.h>

#include "ui/ui_types.h"

struct ShotChartSample {
  uint32_t sourceTimestampMs = 0;
  float elapsedS = 0.0f;
  float pressureBar = 0.0f;
  float flowGps = 0.0f;
  float brewTempC = NAN; // Estimated brew temperature; absent in legacy samples.
  float weightG = 0.0f;
  float targetPressureBar = 0.0f;
  float targetFlowGps = 0.0f;
  uint8_t stepIndex = 0;
};

// Timestamped shot storage shared by the live Brew and frozen Result views.
// Samples live in PSRAM when available so UI rendering never constrains shot
// duration or future History/Compare work.
class ShotChart {
 public:
  ~ShotChart();

  bool begin();
  void swap(ShotChart& other);
  void reset();
  bool append(const UiTelemetry& telemetry);
  bool appendSample(const ShotChartSample& sample);

  size_t size() const { return count_; }
  size_t capacity() const { return capacity_; }
  bool usingPsram() const { return usingPsram_; }
  const ShotChartSample& at(size_t logicalIndex) const;
  float firstElapsedS() const;
  float lastElapsedS() const;
  float lastWeightG() const;
  float peakPressureBar() const;
  uint32_t lastSourceTimestampMs() const {
    return count_ == 0 ? 0 : at(count_ - 1).sourceTimestampMs;
  }

 private:
  static constexpr size_t PSRAM_CAPACITY = 3600;   // 3 min at 20 Hz
  static constexpr size_t FALLBACK_CAPACITY = 600; // 30 s at 20 Hz

  ShotChartSample* samples_ = nullptr;
  size_t capacity_ = 0;
  size_t head_ = 0;
  size_t count_ = 0;
  uint32_t lastSourceTimestampMs_ = 0;
  bool usingPsram_ = false;
};
