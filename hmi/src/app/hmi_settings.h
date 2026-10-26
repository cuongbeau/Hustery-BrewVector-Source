#pragma once

#include <Arduino.h>

struct HmiSettingsData {
  uint8_t brightness = 75;
  uint8_t volume = 60;
  uint8_t autoDimMinutes = 2;
  bool touchSounds = true;
  bool autoSaveShots = true;
};

class HmiSettings {
 public:
  void begin();
  const HmiSettingsData& data() const { return data_; }
  void setBrightness(uint8_t value);
  void setVolume(uint8_t value);
  void setAutoDimMinutes(uint8_t value);
  void setTouchSounds(bool enabled);
  void setAutoSaveShots(bool enabled);

 private:
  HmiSettingsData data_{};
  void save();
};
