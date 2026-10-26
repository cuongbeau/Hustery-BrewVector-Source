#include "ui/screen_manager.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "local_access.h"

#include "ui/components.h"
#include "ui/theme.h"
#include "ui/settings_layout.h"

using namespace UiComponents;
void ScreenManager::drawSettings(const UiTelemetry& t) {
  header(canvas_, "SETTINGS", true);
  const char* titles[] = {"CONNECTIONS", "CALIBRATION", "DISPLAY & SOUND", "SYSTEM"};
  const char* details[] = {"Wi-Fi / Scale / Web", "Pressure / Temp",
                           "Brightness / Audio", "Status / Storage"};
  char display[24];
  snprintf(display, sizeof(display), "Brightness %u%%", t.brightness);
  const char* status[] = {t.wifiConnected ? "Wi-Fi connected" : "Wi-Fi offline",
                          t.mainConnected && t.mainCompatible ? "Main linked" : "Main unavailable",
                          display, t.storageReady ? "Memory card ready" : "No memory card"};
  const uint16_t accents[] = {UiTheme::CYAN, UiTheme::AMBER, UiTheme::CYAN, UiTheme::CYAN};
  for (int i = 0; i < 4; ++i) {
    const auto& r = SettingsLayout::Tiles[i];
    panel(canvas_, r.x, r.y, r.w, r.h, UiTheme::CARD_ALT, UiTheme::BORDER);
    canvas_.drawFastHLine(r.x + 12, r.y + 12, 24, accents[i]);
    textLeft(canvas_, titles[i], r.x + 12, r.y + 30, &fonts::Font2,
             UiTheme::TEXT, UiTheme::CARD_ALT);
    textLeft(canvas_, details[i], r.x + 12, r.y + 52, &fonts::Font0,
             UiTheme::MUTED, UiTheme::CARD_ALT);
    textLeft(canvas_, status[i], r.x + 12, r.y + 72, &fonts::Font0,
             accents[i], UiTheme::CARD_ALT);
  }
}

void ScreenManager::drawConnections(const UiTelemetry& t) {
  header(canvas_, "CONNECTIONS", true);
  const char* titles[] = {"Wi-Fi", "Bluetooth scale", "Open Web app"};
  const char* states[] = {t.wifiConnected ? "CONNECTED" : "OFFLINE",
                          t.scaleConnected ? "CONNECTED" : "OFFLINE", "PHONE / PC"};
  for (int i = 0; i < 3; ++i) {
    const auto& r = SettingsLayout::Connections[i];
    row(canvas_, r.x, r.y, r.w, r.h, titles[i], states[i], true);
  }
}

void ScreenManager::drawWebAccess(const UiTelemetry& t) {
  header(canvas_, "WEB APP", true);
  const bool ap = !t.wifiConnected && t.wifiApIp && t.wifiApIp[0];
  const bool reachable = t.wifiConnected || ap;
  textLeft(canvas_, t.wifiConnected ? "Use the same Wi-Fi as the machine"
                     : ap ? "Connect phone / PC to machine Wi-Fi" : "Connect the machine to Wi-Fi first",
           8, 49, &fonts::Font0, UiTheme::MUTED);
  if (ap) {
    textLeft(canvas_, t.wifiApName, 8, 70, &fonts::Font2, UiTheme::TEXT);
    char apPasswordLine[96];
    snprintf(apPasswordLine, sizeof(apPasswordLine), "Wi-Fi password: %s", BREWVECTOR_AP_PASSWORD);
    textLeft(canvas_, apPasswordLine, 8, 89, &fonts::Font0, UiTheme::MUTED);
  } else textLeft(canvas_, t.wifiConnected ? t.wifiSsid : "No network available",
                   8, 77, &fonts::Font2, UiTheme::TEXT);
  panel(canvas_, 8, 104, 304, 43, UiTheme::CARD_ALT, UiTheme::BORDER);
  char address[40];
  if (reachable) snprintf(address, sizeof(address), "http://%s/", ap ? t.wifiApIp : t.wifiIp);
  else snprintf(address, sizeof(address), "Address unavailable");
  textCenter(canvas_, address, 160, 125, &fonts::Font2, UiTheme::CYAN, UiTheme::CARD_ALT);
  textCenter(canvas_, "Profiles / Shot analysis / CSV", 160, 162, &fonts::Font0, UiTheme::MUTED);
  textCenter(canvas_, "Firmware: /update | admin login", 160, 180,
             &fonts::Font0, UiTheme::MUTED);
  const auto& r = SettingsLayout::WebWifi;
  button(canvas_, r.x, r.y, r.w, r.h, "WI-FI SETTINGS", UiTheme::CYAN, false);
}

