#pragma once

#include <Arduino.h>

struct TouchPoint {
  int16_t x = 0;
  int16_t y = 0;
  bool pressed = false;
};

class TouchController {
 public:
  bool begin();
  bool read(TouchPoint& point);
  uint32_t errors() const { return errors_; }

 private:
  TouchPoint cached_;
  bool sampled_ = false;
  uint32_t sampledAt_ = 0, errors_ = 0;
  bool sample(TouchPoint& point);
  bool readRegisters(uint8_t reg, uint8_t* data, size_t length);
};
