#pragma once

#include <LovyanGFX.hpp>

#include "ui/theme.h"

namespace UiComponents {

enum class ChipTone : uint8_t { Neutral, Info, Good, Warning, Danger };

void textLeft(lgfx::LGFX_Sprite& c, const char* text, int x, int y,
              const lgfx::IFont* font, uint16_t color,
              uint16_t background = UiTheme::BG);
void textCenter(lgfx::LGFX_Sprite& c, const char* text, int x, int y,
                const lgfx::IFont* font, uint16_t color,
                uint16_t background = UiTheme::BG);
void textRight(lgfx::LGFX_Sprite& c, const char* text, int x, int y,
               const lgfx::IFont* font, uint16_t color,
               uint16_t background = UiTheme::BG);

void panel(lgfx::LGFX_Sprite& c, int x, int y, int w, int h,
           uint16_t fill = UiTheme::CARD, uint16_t border = UiTheme::BORDER,
           int radius = UiTheme::RADIUS);
void header(lgfx::LGFX_Sprite& c, const char* title, bool showBack = true,
            const char* owner = nullptr);
void button(lgfx::LGFX_Sprite& c, int x, int y, int w, int h,
            const char* label, uint16_t accent, bool filled = false);
void row(lgfx::LGFX_Sprite& c, int x, int y, int w, int h,
         const char* label, const char* value = nullptr,
         bool chevron = true, const char* owner = nullptr,
         uint16_t valueColor = UiTheme::TEXT);
void toggle(lgfx::LGFX_Sprite& c, int x, int y, bool on);
void slider(lgfx::LGFX_Sprite& c, int x, int y, int w, uint8_t percent,
            uint16_t accent = UiTheme::CYAN);
void chip(lgfx::LGFX_Sprite& c, int x, int y, const char* label,
          ChipTone tone);
void backArrow(lgfx::LGFX_Sprite& c, int x = 18, int y = 17);
void chevron(lgfx::LGFX_Sprite& c, int x, int y,
             uint16_t color = UiTheme::MUTED);

}  // namespace UiComponents
