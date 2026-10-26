#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "local_access.h"

#define RECOVERY_AP_PASS BREWVECTOR_RECOVERY_AP_PASSWORD

extern WebServer recoveryServer;

namespace RecoveryService {
  // Prepare the emergency portal. The AP remains off until explicitly enabled
  // or until the HMI link has been unavailable for the recovery timeout.
  void begin();
  void poll();

  bool active();
  bool hmiWarningActive();
  bool hmiIncompatible();
  const char* ssid();

  void toggle();
}
