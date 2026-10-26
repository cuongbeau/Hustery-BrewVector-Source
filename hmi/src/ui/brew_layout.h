#pragma once
#include "ui/ready_layout.h"

namespace BrewLayout {
constexpr ReadyLayout::Rect Plot{24, 80, 268, 101};
constexpr ReadyLayout::Rect Stop{244, 29, 68, 30};
constexpr int FooterY = 198;
constexpr float LeftMax = 10.0f;
constexpr float TempMin = 75.0f;
constexpr float TempMax = 100.0f;
constexpr float normalized(float value, bool temperature) {
  return temperature ? (value - TempMin) / (TempMax - TempMin) : value / LeftMax;
}
}
