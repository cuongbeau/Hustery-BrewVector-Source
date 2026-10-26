#pragma once

#include <Arduino.h>

namespace UiTheme {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) |
                               ((g & 0xFC) << 3) | (b >> 3));
}

// Pure OLED Black & High-Contrast Instrument Palette
constexpr uint16_t BG = 0x0000;                     // Pure Black background
constexpr uint16_t CARD = 0x0000;                   // Pure Black card interior
constexpr uint16_t CARD_ALT = rgb565(20, 26, 34);   // Elevated button / tile background
constexpr uint16_t BORDER = rgb565(45, 68, 92);     // Crisp card border
constexpr uint16_t GRID = rgb565(28, 38, 48);       // Grid lines
constexpr uint16_t TEXT = 0xFFFF;                   // Pure white text
constexpr uint16_t MUTED = rgb565(140, 160, 180);   // Secondary text
constexpr uint16_t DIM = rgb565(65, 78, 92);        // Inactive text
constexpr uint16_t CYAN = rgb565(0, 225, 255);      // Electric Cyan
constexpr uint16_t CYAN_DARK = rgb565(0, 95, 120);  // Dark cyan
constexpr uint16_t AMBER = rgb565(255, 180, 0);     // Amber Gold
constexpr uint16_t AMBER_DARK = rgb565(160, 110, 0);// Dark amber
constexpr uint16_t ORANGE_RED = rgb565(255, 75, 15);// Temperature Orange-Red
constexpr uint16_t GREEN = rgb565(0, 230, 115);     // Emerald Green
constexpr uint16_t RED = rgb565(255, 45, 45);       // Warning Red


constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;
constexpr int MARGIN = 7;
constexpr int GAP = 6;
constexpr int RADIUS = 6;
constexpr int HEADER_H = 35;
constexpr int TOUCH_ROW_H = 39;

}  // namespace UiTheme
