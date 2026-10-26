#pragma once

// Display calibration only; never changes the heater sensor or PID target.
namespace ThermalCalibration {
constexpr float DefaultOffsetC = 21.0f;
constexpr float MinOffsetC = 0.0f;
constexpr float MaxOffsetC = 60.0f;
constexpr bool valid(float offset) {
  return offset >= MinOffsetC && offset <= MaxOffsetC;
}
constexpr float estimate(float blockC, float offsetC) {
  return blockC - offsetC;
}
}
