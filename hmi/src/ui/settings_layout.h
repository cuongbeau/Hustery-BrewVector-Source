#pragma once
#include "ui/ready_layout.h"

// Shared visible/touch bounds for the compact 320x240 settings frame.
namespace SettingsLayout {
using ReadyLayout::Rect;
constexpr Rect Tiles[] = {{8, 43, 148, 89}, {164, 43, 148, 89},
                          {8, 140, 148, 89}, {164, 140, 148, 89}};
constexpr Rect Connections[] = {{8, 44, 304, 53}, {8, 106, 304, 53},
                                {8, 168, 304, 53}};
constexpr Rect SystemRows[] = {{8, 84, 304, 34}, {8, 123, 304, 34},
                               {8, 162, 304, 34}, {8, 201, 304, 34}};
constexpr Rect WebWifi{8, 198, 304, 34};
inline int tileAt(int x, int y) {
  for (int i = 0; i < 4; ++i) if (Tiles[i].contains(x, y)) return i;
  return -1;
}
inline int connectionAt(int x, int y) {
  for (int i = 0; i < 3; ++i) if (Connections[i].contains(x, y)) return i;
  return -1;
}
inline int systemAt(int x, int y) {
  for (int i = 0; i < 4; ++i) if (SystemRows[i].contains(x, y)) return i;
  return -1;
}
}
