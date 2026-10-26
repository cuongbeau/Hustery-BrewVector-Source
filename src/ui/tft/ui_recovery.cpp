#include "ui_recovery.h"

#include <Arduino.h>

#include "protocol/hmi_protocol.h"
#include "src/communication/hmi_link.h"
#include "src/ui/tft/tft_driver.h"
#include "src/recovery/recovery_service.h"

namespace {
constexpr uint16_t BG = 0x0209;
constexpr uint16_t PANEL = 0x0B11;
constexpr uint16_t CYAN = 0x06BF;
constexpr uint16_t RED = 0xF9A8;
constexpr uint16_t MUTED = 0x94B2;

void centered(const char* text, int y, uint16_t color,
              const lgfx::IFont* font) {
  tft.setFont(font);
  tft.setTextDatum(textdatum_t::top_center);
  tft.setTextColor(color, BG);
  tft.drawString(text, W / 2, y);
}
}  // namespace

void uiRecoveryScreen() {
  tft.fillScreen(BG);
  tft.fillRect(0, 0, W, 7, RED);
  centered("HMI CONNECTION LOST", 18, RED, &fonts::FreeSansBold18pt7b);
  centered("RECOVERY WI-FI ACTIVE", 60, CYAN,
           &fonts::FreeSansBold12pt7b);

  tft.fillRoundRect(34, 96, W - 68, 150, 12, PANEL);
  tft.setTextDatum(textdatum_t::top_left);
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(MUTED, PANEL);
  tft.drawString("Wi-Fi", 56, 111);
  tft.drawString("Password", 56, 158);
  tft.drawString("Open in browser", 56, 205);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(TFT_WHITE, PANEL);
  tft.drawString(RecoveryService::ssid(), 165, 107);
  tft.drawString(RECOVERY_AP_PASS, 165, 154);
  tft.setTextColor(CYAN, PANEL);
  tft.drawString("192.168.4.1", 165, 201);

  centered("Check TX / RX / GND  |  Hold knob 5s to exit", 270, MUTED,
           &fonts::FreeSans9pt7b);
  char revision[48];
  snprintf(revision, sizeof(revision), "Main protocol %u  |  HMI %u",
           HmiProtocol::SCHEMA_REVISION,
           HmiLink::remoteSchemaRevision());
  centered(revision, 296, MUTED, &fonts::Font0);
}

void uiRecoveryWarning(bool incompatible) {
  static uint32_t lastDrawMs = 0;
  const uint32_t nowMs = millis();
  if (nowMs - lastDrawMs < 350U) return;
  lastDrawMs = nowMs;
  const int x = 128;
  const int width = 224;
  tft.fillRoundRect(x, 3, width, 24, 5, 0x4000);
  tft.setFont(&fonts::FreeSansBold9pt7b);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(TFT_WHITE, 0x4000);
  tft.drawString(incompatible ? "HMI FIRMWARE INCOMPATIBLE"
                              : "HMI OFFLINE - MAIN ACTIVE",
                 W / 2, 15);
}
