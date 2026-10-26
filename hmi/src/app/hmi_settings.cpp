#include "app/hmi_settings.h"

#include <Preferences.h>

namespace {
constexpr char SETTINGS_NAMESPACE[] = "hmi_settings";
}

void HmiSettings::begin() {
  Preferences prefs;
  prefs.begin(SETTINGS_NAMESPACE, true);
  data_.brightness = constrain(prefs.getUChar("bright", 75), 10, 100);
  data_.volume = constrain(prefs.getUChar("volume", 60), 0, 100);
  data_.autoDimMinutes = prefs.getUChar("dim_min", 2);
  data_.touchSounds = prefs.getBool("touch_snd", true);
  data_.autoSaveShots = prefs.getBool("auto_save", true);
  prefs.end();
}

void HmiSettings::save() {
  Preferences prefs;
  prefs.begin(SETTINGS_NAMESPACE, false);
  prefs.putUChar("bright", data_.brightness);
  prefs.putUChar("volume", data_.volume);
  prefs.putUChar("dim_min", data_.autoDimMinutes);
  prefs.putBool("touch_snd", data_.touchSounds);
  prefs.putBool("auto_save", data_.autoSaveShots);
  prefs.end();
}

void HmiSettings::setBrightness(uint8_t value) {
  data_.brightness = constrain(value, 10, 100);
  save();
}

void HmiSettings::setVolume(uint8_t value) {
  data_.volume = constrain(value, 0, 100);
  save();
}

void HmiSettings::setAutoDimMinutes(uint8_t value) {
  data_.autoDimMinutes = value;
  save();
}

void HmiSettings::setTouchSounds(bool enabled) {
  data_.touchSounds = enabled;
  save();
}

void HmiSettings::setAutoSaveShots(bool enabled) {
  data_.autoSaveShots = enabled;
  save();
}
