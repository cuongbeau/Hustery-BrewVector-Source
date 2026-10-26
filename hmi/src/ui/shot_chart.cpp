#include "ui/shot_chart.h"
#include <utility>

void ShotChart::swap(ShotChart& other){
  std::swap(samples_,other.samples_);std::swap(capacity_,other.capacity_);
  std::swap(head_,other.head_);std::swap(count_,other.count_);
  std::swap(lastSourceTimestampMs_,other.lastSourceTimestampMs_);std::swap(usingPsram_,other.usingPsram_);
}

#include <esp_heap_caps.h>

ShotChart::~ShotChart() {
  if (samples_) heap_caps_free(samples_);
}

bool ShotChart::begin() {
  if (samples_) return true;
  samples_ = static_cast<ShotChartSample*>(heap_caps_malloc(
      sizeof(ShotChartSample) * PSRAM_CAPACITY,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (samples_) {
    capacity_ = PSRAM_CAPACITY;
    usingPsram_ = true;
  } else {
    samples_ = static_cast<ShotChartSample*>(heap_caps_malloc(
        sizeof(ShotChartSample) * FALLBACK_CAPACITY, MALLOC_CAP_8BIT));
    capacity_ = samples_ ? FALLBACK_CAPACITY : 0;
    usingPsram_ = false;
  }
  reset();
  return samples_ != nullptr;
}

void ShotChart::reset() {
  head_ = 0;
  count_ = 0;
  lastSourceTimestampMs_ = 0;
}

bool ShotChart::append(const UiTelemetry& telemetry) {
  if (!samples_ || capacity_ == 0 || telemetry.sourceTimestampMs == 0 ||
      telemetry.sourceTimestampMs == lastSourceTimestampMs_)
    return false;

  // A backwards elapsed clock means a new shot arrived without a UI screen
  // transition (for example after a link reconnect).
  if (count_ > 0 && telemetry.elapsedS + 0.10f < lastElapsedS()) reset();

  size_t writeIndex = 0;
  if (count_ < capacity_) {
    writeIndex = (head_ + count_) % capacity_;
    ++count_;
  } else {
    writeIndex = head_;
    head_ = (head_ + 1) % capacity_;
  }

  ShotChartSample& sample = samples_[writeIndex];
  sample.sourceTimestampMs = telemetry.sourceTimestampMs;
  sample.elapsedS = telemetry.elapsedS;
  sample.pressureBar = telemetry.pressureBar;
  sample.flowGps = telemetry.flowGps;
  sample.brewTempC = telemetry.mainConnected && telemetry.mainCompatible
      ? telemetry.boilerTempC : NAN;
  sample.weightG = telemetry.weightG;
  // No pressure target in Manual/power/flow phases or during scale prep.
  // Preserve gaps between modes instead of inventing a target at zero bar.
  sample.targetPressureBar = telemetry.pressureTargetValid ? telemetry.targetPressureBar : NAN;
  sample.targetFlowGps = telemetry.targetFlowGps;
  sample.stepIndex = telemetry.stepIndex;
  lastSourceTimestampMs_ = telemetry.sourceTimestampMs;
  return true;
}

bool ShotChart::appendSample(const ShotChartSample& sample) {
  if (!samples_ || capacity_ == 0) return false;
  size_t writeIndex = 0;
  if (count_ < capacity_) {
    writeIndex = (head_ + count_) % capacity_;
    ++count_;
  } else {
    writeIndex = head_;
    head_ = (head_ + 1) % capacity_;
  }
  samples_[writeIndex] = sample;
  lastSourceTimestampMs_ = sample.sourceTimestampMs;
  return true;
}

const ShotChartSample& ShotChart::at(size_t logicalIndex) const {
  static const ShotChartSample empty{};
  if (!samples_ || logicalIndex >= count_) return empty;
  return samples_[(head_ + logicalIndex) % capacity_];
}

float ShotChart::firstElapsedS() const {
  return count_ == 0 ? 0.0f : at(0).elapsedS;
}

float ShotChart::lastElapsedS() const {
  return count_ == 0 ? 0.0f : at(count_ - 1).elapsedS;
}

float ShotChart::lastWeightG() const {
  return count_ == 0 ? 0.0f : at(count_ - 1).weightG;
}

float ShotChart::peakPressureBar() const {
  float peak = 0.0f;
  for (size_t i = 0; i < count_; ++i) peak = max(peak, at(i).pressureBar);
  return peak;
}
