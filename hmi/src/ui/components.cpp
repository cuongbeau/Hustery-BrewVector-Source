#include "ui/components.h"

namespace UiComponents {

void textLeft(lgfx::LGFX_Sprite& c, const char* text, int x, int y,
              const lgfx::IFont* font, uint16_t color, uint16_t background) {
  c.setFont(font);
  c.setTextDatum(middle_left);
  c.setTextColor(color, background);
  c.drawString(text, x, y);
}

void textCenter(lgfx::LGFX_Sprite& c, const char* text, int x, int y,
                const lgfx::IFont* font, uint16_t color,
                uint16_t background) {
  c.setFont(font);
  c.setTextDatum(middle_center);
  c.setTextColor(color, background);
  c.drawString(text, x, y);
}

void textRight(lgfx::LGFX_Sprite& c, const char* text, int x, int y,
               const lgfx::IFont* font, uint16_t color,
               uint16_t background) {
  c.setFont(font);
  c.setTextDatum(middle_right);
  c.setTextColor(color, background);
  c.drawString(text, x, y);
}

void panel(lgfx::LGFX_Sprite& c, int x, int y, int w, int h, uint16_t fill,
           uint16_t border, int radius) {
  c.fillRoundRect(x, y, w, h, radius, fill);
  c.drawRoundRect(x, y, w, h, radius, border);
}

void backArrow(lgfx::LGFX_Sprite& c, int x, int y) {
  c.drawLine(x + 6, y - 7, x - 1, y, UiTheme::TEXT);
  c.drawLine(x - 1, y, x + 6, y + 7, UiTheme::TEXT);
  c.drawFastHLine(x, y, 15, UiTheme::TEXT);
}

void header(lgfx::LGFX_Sprite& c, const char* title, bool showBack,
            const char* owner) {
  c.fillRect(0, 0, UiTheme::SCREEN_W, UiTheme::HEADER_H, UiTheme::BG);
  c.drawFastHLine(0, UiTheme::HEADER_H - 1, UiTheme::SCREEN_W,
                  UiTheme::BORDER);
  if (showBack) backArrow(c);
  textCenter(c, title, 160, 17, &fonts::FreeSansBold9pt7b, UiTheme::TEXT, UiTheme::BG);
  if (owner != nullptr) {
    const int w = 48;
    panel(c, 266, 6, w, 22, UiTheme::BG, UiTheme::CYAN, 3);
    textCenter(c, owner, 290, 17, &fonts::Font2, UiTheme::CYAN, UiTheme::BG);
  }
}

void button(lgfx::LGFX_Sprite& c, int x, int y, int w, int h,
            const char* label, uint16_t accent, bool filled) {
  const uint16_t fill = filled ? accent : UiTheme::CARD_ALT;
  const uint16_t color = filled && accent != UiTheme::AMBER
                             ? UiTheme::TEXT
                             : (filled ? UiTheme::BG : accent);
  panel(c, x, y, w, h, fill, accent, UiTheme::RADIUS);
  textCenter(c, label, x + w / 2, y + h / 2, &fonts::Font2, color, fill);
}

void chevron(lgfx::LGFX_Sprite& c, int x, int y, uint16_t color) {
  c.drawLine(x - 3, y - 5, x + 2, y, color);
  c.drawLine(x + 2, y, x - 3, y + 5, color);
}

void chip(lgfx::LGFX_Sprite& c, int x, int y, const char* label,
          ChipTone tone) {
  uint16_t color = UiTheme::MUTED;
  if (tone == ChipTone::Info) color = UiTheme::CYAN;
  if (tone == ChipTone::Good) color = UiTheme::GREEN;
  if (tone == ChipTone::Warning) color = UiTheme::AMBER;
  if (tone == ChipTone::Danger) color = UiTheme::RED;
  const int w = 14 + c.textWidth(label);
  c.fillRoundRect(x, y - 9, w, 18, 3, UiTheme::BG);
  c.drawRoundRect(x, y - 9, w, 18, 3, color);
  textCenter(c, label, x + w / 2, y, &fonts::Font2, color, UiTheme::BG);
}

void row(lgfx::LGFX_Sprite& c, int x, int y, int w, int h,
         const char* label, const char* value, bool showChevron,
         const char* owner, uint16_t valueColor) {
  panel(c, x, y, w, h, UiTheme::CARD_ALT, UiTheme::BORDER, 4);
  textLeft(c, label, x + 10, y + h / 2, &fonts::Font2, UiTheme::TEXT,
           UiTheme::CARD_ALT);
  int right = x + w - 10;
  if (showChevron) {
    chevron(c, right - 2, y + h / 2);
    right -= 16;
  }
  if (owner != nullptr) {
    const int ownerW = 46;
    panel(c, right - ownerW, y + h / 2 - 10, ownerW, 20, UiTheme::BG,
          UiTheme::CYAN, 3);
    textCenter(c, owner, right - ownerW / 2, y + h / 2, &fonts::Font2,
               UiTheme::CYAN, UiTheme::BG);
    right -= ownerW + 8;
  }
  if (value != nullptr) {
    textRight(c, value, right, y + h / 2, &fonts::Font2, valueColor,
              UiTheme::CARD_ALT);
  }
}

void toggle(lgfx::LGFX_Sprite& c, int x, int y, bool on) {

  const uint16_t color = on ? UiTheme::CYAN : UiTheme::DIM;
  c.fillRoundRect(x, y, 48, 24, 12, color);
  c.fillCircle(x + (on ? 36 : 12), y + 12, 9, UiTheme::TEXT);
  textRight(c, on ? "ON" : "OFF", x - 5, y + 12, &fonts::Font0, color,
            UiTheme::CARD_ALT);
}

void slider(lgfx::LGFX_Sprite& c, int x, int y, int w, uint8_t percent,
            uint16_t accent) {
  const int fill = constrain(static_cast<int>(w * percent / 100), 0, w);
  c.fillRoundRect(x, y - 3, w, 6, 3, UiTheme::BORDER);
  if (fill > 0) c.fillRoundRect(x, y - 3, fill, 6, 3, accent);
  c.fillCircle(x + fill, y, 7, UiTheme::TEXT);
}

}  // namespace UiComponents
