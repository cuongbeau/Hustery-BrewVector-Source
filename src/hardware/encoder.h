// File: src/hardware/encoder.h
// Refactor: Tách từ encoder.h gốc — chỉ giữ public API
// Implementation nằm trong encoder.cpp

#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

namespace Enc {
  void begin();
  int  readSteps(bool accelerate = true);
} // namespace Enc

#endif // ENCODER_H
