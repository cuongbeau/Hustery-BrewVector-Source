// File: src/ui/tft/ui_settings.cpp
#include "ui_settings.h"
#include "ui_common.h"
#include "src/hardware/encoder.h"
#include "interfaces/system_state.h"
#include "src/recovery/recovery_service.h"
#include "src/communication/hmi_link.h"
#include <Update.h>
#include "src/app/orchestrator.h"

#ifndef BUILD_VERSION
#define BUILD_VERSION "Unknown"
#endif

#include "src/app/state_machine.h"

enum SettingItem {
  ITEM_VERSION = 0,
  ITEM_HMI_LINK,
  ITEM_RECOVERY_WIFI,
  ITEM_FW_ROLLBACK,
  ITEM_REBOOT,
  ITEM_MAX
};

static int currentItem = ITEM_VERSION;
static bool isEditing = false;
static bool needRedraw = true;

static void drawMenu() {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_TEXT, COL_BG);
  F::medb();
  drawCentered(240, 20, "SETTINGS", COL_YELL, true);

  tft.drawLine(20, 45, 460, 45, COL_BORDER);

  F::small();
  tft.setTextDatum(textdatum_t::middle_left);

  for (int i = 0; i < ITEM_MAX; i++) {
    int y = 70 + i * 35;
    uint16_t color = (i == currentItem) ? (isEditing ? COL_RED : COL_MINT) : COL_TEXT;
    tft.setTextColor(color, COL_BG);

    // Draw cursor indicator
    if (i == currentItem) {
      tft.fillTriangle(10, y - 5, 10, y + 5, 20, y, color);
    } else {
      tft.fillTriangle(10, y - 5, 10, y + 5, 20, y, COL_BG);
    }

    tft.setCursor(30, y - 7);
    switch (i) {
      case ITEM_VERSION:
        tft.printf("Firmware Version: %s", BUILD_VERSION);
        break;
      case ITEM_HMI_LINK:
        tft.printf("HMI Link: %s (Protocol %u)",
                   HmiLink::compatible() ? "ONLINE" :
                   HmiLink::connected() ? "INCOMPATIBLE" : "OFFLINE",
                   HmiLink::remoteSchemaRevision());
        break;
      case ITEM_RECOVERY_WIFI:
        tft.printf("Recovery WiFi: %s", RecoveryService::active() ? "ON" : "OFF");
        break;
      case ITEM_FW_ROLLBACK:
        tft.print("Rollback Firmware (OTA Backup)");
        break;
      case ITEM_REBOOT:
        tft.print("Reboot System");
        break;
    }
  }

  // Draw hints
  F::tiny();
  tft.setTextColor(0x7BEF, COL_BG);
  drawCentered(240, 300, "Turn: Select | Hold: Confirm | Click: Back", 0x7BEF);
}

void uiSettingsStart() {
  currentItem = ITEM_VERSION;
  isEditing = false;
  needRedraw = true;
  Enc::readSteps(); // clear step
}

void uiSettingsDeinit() {
  tft.fillScreen(COL_BG);
}

void uiSettingsUpdate() {
  if (needRedraw) {
    drawMenu();
    needRedraw = false;
  }

  int step = Enc::readSteps();
  
  bool shortClick = StateMachine::settingsShortClick;
  bool longPress  = StateMachine::settingsLongPress;
  StateMachine::settingsShortClick = false;
  StateMachine::settingsLongPress  = false;

  if (step != 0) {
    if (!isEditing) {
      currentItem += step;
      if (currentItem < 0) currentItem = 0;
      if (currentItem >= ITEM_MAX) currentItem = ITEM_MAX - 1;
      needRedraw = true;
    }
  }

  if (shortClick) {
    if (isEditing) {
      isEditing = false; // Cancel edit
      needRedraw = true;
    } else {
      Orchestrator::forceIdle(); // Back/Exit
    }
  }

  if (longPress) {
    if (!isEditing) {
      // Enter Edit or Execute Action
      if (currentItem == ITEM_HMI_LINK) {
        needRedraw = true;
      }
      else if (currentItem == ITEM_RECOVERY_WIFI) {
        RecoveryService::toggle();
        Orchestrator::forceIdle();
        needRedraw = true;
      }
      else if (currentItem == ITEM_FW_ROLLBACK) {
        tft.fillScreen(COL_BG);
        if (Update.canRollBack()) {
          drawCentered(240, 160, "Rolling back...", COL_YELL);
          if (Update.rollBack()) {
            drawCentered(240, 190, "Rollback OK! Rebooting...", COL_MINT);
            delay(1500);
            ESP.restart();
          } else {
            drawCentered(240, 190, "Rollback FAILED!", COL_RED);
            delay(2000);
          }
        } else {
          drawCentered(240, 160, "No valid backup to rollback!", COL_RED);
          delay(2000);
        }
        needRedraw = true;
      }
      else if (currentItem == ITEM_REBOOT) {
        tft.fillScreen(COL_BG);
        drawCentered(240, 160, "Rebooting...", COL_MINT);
        delay(500);
        ESP.restart();
      }
    }
  }
}
