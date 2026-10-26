#pragma once

#include <Arduino.h>
#include <atomic>
#include <freertos/queue.h>

class AudioService {
 public:
  bool begin(uint8_t volume);
  bool ready() const { return ready_; }
  void setVolume(uint8_t volume);
  void touchClick();
  void testTone();

 private:
  bool ready_ = false;
  std::atomic<uint8_t> volume_{60};
  struct Tone { uint16_t frequency, duration; };
  QueueHandle_t tones_ = nullptr;
  void run();
  void renderTone(const Tone& tone);
  void playTone(uint16_t frequencyHz, uint16_t durationMs);
};
