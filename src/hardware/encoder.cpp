// File: src/hardware/encoder.cpp
// Version: 2025-11-22_01
// Refactor: Tách implementation từ encoder.h gốc
// KHÔNG sửa logic acceleration hay debounce

#include "encoder.h"
#include <ESP32Encoder.h>
#include "config/hardware_pins.h"

namespace Enc {

  static ESP32Encoder encoder;
  static int32_t lastCount = 0;

  // ===== Khởi tạo =====
  void begin() {
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(ENC_CLK, ENC_DT); 
    encoder.setFilter(1023);
    encoder.setCount(0);
    lastCount = 0;

    pinMode(ENC_SW,  INPUT_PULLUP);
  }

  // ===== Đọc số bước xoay (đã gồm Gia tốc) =====
  int readSteps(bool accelerate) {
    int32_t currentCount = encoder.getCount() / 2;
    int rawStep = currentCount - lastCount;

    if (rawStep == 0) return 0;
    lastCount = currentCount;

    static unsigned long lastRotateMs = 0;
    static int lastDir = 0;
    
    unsigned long now = millis();
    unsigned long dt = now - lastRotateMs;
    lastRotateMs = now;

    int currentDir = (rawStep > 0) ? 1 : -1;
    int finalStep = rawStep;

    if (currentDir == lastDir) {
      if (dt < 35) {
        finalStep = rawStep * 5; 
      } 
      else if (dt < 80) {
        finalStep = rawStep * 3; 
      }
    }
    lastDir = currentDir;

    return accelerate ? finalStep : rawStep;
  }
} // namespace Enc
