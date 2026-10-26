// File: src/hardware/valve_sense.h
// Tách từ control_logic.h — Brew sense + Hot sense debounce
// Refactor: 2026-04-20

#ifndef VALVE_SENSE_H
#define VALVE_SENSE_H

#include <Arduino.h>

// Cảm biến dòng/van
#define USE_BREW_SENSE     1
#define USE_HOTWATER_SENSE 1

enum class ValveEvent : uint8_t {
  NONE,
  BREW_START,
  BREW_STOP,
  HOT_START,
  HOT_STOP
};

namespace ValveSense {
  void init();
  ValveEvent update();
  bool isMachineValveActive();
}

#endif // VALVE_SENSE_H
