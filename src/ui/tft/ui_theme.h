#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>

namespace TftTheme {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) |
                               ((g & 0xFC) << 3) | (b >> 3));
}

// Pure OLED Black & High-Contrast Instrument Palette
constexpr uint16_t BG = TFT_BLACK;                 // 0x0000 Pure Black background (Maximum contrast)
constexpr uint16_t CARD = TFT_BLACK;               // 0x0000 Pure Black card interior (100% seamless, no square patches)
constexpr uint16_t CARD_ALT = rgb565(22, 30, 40);   // 0x1185 Subtle arc track / elevated elements
constexpr uint16_t BORDER = rgb565(45, 68, 92);     // 0x2E2B Crisp card border

constexpr uint16_t GRID = rgb565(28, 38, 48);       // 0x1946 Grid lines
constexpr uint16_t TEXT = TFT_WHITE;                // 0xFFFF Pure crisp white
constexpr uint16_t MUTED = rgb565(140, 160, 180);   // 0x8D16 Slate gray secondary text
constexpr uint16_t DIM = rgb565(65, 78, 92);        // 0x426B Inactive text
constexpr uint16_t CYAN = rgb565(0, 225, 255);      // 0x071F Vivid Electric Cyan
constexpr uint16_t CYAN_DARK = rgb565(0, 95, 120);  // 0x030F Dark cyan
constexpr uint16_t AMBER = rgb565(255, 180, 0);     // 0xFDA0 Vivid Amber Gold (Flow)
constexpr uint16_t AMBER_DARK = rgb565(160, 110, 0);// 0xA360 Dark amber
constexpr uint16_t ORANGE_RED = rgb565(255, 75, 15);// 0xFA42 Vivid Orange-Red for Temperature
constexpr uint16_t TEMP = ORANGE_RED;               // Temperature color
constexpr uint16_t GREEN = rgb565(0, 230, 115);     // 0x072E Vivid Emerald Green (Ready/OK)
constexpr uint16_t RED = rgb565(255, 45, 45);       // 0xF965 High-vis Warning Red

// Compatibility aliases
constexpr uint16_t SURFACE = CARD;
constexpr uint16_t SURFACE_ALT = CARD_ALT;
constexpr uint16_t HEADER = CARD_ALT;
constexpr uint16_t OK = GREEN;
constexpr uint16_t TARGET = AMBER;
constexpr uint16_t MINT = CYAN;
constexpr uint16_t YELLOW = AMBER;

}  // namespace TftTheme


