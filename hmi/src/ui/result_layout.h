#pragma once
#include "ui/ready_layout.h"
namespace ResultLayout {
constexpr ReadyLayout::Rect History{8, 199, 148, 35};
constexpr ReadyLayout::Rect Home{164, 199, 148, 35};
constexpr ReadyLayout::Rect Back{8, 199, 304, 35};
constexpr ReadyLayout::Rect Prev{8, 216, 90, 24};
constexpr ReadyLayout::Rect Next{222, 216, 90, 24};
constexpr int Rows = 4;
constexpr int RowY = 38;
constexpr int RowStep = 44;
constexpr int RowHeight = 40;
constexpr int rowAt(int x, int y) {
  return x < 8 || x >= 312 || y < RowY || y >= RowY + Rows * RowStep ||
      (y - RowY) % RowStep >= RowHeight ? -1 : (y - RowY) / RowStep;
}
}