void ScreenManager::drawWifi(const UiTelemetry& t) {
  header(canvas_, "WI-FI", true, "HMI");
  panel(canvas_, 7, 42, 306, 78, UiTheme::CARD);
  textLeft(canvas_, t.wifiStatus, 19, 59,
           &fonts::Font2, t.wifiConnected ? UiTheme::GREEN
                                         : (t.wifiConnecting ? UiTheme::CYAN
                                                             : UiTheme::AMBER),
           UiTheme::CARD);
  textLeft(canvas_, t.wifiSsid, 19, 82, &fonts::Font4, UiTheme::TEXT,
           UiTheme::CARD);
  textLeft(canvas_, t.wifiIp, 19, 105, &fonts::Font0, UiTheme::MUTED,
           UiTheme::CARD);
  char signal[18];
  snprintf(signal, sizeof(signal), t.wifiConnected ? "%ld dBm" : "--",
           static_cast<long>(t.wifiRssi));
  textRight(canvas_, signal, 300, 82, &fonts::Font2, UiTheme::CYAN,
            UiTheme::CARD);
  row(canvas_, 7, 127, 306, 42, "Auto connect", nullptr, false);
  toggle(canvas_, 254, 136, t.wifiAutoConnect);
  button(canvas_, 7, 190, 148, 42, "NETWORKS", UiTheme::CYAN, false);
  button(canvas_, 165, 190, 148, 42, "FORGET", UiTheme::RED, false);
}

void ScreenManager::drawNetworks() {
  header(canvas_, "NETWORKS", true, "HMI");
  if (!wifiNetworks_ || wifiNetworkCount_ == 0) {
    textCenter(canvas_, "Tap RESCAN to find networks", 160, 112,
               &fonts::Font2, UiTheme::MUTED);
  }
  const uint8_t count = min<uint8_t>(wifiNetworkCount_, 3);
  for (uint8_t i = 0; i < count; ++i) {
    char signal[24];
    snprintf(signal, sizeof(signal), "%ld dBm%s",
             static_cast<long>(wifiNetworks_[i].rssi),
             wifiNetworks_[i].secure ? "  LOCK" : "");
    row(canvas_, 7, 42 + i * 48, 306, 42, wifiNetworks_[i].ssid, signal,
        false, selectedNetwork_ == static_cast<int8_t>(i) ? "SELECTED" : nullptr,
        selectedNetwork_ == static_cast<int8_t>(i) ? UiTheme::CYAN
                                                   : UiTheme::TEXT);
  }
  button(canvas_, 7, 192, 148, 40, wifiScanning_ ? "SCANNING..." : "RESCAN",
         UiTheme::CYAN, false);
  button(canvas_, 165, 192, 148, 40,
         selectedNetwork_ >= 0 ? "CONNECT" : "SELECT NETWORK",
         selectedNetwork_ >= 0 ? UiTheme::CYAN : UiTheme::DIM,
         selectedNetwork_ >= 0);
}

