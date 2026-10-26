// File: src/hardware/relay_driver.cpp
// Tách từ control_logic.h dòng 99-108, 813-817
// Refactor: 2026-04-20

#include "relay_driver.h"
#include "config/hardware_pins.h"

namespace RelayDriver {

  static uint32_t relayPulseEndMs = 0;

  void init() {
    pinMode(RELAY_SHOT_PIN, OUTPUT);
    digitalWrite(RELAY_SHOT_PIN, HIGH); // Inactive (Active-Low)
  }

  void pulse(uint32_t ms) {
    digitalWrite(RELAY_SHOT_PIN, LOW); // Active Low
    relayPulseEndMs = millis() + ms;
    Serial.printf("[Relay] Pulse Start (%d ms)\n", ms);
  }

  void service() {
    if (relayPulseEndMs > 0 && millis() >= relayPulseEndMs) {
      digitalWrite(RELAY_SHOT_PIN, HIGH); // Release
      relayPulseEndMs = 0;
    }
  }

} // namespace RelayDriver
