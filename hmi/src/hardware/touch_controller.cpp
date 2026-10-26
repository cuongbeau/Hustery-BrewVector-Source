#include "hardware/touch_controller.h"

#include <Wire.h>

#include "board_pins.h"

namespace {
constexpr uint8_t REG_TOUCH_COUNT = 0x02;
constexpr uint8_t REG_FIRST_POINT = 0x03;
}

bool TouchController::begin() {
  pinMode(BoardPins::TOUCH_RST, OUTPUT);
  pinMode(BoardPins::TOUCH_INT, INPUT_PULLUP);

  digitalWrite(BoardPins::TOUCH_RST, LOW);
  delay(12);
  digitalWrite(BoardPins::TOUCH_RST, HIGH);
  delay(120);

  Wire.begin(BoardPins::TOUCH_SDA, BoardPins::TOUCH_SCL, 400000);
  Wire.setTimeOut(20);

  uint8_t count = 0;
  return readRegisters(REG_TOUCH_COUNT, &count, 1);
}

bool TouchController::read(TouchPoint& point) {
  const uint32_t now = millis();
  if (sampled_ && now - sampledAt_ < 10) { point = cached_; return point.pressed; }
  sampled_ = true; sampledAt_ = now;
  const bool pressed = sample(point);
  cached_ = point;
  return pressed;
}

bool TouchController::sample(TouchPoint& point) {
  point.pressed = false;

  uint8_t count = 0;
  if (!readRegisters(REG_TOUCH_COUNT, &count, 1) || (count & 0x0F) == 0) {
    return false;
  }

  uint8_t data[4] = {};
  if (!readRegisters(REG_FIRST_POINT, data, sizeof(data))) {
    return false;
  }

  const int32_t rawX = ((data[0] & 0x0F) << 8) | data[1];
  const int32_t rawY = ((data[2] & 0x0F) << 8) | data[3];

  const int32_t mappedX = 239 - rawX;
  const int32_t mappedY = 319 - rawY;
  const int32_t landscapeX = mappedY;
  const int32_t landscapeY = 239 - mappedX;

  // Display rotation 3 is the 180-degree counterpart of the original
  // landscape rotation 1, so rotate the calibrated touch coordinates too.
  point.x = constrain(319 - landscapeX, 0, 319);
  point.y = constrain(239 - landscapeY, 0, 239);
  point.pressed = true;
  return true;
}

bool TouchController::readRegisters(uint8_t reg, uint8_t* data, size_t length) {
  Wire.beginTransmission(BoardPins::TOUCH_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    ++errors_;
    return false;
  }

  const size_t received = Wire.requestFrom(BoardPins::TOUCH_ADDRESS, length);
  if (received != length) {
    ++errors_;
    while (Wire.available()) Wire.read();
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    data[i] = Wire.read();
  }
  return true;
}