void ScreenManager::handleKeyboardTap(int x, int y) {
  if (x >= 255 && y >= 36 && y < 74) {
    passwordVisible_ = !passwordVisible_;
    return;
  }
  if (y >= 207) {
    if (x >= 76 && x < 134) {
      if (keyboardSymbols_) keyboardSymbols_ = false;
      keyboardUpper_ = !keyboardUpper_;
    } else if (x >= 138 && x < 196) {
      keyboardSymbols_ = !keyboardSymbols_;
    }
    return;
  }
  if (y < 75 || y >= 190) return;
  if (x >= 287) {
    const size_t length = strlen(wifiPassword_);
    if (length > 0) wifiPassword_[length - 1] = '\0';
    return;
  }
  const char* rowsLower[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  const char* rowsUpper[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  const char* rowsSymbol[] = {"1234567890", "!@#$%&*()-", "_+=.?/:"};
  const char* keys = keyboardSymbols_ ? rowsSymbol[(y - 76) / 40]
                     : keyboardUpper_ ? rowsUpper[(y - 76) / 40]
                                      : rowsLower[(y - 76) / 40];
  const size_t keyCount = strlen(keys);
  if (keyCount == 0) return;
  const int keyWidth = 280 / static_cast<int>(keyCount);
  const int keyIndex = constrain((x - 4) / max(1, keyWidth), 0,
                                 static_cast<int>(keyCount) - 1);
  const size_t length = strlen(wifiPassword_);
  if (length + 1 < sizeof(wifiPassword_)) {
    wifiPassword_[length] = keys[keyIndex];
    wifiPassword_[length + 1] = '\0';
  }
}

void ScreenManager::drawWifiKeyboard() {
  header(canvas_, "WI-FI PASSWORD", true, "HMI");
  char shortSsid[25] = {};
  snprintf(shortSsid, sizeof(shortSsid), "%s", wifiSsid_);
  textLeft(canvas_, shortSsid, 8, 47, &fonts::Font2, UiTheme::CYAN);
  char passwordLine[29] = {};
  const size_t length = strlen(wifiPassword_);
  if (passwordVisible_) {
    const char* visibleStart = length > sizeof(passwordLine) - 1
                                   ? wifiPassword_ + length - sizeof(passwordLine) + 1
                                   : wifiPassword_;
    snprintf(passwordLine, sizeof(passwordLine), "%s", visibleStart);
  } else {
    const size_t dots = min<size_t>(length, sizeof(passwordLine) - 1);
    memset(passwordLine, '*', dots);
  }
  textLeft(canvas_, length ? passwordLine : "Password (blank if open)", 8, 67,
           &fonts::Font0, length ? UiTheme::TEXT : UiTheme::MUTED);
  button(canvas_, 257, 40, 59, 29, passwordVisible_ ? "HIDE" : "SHOW",
         UiTheme::AMBER, false);

  const char* rowsLower[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  const char* rowsUpper[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  const char* rowsSymbol[] = {"1234567890", "!@#$%&*()-", "_+=.?/:"};
  for (int r = 0; r < 3; ++r) {
    const char* keys = keyboardSymbols_ ? rowsSymbol[r]
                       : keyboardUpper_ ? rowsUpper[r]
                                        : rowsLower[r];
    const int count = strlen(keys);
    const int keyWidth = 280 / count;
    for (int k = 0; k < count; ++k) {
      char label[2] = {keys[k], '\0'};
      button(canvas_, 4 + k * keyWidth, 76 + r * 40, keyWidth - 2, 34,
             label, UiTheme::TEXT, false);
    }
  }
  button(canvas_, 287, 76, 29, 114, "<", UiTheme::AMBER, false);
  button(canvas_, 4, 207, 68, 29, "CANCEL", UiTheme::TEXT, false);
  button(canvas_, 76, 207, 58, 29, keyboardUpper_ ? "abc" : "ABC",
         UiTheme::AMBER, false);
  button(canvas_, 138, 207, 58, 29, keyboardSymbols_ ? "abc" : "123",
         UiTheme::AMBER, false);
  button(canvas_, 200, 207, 116, 29, "CONNECT", UiTheme::CYAN, true);
}

void ScreenManager::handleProfileNameKeyboardTap(int x, int y) {
  if (y >= 207) {
    if (x >= 76 && x < 134) {
      if (keyboardSymbols_) keyboardSymbols_ = false;
      keyboardUpper_ = !keyboardUpper_;
    } else if (x >= 138 && x < 196) {
      keyboardSymbols_ = !keyboardSymbols_;
    }
    return;
  }
  if (y < 75 || y >= 190) return;
  if (x >= 287) {
    const size_t length = strlen(profileNameEdit_);
    if (length > 0) profileNameEdit_[length - 1] = '\0';
    return;
  }
  const char* rowsLower[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  const char* rowsUpper[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  const char* rowsSymbol[] = {"1234567890", "-()&+", " _"};
  const int rowIndex = (y - 76) / 40;
  const char* keys = keyboardSymbols_ ? rowsSymbol[rowIndex]
                     : keyboardUpper_ ? rowsUpper[rowIndex]
                                      : rowsLower[rowIndex];
  const size_t keyCount = strlen(keys);
  if (keyCount == 0) return;
  const int keyWidth = 280 / static_cast<int>(keyCount);
  const int keyIndex = constrain((x - 4) / max(1, keyWidth), 0,
                                 static_cast<int>(keyCount) - 1);
  const size_t length = strlen(profileNameEdit_);
  if (length + 1 < sizeof(profileNameEdit_)) {
    profileNameEdit_[length] = keys[keyIndex];
    profileNameEdit_[length + 1] = '\0';
  }
}

void ScreenManager::drawProfileNameKeyboard() {
  header(canvas_, "PROFILE NAME", true, "DRAFT");
  textLeft(canvas_, profileNameEdit_[0] ? profileNameEdit_ : "Name is required",
           8, 58, &fonts::Font2,
           profileNameEdit_[0] ? UiTheme::CYAN : UiTheme::RED);
  const char* rowsLower[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  const char* rowsUpper[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  const char* rowsSymbol[] = {"1234567890", "-()&+", " _"};
  for (int r = 0; r < 3; ++r) {
    const char* keys = keyboardSymbols_ ? rowsSymbol[r]
                       : keyboardUpper_ ? rowsUpper[r]
                                        : rowsLower[r];
    const int count = strlen(keys);
    const int keyWidth = 280 / count;
    for (int k = 0; k < count; ++k) {
      char label[2] = {keys[k], '\0'};
      button(canvas_, 4 + k * keyWidth, 76 + r * 40, keyWidth - 2, 34,
             label[0] == ' ' ? "SP" : label, UiTheme::TEXT, false);
    }
  }
  button(canvas_, 287, 76, 29, 114, "<", UiTheme::AMBER, false);
  button(canvas_, 4, 207, 68, 29, "CANCEL", UiTheme::TEXT, false);
  button(canvas_, 76, 207, 58, 29, keyboardUpper_ ? "abc" : "ABC",
         UiTheme::AMBER, false);
  button(canvas_, 138, 207, 58, 29, keyboardSymbols_ ? "abc" : "123",
         UiTheme::AMBER, false);
  button(canvas_, 200, 207, 116, 29, "DONE", UiTheme::CYAN, true);
}

void ScreenManager::drawQuickConvertConfirm() {
  header(canvas_, "CONVERT TO QUICK", true, "DRAFT");
  panel(canvas_, 16, 58, 288, 108, UiTheme::CARD, UiTheme::AMBER);
  textCenter(canvas_, "REBUILD AS 5-STEP PROFILE?", 160, 89, &fonts::Font4,
             UiTheme::AMBER, UiTheme::CARD);
  textCenter(canvas_, "Advanced step edits will be replaced", 160, 122,
             &fonts::Font0, UiTheme::MUTED, UiTheme::CARD);
  textCenter(canvas_, "Duplicate first if you need both versions", 160, 143,
             &fonts::Font0, UiTheme::MUTED, UiTheme::CARD);
  button(canvas_, 7, 184, 148, 45, "CANCEL", UiTheme::TEXT, false);
  button(canvas_, 165, 184, 148, 45, "CONVERT", UiTheme::AMBER, true);
}

void ScreenManager::drawScale(const UiTelemetry& t) {
  header(canvas_, "SCALE SETTINGS", true, "BLE");

  panel(canvas_, 7, 40, 306, 144, UiTheme::CARD,
        t.scaleConnected ? UiTheme::GREEN : UiTheme::BORDER);

  const char* devName = (t.scaleDeviceName && t.scaleDeviceName[0] && strcmp(t.scaleDeviceName, "--") != 0)
                            ? t.scaleDeviceName
                            : (t.scaleConnected ? "Hustery Scale" : "NO PAIRED SCALE");
  textLeft(canvas_, devName, 18, 56, &fonts::Font2, UiTheme::TEXT);

  const char* statusText = t.scaleConnected
                               ? "CONNECTED"
                               : (t.scaleScanning ? "SCANNING" : "OFFLINE");
  const uint16_t statusColor = t.scaleConnected ? UiTheme::GREEN
                             : (t.scaleScanning ? UiTheme::CYAN : UiTheme::MUTED);
  panel(canvas_, 206, 46, 96, 20, UiTheme::BG, statusColor, 4);
  textCenter(canvas_, statusText, 254, 56, &fonts::Font2, statusColor, UiTheme::BG);

  char macInfo[48];
  if (t.scaleDeviceMac && t.scaleDeviceMac[0] && strcmp(t.scaleDeviceMac, "--") != 0) {
    snprintf(macInfo, sizeof(macInfo), "MAC: %s  (%d dBm)", t.scaleDeviceMac, t.scaleRssi);
  } else {
    snprintf(macInfo, sizeof(macInfo), "No MAC saved (Tap SCAN to pair)");
  }
  textLeft(canvas_, macInfo, 18, 80, &fonts::Font2, UiTheme::MUTED);

  canvas_.drawFastHLine(18, 94, 284, UiTheme::BORDER);

  char liveStr[32];
  snprintf(liveStr, sizeof(liveStr), "Live: %.1f g", t.weightG);
  textLeft(canvas_, liveStr, 18, 120, &fonts::FreeSansBold12pt7b,
           t.scaleConnected ? UiTheme::CYAN : UiTheme::MUTED);

  char battStr[24];
  if (t.scaleBatteryPercent <= 100) {
    snprintf(battStr, sizeof(battStr), "Battery: %u%%", t.scaleBatteryPercent);
  } else {
    snprintf(battStr, sizeof(battStr), "Battery: --");
  }
  textLeft(canvas_, battStr, 185, 120, &fonts::Font2,
           t.scaleBatteryPercent <= 100 ? UiTheme::GREEN : UiTheme::MUTED);

  if (t.scaleConnected) {
    textLeft(canvas_, t.scaleStable ? "STABLE" : "MOVING", 18, 150,
             &fonts::Font2,
             t.scaleStable ? UiTheme::GREEN : UiTheme::AMBER);
  } else {
    textLeft(canvas_, "Auto-reconnect standby", 18, 150,
             &fonts::Font2, UiTheme::MUTED);
  }

  button(canvas_, 7, 192, 148, 42, "SCAN NEW SCALE", UiTheme::CYAN, false);
  button(canvas_, 165, 192, 148, 42, "FORGET SCALE", UiTheme::AMBER, false);
}

void ScreenManager::drawScaleDevices(const UiTelemetry& t) {
  header(canvas_, "NEARBY SCALES", true, t.scaleScanning ? "SCANNING" : "SELECT");

  if (scaleDeviceCount_ == 0) {
    panel(canvas_, 7, 42, 306, 142, UiTheme::CARD, UiTheme::BORDER);
    textCenter(canvas_, t.scaleScanning ? "Searching for BLE scales..." : "No scales found nearby",
               160, 95, &fonts::Font2, UiTheme::MUTED);
    textCenter(canvas_, "Ensure your scale is turned ON", 160, 125,
               &fonts::Font2, UiTheme::MUTED);
  } else {
    const uint8_t limit = min<uint8_t>(scaleDeviceCount_, 4);
    for (uint8_t i = 0; i < limit; ++i) {
      const auto& scale = scaleDevices_[i];
      const int y = 42 + i * 36;
      const bool selected = (selectedScaleDevice_ == static_cast<int8_t>(i));

      panel(canvas_, 7, y, 306, 33, selected ? UiTheme::CARD_ALT : UiTheme::CARD,
            selected ? UiTheme::CYAN : UiTheme::BORDER);

      textLeft(canvas_, scale.name[0] ? scale.name : "BLE Scale", 16, y + 16,
               &fonts::Font2, selected ? UiTheme::CYAN : UiTheme::TEXT);

      char details[32];
      snprintf(details, sizeof(details), "%d dBm", scale.rssi);
      textRight(canvas_, details, 300, y + 16, &fonts::Font2, UiTheme::MUTED);
    }
  }

  button(canvas_, 7, 192, 148, 42, t.scaleScanning ? "SCANNING..." : "RESCAN",
         UiTheme::TEXT, false);
  button(canvas_, 165, 192, 148, 42, "PAIR & SAVE",
         selectedScaleDevice_ >= 0 ? UiTheme::CYAN : UiTheme::MUTED,
         selectedScaleDevice_ >= 0);
}



void ScreenManager::drawCalibration() {
  header(canvas_, "CALIBRATION", true);
  row(canvas_, 8, 52, 304, 58, "Pressure sensor", "WIZARD", true);
  row(canvas_, 8, 126, 304, 58, "Brew temperature", "OFFSET", true);
}

void ScreenManager::drawThermalCal(const UiTelemetry& t) {
  header(canvas_, "BREW TEMP OFFSET", true, "MAIN");
  char value[40];
  textCenter(canvas_, "Estimated brew = block - offset", 160, 49, &fonts::Font0, UiTheme::MUTED);
  if (t.mainConnected && t.mainCompatible && isfinite(t.blockTempC))
    snprintf(value, sizeof(value), "Block %.1f C / Brew %.1f C", t.blockTempC, t.boilerTempC);
  else snprintf(value, sizeof(value), "MAIN UNAVAILABLE");
  textCenter(canvas_, value, 160, 72, &fonts::Font2, UiTheme::TEXT);
  const bool editable = thermalCalEditable_ && thermalCalLoaded_ && !thermalCalPendingId_ && !thermalCalConflict_;
  button(canvas_, 8, 96, 80, 44, "- 0.1", editable ? UiTheme::CYAN : UiTheme::DIM, editable);
  if (thermalCalLoaded_) snprintf(value, sizeof(value), "%.1f C", thermalOffsetDraft_);
  else snprintf(value, sizeof(value), "--");
  textCenter(canvas_, value, 160, 109, &fonts::Font4, UiTheme::TEXT);
  button(canvas_, 232, 96, 80, 44, "+ 0.1", editable ? UiTheme::CYAN : UiTheme::DIM, editable);
  textCenter(canvas_, "Higher offset = lower brew estimate", 160, 155, &fonts::Font0, UiTheme::MUTED);
  textCenter(canvas_, thermalCalMessage_ ? thermalCalMessage_ :
      thermalCalEditable_ ? "0-60 C / Save while idle" : "Save available when Main is idle", 160, 181,
      &fonts::Font0, UiTheme::AMBER);
  button(canvas_, 8, 202, 140, 34, "RELOAD", UiTheme::DIM, !thermalCalPendingId_);
  button(canvas_, 172, 202, 140, 34, "SAVE", editable && thermalCalDirty_ ? UiTheme::CYAN : UiTheme::DIM,
      editable && thermalCalDirty_);
}

void ScreenManager::drawPressureCal(const UiTelemetry& t) {
  header(canvas_, "PRESSURE CAL", true, "MAIN");
  const bool ready = t.mainConnected && t.mainCompatible;
  char value[32];
  char adc[32];
  snprintf(value, sizeof(value), "%.2f bar", t.pressureBar);
  snprintf(adc, sizeof(adc), isfinite(t.pressureAdcVolts) ? "%.3f V" : "--",
           t.pressureAdcVolts);

  if (pressureCalStep_ == PressureCalStep::Overview) {
    textCenter(canvas_, "PRESSURE SENSOR WIZARD", 160, 49, &fonts::Font2,
               UiTheme::CYAN);
    panel(canvas_, 12, 63, 296, 76, UiTheme::CARD,
          ready ? UiTheme::CYAN : UiTheme::RED);
    textLeft(canvas_, ready ? "MAIN READY" : "MAIN OFFLINE", 24, 76,
             &fonts::Font2, ready ? UiTheme::GREEN : UiTheme::RED,
             UiTheme::CARD);
    textLeft(canvas_, "LIVE", 24, 106, &fonts::Font0, UiTheme::MUTED,
             UiTheme::CARD);
    textRight(canvas_, value, 294, 103, &fonts::Font4, UiTheme::TEXT,
              UiTheme::CARD);
    row(canvas_, 12, 147, 296, 27, "Offset", t.calibrationAvailable ? "LOADED" : "READING...",
        false);
    snprintf(value, sizeof(value), "%.3f V / %.2f bar/V", t.pressureOffsetVolts,
             t.pressureScaleBarPerVolt);
    textCenter(canvas_, value, 160, 180, &fonts::Font0, UiTheme::MUTED);
    button(canvas_, 12, 198, 296, 35, "START WIZARD", ready ? UiTheme::CYAN : UiTheme::DIM,
           ready);
    return;
  }

  if (pressureCalStep_ == PressureCalStep::Zero) {
    textCenter(canvas_, "STEP 1 / 2  -  ZERO", 160, 48, &fonts::Font2,
               UiTheme::CYAN);
    textCenter(canvas_, "Vent freely. Confirm 0 bar before saving.", 160, 69,
               &fonts::Font0, UiTheme::MUTED);
    textCenter(canvas_, value, 160, 92, &fonts::Font6, UiTheme::TEXT);
    row(canvas_, 12, 125, 296, 30, "ADC live", adc, false);
    row(canvas_, 12, 158, 296, 25, "Machine", ready ? "READY" : "OFFLINE", false,
        nullptr, ready ? UiTheme::GREEN : UiTheme::RED);
    if (pressureCalMessage_)
      textCenter(canvas_, pressureCalMessage_, 160, 190, &fonts::Font2,
                 pressureCalPendingAction_ == UiAction::None ? UiTheme::AMBER
                                                               : UiTheme::CYAN);
    button(canvas_, 12, 204, 296, 29, "SAVE ZERO @ 0 BAR", ready ? UiTheme::CYAN : UiTheme::DIM,
           ready && pressureCalPendingAction_ == UiAction::None);
    return;
  }

  if (pressureCalStep_ == PressureCalStep::Reference) {
    textCenter(canvas_, "STEP 2 / 2  -  REFERENCE", 160, 48, &fonts::Font2,
               UiTheme::AMBER);
    textCenter(canvas_, "Blind basket + mechanical gauge required.", 160, 69,
               &fonts::Font0, UiTheme::MUTED);
    row(canvas_, 12, 82, 296, 28, "Live pressure", value, false);
    snprintf(value, sizeof(value), "%.1f bar", pressureCalibrationReferenceBar_);
    panel(canvas_, 12, 118, 296, 42, UiTheme::CARD, UiTheme::AMBER);
    button(canvas_, 16, 122, 84, 34, "- 0.1", UiTheme::DIM, true);
    textCenter(canvas_, value, 160, 130, &fonts::Font4, UiTheme::TEXT,
               UiTheme::CARD);
    button(canvas_, 220, 122, 84, 34, "+ 0.1", UiTheme::DIM, true);
    textCenter(canvas_, "Set this to the mechanical gauge reading.", 160, 166,
               &fonts::Font0, UiTheme::MUTED);
    if (pressureCalTestRunning_) {
      snprintf(value, sizeof(value), "TEST RUN  •  PUMP %.0f%%", t.pumpPercent);
      textCenter(canvas_, value, 160, 171, &fonts::Font2, UiTheme::GREEN);
    } else if (pressureCalMessage_) {
      textCenter(canvas_, pressureCalMessage_, 160, 171, &fonts::Font2,
                 UiTheme::AMBER);
    } else {
      textCenter(canvas_, "Start test; use encoder to reduce pump power.", 160, 171,
                 &fonts::Font0, UiTheme::MUTED);
    }
    button(canvas_, 12, 181, 296, 22,
           pressureCalTestRunning_ ? "STOP TEST" : "START TEST PUMP",
           pressureCalTestRunning_ ? UiTheme::RED : UiTheme::GREEN, ready);
    button(canvas_, 12, 207, 296, 26, "CAPTURE & SAVE",
           pressureCalTestRunning_ ? UiTheme::AMBER : UiTheme::DIM,
           pressureCalTestRunning_ && pressureCalPendingAction_ == UiAction::None);
    return;
  }

  textCenter(canvas_, "CALIBRATION SAVED", 160, 50, &fonts::Font2,
             UiTheme::GREEN);
  textCenter(canvas_, "Pressure mapping is active on Main.", 160, 74,
             &fonts::Font0, UiTheme::MUTED);
  panel(canvas_, 12, 91, 296, 75, UiTheme::CARD, UiTheme::GREEN);
  snprintf(value, sizeof(value), "Offset  %.3f V", t.pressureOffsetVolts);
  textLeft(canvas_, value, 24, 106, &fonts::Font2, UiTheme::TEXT,
           UiTheme::CARD);
  snprintf(value, sizeof(value), "Scale   %.2f bar/V", t.pressureScaleBarPerVolt);
  textLeft(canvas_, value, 24, 137, &fonts::Font2, UiTheme::TEXT,
           UiTheme::CARD);
  textCenter(canvas_, "Review complete. Tap DONE to return.", 160, 181,
             &fonts::Font0, UiTheme::MUTED);
  button(canvas_, 12, 204, 296, 29, "DONE", UiTheme::GREEN, true);
}

void ScreenManager::drawMachine() {
  header(canvas_, "MACHINE", true, "MAIN");
  panel(canvas_, 14, 54, 292, 105, UiTheme::CARD, UiTheme::AMBER);
  textCenter(canvas_, "MAIN-OWNED SETTINGS", 160, 82, &fonts::Font4,
             UiTheme::AMBER, UiTheme::CARD);
  textCenter(canvas_, "Flush duration, units and scale-loss policy", 160, 113,
             &fonts::Font0, UiTheme::MUTED, UiTheme::CARD);
  textCenter(canvas_, "will be enabled with protocol v0.3", 160, 137,
             &fonts::Font0, UiTheme::MUTED, UiTheme::CARD);
  button(canvas_, 14, 181, 292, 42, "NO MAIN CHANGES IN THIS BUILD",
         UiTheme::DIM, false);
}

void ScreenManager::drawDisplaySound(const UiTelemetry& t) {
  header(canvas_, "DISPLAY & SOUND", true, "HMI");
  char brightness[8];
  snprintf(brightness, sizeof(brightness), "%u%%", t.brightness);
  row(canvas_, 7, 42, 306, 39, "Brightness", brightness, false);
  slider(canvas_, 145, 61, 105, t.brightness);
  char dim[12];
  snprintf(dim, sizeof(dim), t.autoDimMinutes ? "%u min" : "OFF",
           t.autoDimMinutes);
  row(canvas_, 7, 84, 306, 39, "Auto dim", dim, true);
  row(canvas_, 7, 126, 306, 39, "Touch sounds", nullptr, false);
  toggle(canvas_, 254, 133, t.touchSounds);
  char volume[8];
  snprintf(volume, sizeof(volume), "%u%%", t.volume);
  row(canvas_, 7, 168, 306, 39, "Volume", volume, false);
  slider(canvas_, 145, 187, 105, t.volume);
  row(canvas_, 7, 210, 306, 25, "Test speaker", t.audioReady ? "PLAY" : "ERROR",
      true, nullptr, t.audioReady ? UiTheme::CYAN : UiTheme::RED);
}

void ScreenManager::drawStorageLogs(const UiTelemetry& t) {
  header(canvas_, "STORAGE & LOGS", true, "HMI");
  panel(canvas_, 7, 42, 306, 46, UiTheme::CARD, UiTheme::CYAN);
  textLeft(canvas_, "TF CARD", 18, 57, &fonts::Font2, UiTheme::TEXT,
           UiTheme::CARD);
  char capacity[32];
  if (t.storageReady)
    snprintf(capacity, sizeof(capacity), "%.1f / %.1f GB used",
             t.storageUsedBytes / 1073741824.0,
             t.storageTotalBytes / 1073741824.0);
  else
    snprintf(capacity, sizeof(capacity), "No card mounted");
  textLeft(canvas_, capacity,
           18, 76, &fonts::Font0, UiTheme::MUTED,
           UiTheme::CARD);
  textRight(canvas_, t.storageReady ? "READY" : "NOT READY", 300, 65,
            &fonts::Font2, t.storageReady ? UiTheme::GREEN : UiTheme::AMBER,
            UiTheme::CARD);
  row(canvas_, 7, 94, 306, 33, "Auto-save shots", nullptr, false);
  toggle(canvas_, 254, 99, t.autoSaveShots);
  textLeft(canvas_, "Download CSV in Web app > History", 17, 148,
           &fonts::Font0, UiTheme::MUTED);
  textLeft(canvas_, "Open Settings > Connections > Web", 17, 181,
           &fonts::Font0, UiTheme::MUTED);
  const bool eraseNotice =
      t.saveStatus != nullptr && strncmp(t.saveStatus, "ERASE", 5) == 0;
  const bool eraseSucceeded =
      eraseNotice && strcmp(t.saveStatus, "ERASED") == 0;
  row(canvas_, 7, 205, 306, 30, "Erase history",
      eraseNotice ? t.saveStatus : nullptr, !eraseNotice, nullptr,
      eraseSucceeded ? UiTheme::GREEN : UiTheme::RED);
}

void ScreenManager::drawSystem(const UiTelemetry& t) {
  header(canvas_, "SYSTEM", true);
  textLeft(canvas_, "Main", 12, 52, &fonts::Font2, UiTheme::TEXT);
  textRight(canvas_, !t.mainConnected ? "OFFLINE" : t.mainCompatible ? "CONNECTED" : "UPDATE REQUIRED",
            308, 52, &fonts::Font2,
            t.mainConnected && t.mainCompatible ? UiTheme::GREEN : UiTheme::AMBER);
  textLeft(canvas_, "Touch display", 12, 72, &fonts::Font0, UiTheme::MUTED);
  textRight(canvas_, t.touchReady ? "TOUCH READY" : "TOUCH UNAVAILABLE", 308, 72,
            &fonts::Font0, t.touchReady ? UiTheme::GREEN : UiTheme::AMBER);
  const char* titles[] = {"Heater PID", "Device status", "Storage & Auto-save", "Restart display"};
  for (int i = 0; i < 4; ++i) {
    const auto& r = SettingsLayout::SystemRows[i];
    row(canvas_, r.x, r.y, r.w, r.h, titles[i], nullptr, true);
  }
}

void ScreenManager::drawDiagnostics(const UiTelemetry& t) {
  header(canvas_, "DIAGNOSTICS", true, "HMI");
  const char* labels[] = {"Main -> HMI", "HMI -> Main", "UART errors",
                          "Wi-Fi", "Touch / Audio", "TF / Memory"};
  char mainRx[30], reverse[28], uart[34], wifi[24], hardware[24], storage[32];
  if (t.mainConnected) {
    snprintf(mainRx, sizeof(mainRx), "%s R%u %lums",
             t.mainCompatible ? "OK" : "REV?",
             static_cast<unsigned>(t.mainSchemaRevision),
             static_cast<unsigned long>(t.mainFrameAgeMs));
  } else {
    snprintf(mainRx, sizeof(mainRx), "OFFLINE");
  }
  snprintf(reverse, sizeof(reverse), "%s  reconnect %lu",
           t.mainSeesHmiCompatible ? "OK" :
               (t.mainSeesHmi ? "REV?" : "LOST"),
           static_cast<unsigned long>(t.uartReconnects));
  snprintf(uart, sizeof(uart), "CRC %lu  FRM %lu  STALE %lu",
           static_cast<unsigned long>(t.uartCrcErrors),
           static_cast<unsigned long>(t.uartFramingErrors),
           static_cast<unsigned long>(t.uartStaleResets));
  snprintf(wifi, sizeof(wifi), t.wifiConnected ? "%ld dBm" : "OFFLINE",
           static_cast<long>(t.wifiRssi));
  snprintf(hardware, sizeof(hardware), "%s / %s",
           t.touchReady ? "TOUCH OK" : "TOUCH ERR",
           t.audioReady ? "AUDIO OK" : "AUDIO ERR");
  snprintf(storage, sizeof(storage), "%s  H%.0fK P%.0fK",
           t.storageReady ? "TF OK" : "TF ERR",
           t.freeHeapBytes / 1024.0f, t.freePsramBytes / 1024.0f);
  const char* values[] = {mainRx, reverse, uart, wifi, hardware, storage};
  const bool uartRecentError = t.uartLastErrorAgeMs < 5000U;
  for (int i = 0; i < 6; ++i) {
    const bool bad = (i == 0 && !t.mainCompatible) ||
                     (i == 1 && !t.mainSeesHmiCompatible) ||
                     (i == 2 && uartRecentError) ||
                     (i == 3 && !t.wifiConnected) ||
                     (i == 4 && (!t.touchReady || !t.audioReady)) ||
                     (i == 5 && !t.storageReady);
    row(canvas_, 7, 37 + i * 31, 306, 27, labels[i], values[i], false,
        nullptr, bad ? UiTheme::RED
                     : (i != 2 ? UiTheme::GREEN : UiTheme::TEXT));
  }
  char uptime[36];
  snprintf(uptime, sizeof(uptime), "UPTIME  %luh %02lum",
           static_cast<unsigned long>(t.uptimeSeconds / 3600),
           static_cast<unsigned long>((t.uptimeSeconds / 60) % 60));
  textCenter(canvas_, uptime, 160, 228, &fonts::Font0, UiTheme::MUTED);
}

void ScreenManager::drawRestartConfirm() {
  header(canvas_, "RESTART HMI", true, "HMI");
  panel(canvas_, 16, 60, 288, 102, UiTheme::CARD, UiTheme::AMBER);
  textCenter(canvas_, "RESTART TOUCH DISPLAY?", 160, 91, &fonts::Font4,
             UiTheme::AMBER, UiTheme::CARD);
  textCenter(canvas_, "Brew control on Main is not interrupted", 160, 126,
             &fonts::Font0, UiTheme::MUTED, UiTheme::CARD);
  button(canvas_, 7, 184, 148, 45, "CANCEL", UiTheme::TEXT, false);
  button(canvas_, 165, 184, 148, 45, "RESTART", UiTheme::RED, true);
}

void ScreenManager::drawFirmware(const UiTelemetry& t) {
  header(canvas_, "FIRMWARE", true, "HMI");
  panel(canvas_, 7, 42, 148, 63, UiTheme::CARD);
  panel(canvas_, 165, 42, 148, 63, UiTheme::CARD);
  textCenter(canvas_, "HMI", 81, 60, &fonts::Font2, UiTheme::TEXT,
             UiTheme::CARD);
  textCenter(canvas_, "v0.3.0", 81, 84, &fonts::Font2, UiTheme::TEXT,
             UiTheme::CARD);
  textCenter(canvas_, "MAIN", 239, 60, &fonts::Font2, UiTheme::TEXT,
             UiTheme::CARD);
  textCenter(canvas_, t.mainConnected ? "LINKED" : "OFFLINE", 239, 84,
             &fonts::Font2, t.mainConnected ? UiTheme::GREEN : UiTheme::RED,
             UiTheme::CARD);
  textCenter(canvas_, "WEB OTA READY", 160, 121, &fonts::Font2,
             t.wifiConnected ? UiTheme::GREEN : UiTheme::AMBER);
  char otaAddress[48];
  snprintf(otaAddress, sizeof(otaAddress), "http://%s/update", t.wifiIp);
  panel(canvas_, 7, 141, 306, 45, UiTheme::CARD);
  textCenter(canvas_, otaAddress, 160, 163, &fonts::Font2, UiTheme::CYAN,
             UiTheme::CARD);
  textCenter(canvas_, "Open this address on phone or PC", 160, 205,
             &fonts::Font0, UiTheme::MUTED);
  textCenter(canvas_, "OTA: admin + private password", 160, 225,
             &fonts::Font0, UiTheme::AMBER);
}
