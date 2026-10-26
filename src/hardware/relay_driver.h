// File: src/hardware/relay_driver.h
// Tách từ control_logic.h — relay pulse logic
// Refactor: 2026-04-20

#ifndef RELAY_DRIVER_H
#define RELAY_DRIVER_H

#include <Arduino.h>

namespace RelayDriver {
  void init();
  void pulse(uint32_t ms = 500);
  void service();  // Check pulse end, deactivate relay
}

#endif // RELAY_DRIVER_H
