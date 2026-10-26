// File: src/ui/ui_common.h
// Shared TFT colors, fonts, text helpers, and chart helpers.

#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "tft_driver.h"
#include "interfaces/shot_types.h"
#include "ui_theme.h"

// ====== COLORS ======
static const uint16_t COL_BG = TftTheme::BG;
static const uint16_t COL_TEXT = TftTheme::TEXT;
static const uint16_t COL_BORDER = TftTheme::BORDER;
static const uint16_t COL_GRID = TftTheme::GRID;
static const uint16_t COL_GRID_MINOR = TftTheme::GRID;
static const uint16_t COL_AXIS_LABEL = TftTheme::MUTED;
static const uint16_t COL_FOOTER_LABEL = TftTheme::MUTED;

static const uint16_t COL_MINT = TftTheme::CYAN;
static const uint16_t COL_YELL = TftTheme::AMBER;
static const uint16_t COL_RED = TftTheme::RED;

static const uint16_t COL_PR = TftTheme::CYAN;
static const uint16_t COL_TMP = TftTheme::AMBER;
static const uint16_t COL_TGT_P = TftTheme::CYAN_DARK;
static const uint16_t COL_TGT_F = TftTheme::AMBER_DARK;
static const uint16_t COL_TGT_D = TftTheme::MUTED;


// ====== FONT SHORTCUTS ======
namespace F {
  inline void tiny() { tft.setFont(&fonts::Font0); tft.setTextSize(1); }
  inline void small() { tft.setFont(&fonts::FreeSans9pt7b); tft.setTextSize(1); }
  inline void smbold() { tft.setFont(&fonts::FreeSansBold9pt7b); tft.setTextSize(1); }
  inline void med() { tft.setFont(&fonts::FreeSans12pt7b); tft.setTextSize(1); }
  inline void medb() { tft.setFont(&fonts::FreeSansBold12pt7b); tft.setTextSize(1); }
  inline void big() { tft.setFont(&fonts::FreeSans18pt7b); tft.setTextSize(1); }
  inline void bigb() { tft.setFont(&fonts::FreeSansBold18pt7b); tft.setTextSize(1); }
}

// ====== TEXT HELPERS ======
inline void drawCentered(int cx, int y, const char* s, uint16_t col,
                         bool bold = false, LovyanGFX* dev = &tft) {
  dev->setTextColor(col, COL_BG);
  int w = dev->textWidth(s);
  int x = cx - w / 2;
  if (bold) {
    dev->setCursor(x + 1, y);
    dev->print(s);
  }
  dev->setCursor(x, y);
  dev->print(s);
}

template <typename... Args>
inline void printfCentered(int cx, int y, uint16_t col, bool bold,
                           LovyanGFX* dev, const char* fmt, Args... args) {
  char buf[64];
  snprintf(buf, sizeof(buf), fmt, args...);
  drawCentered(cx, y, buf, col, bold, dev);
}

template <typename... Args>
inline void printfCentered(int cx, int y, uint16_t col, bool bold,
                           const char* fmt, Args... args) {
  printfCentered(cx, y, col, bold, &tft, fmt, args...);
}

inline void drawFitted1Line(int x, int y, int maxW, const char* s, uint16_t fg,
                            uint16_t bg) {
  if (!s) return;
  tft.setTextColor(fg, bg);

  if (tft.textWidth(s) <= maxW) {
    tft.setCursor(x, y);
    tft.print(s);
    return;
  }

  const char* dots = "...";
  int dotsW = tft.textWidth(dots);

  char buf[96];
  strncpy(buf, s, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;

  int n = (int)strlen(buf);
  while (n > 0) {
    buf[n] = 0;
    if (tft.textWidth(buf) + dotsW <= maxW) break;
    n--;
  }

  tft.setCursor(x, y);
  tft.print(buf);
  tft.print(dots);
}

// ====== CHART STRUCT ======
struct Chart {
  int x = 24, y = 64, w = 420, h = 192;
  float pMax = 12.0f;
  float tMax = 120.0f;
  float timeMax = 45.0f;
  int pts = 0;
  int maxPts = 420;
  float tScale = 1.0f;
};

// Chart coordinate helpers
inline int cx(const Chart& c, float tsec) {
  return c.x + (int)(tsec * c.w / c.timeMax);
}

inline int cyP(const Chart& c, float bar) {
  return c.y + c.h - (int)(bar / c.pMax * c.h);
}

inline int cyT(const Chart& c, float temp) {
  return c.y + c.h - (int)(temp / c.tMax * c.h);
}

// Dashed line helper
inline void dashed(int x0, int y0, int x1, int y1, uint16_t col,
                   int dashLen = 6, int gap = 4, LovyanGFX* dev = &tft) {
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 1) return;
  float step = dashLen + gap;
  for (float d = 0; d < len; d += step) {
    float s0 = d / len;
    float s1 = fminf((d + dashLen) / len, 1.0f);
    dev->drawLine(x0 + dx * s0, y0 + dy * s0, x0 + dx * s1,
                  y0 + dy * s1, col);
  }
}

// Grid drawing
inline void drawGrid(const Chart& c, LovyanGFX* dev = &tft) {
  bool isSprite = (dev != &tft);
  int ox = isSprite ? 0 : c.x;
  int oy = isSprite ? 0 : c.y;

  dev->drawRect(ox, oy, c.w, c.h, COL_BORDER);

  // Horizontal grids on the shared 0-12 pressure/flow axis.
  for (int p = 1; p <= 11; ++p) {
    if ((p % 3) == 0) continue;
    int yy = cyP(c, (float)p) - (isSprite ? c.y : 0);
    dashed(ox, yy, ox + c.w, yy, COL_GRID_MINOR, 1, 8, dev);
  }

  for (int p = 3; p <= 9; p += 3) {
    int yy = cyP(c, (float)p) - (isSprite ? c.y : 0);
    dashed(ox, yy, ox + c.w, yy, COL_GRID, 2, 4, dev);
  }

  // Vertical grids every 5 seconds, scaled to the active time window.
  for (float t = 5.0f; t < c.timeMax; t += 5.0f) {
    int xx = (int)(t * c.w / c.timeMax);
    dashed(ox + xx, oy, ox + xx, oy + c.h, COL_GRID, 2, 4, dev);
  }

  // Labels sit outside the chart sprite, so they are always drawn to TFT.
  F::tiny();
  tft.setTextDatum(textdatum_t::middle_right);
  tft.setTextColor(COL_AXIS_LABEL, COL_BG);
  for (int p = 0; p <= 12; p += 3) {
    int yy = cyP(c, (float)p);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", p);
    tft.drawString(buf, c.x - 4, yy);
  }
  tft.drawString("P/F", c.x - 4, c.y - 8);

  tft.setTextDatum(textdatum_t::top_center);
  tft.setTextColor(COL_AXIS_LABEL, COL_BG);
  for (float t = 0.0f; t <= c.timeMax + 0.1f; t += 15.0f) {
    int xx = cx(c, t);
    char buf[8];
    if (t + 15.0f > c.timeMax) snprintf(buf, sizeof(buf), "%.0fs", t);
    else snprintf(buf, sizeof(buf), "%.0f", t);
    tft.drawString(buf, xx, c.y + c.h + 3);
  }

  tft.setTextDatum(textdatum_t::top_left);
}

#endif // UI_COMMON_H
