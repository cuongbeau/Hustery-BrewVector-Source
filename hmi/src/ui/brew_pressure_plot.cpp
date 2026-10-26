#include "ui/screen_manager.h"
#include "ui/brew_layout.h"
#include "ui/components.h"
#include "ui/theme.h"
#include <math.h>

using namespace UiComponents;

void ScreenManager::drawBrewPressurePlot(const ShotChart& data) {
  // Saved History retains its original pressure-only layout.
  const ReadyLayout::Rect plot{22, 57, 184, 115};
  float maximum = 10.0f;
  bool hasTarget = false;
  for (size_t i = 0; i < data.size(); ++i) {
    const auto& sample = data.at(i);
    if (isfinite(sample.pressureBar)) maximum = fmaxf(maximum, sample.pressureBar);
    if (isfinite(sample.targetPressureBar)) {
      maximum = fmaxf(maximum, sample.targetPressureBar);
      hasTarget = true;
    }
  }
  maximum = ceilf(maximum / 5.0f) * 5.0f;
  const float endTime = fmaxf(30.0f, ceilf(data.lastElapsedS() / 10.0f) * 10.0f);
  const auto xAt = [&](float seconds) {
    return plot.x + constrain(static_cast<int>(seconds / endTime * (plot.w - 1)), 0, plot.w - 1);
  };
  const auto yAt = [&](float pressure) {
    return plot.y + plot.h - 1 - constrain(
        static_cast<int>(pressure / maximum * (plot.h - 1)), 0, plot.h - 1);
  };

  canvas_.drawFastHLine(9, 41, 12, UiTheme::CYAN);
  textLeft(canvas_, "Pressure", 26, 41, &fonts::Font0, UiTheme::TEXT);
  if (hasTarget) {
    canvas_.drawFastHLine(111, 41, 4, UiTheme::MUTED);
    canvas_.drawFastHLine(119, 41, 4, UiTheme::MUTED);
    textLeft(canvas_, "Target", 128, 41, &fonts::Font0, UiTheme::MUTED);
  }
  for (int tick = 0; tick <= 2; ++tick) {
    const float pressure = tick * maximum / 2;
    const int y = yAt(pressure);
    for (int x = plot.x; x < plot.x + plot.w; x += 5)
      canvas_.drawFastHLine(x, y, min(2, plot.x + plot.w - x), UiTheme::GRID);
    char label[12];
    snprintf(label, sizeof(label), "%g", pressure);
    textRight(canvas_, label, plot.x - 4, y, &fonts::Font0, UiTheme::MUTED);
  }
  const float tickStep = endTime <= 30 ? 10 : 20;
  for (float seconds = 0; seconds < endTime; seconds += tickStep) {
    const int x = xAt(seconds);
    for (int y = plot.y; y < plot.y + plot.h; y += 5)
      canvas_.drawFastVLine(x, y, min(2, plot.y + plot.h - y), UiTheme::GRID);
    char label[12];
    snprintf(label, sizeof(label), "%.0f", seconds);
    textCenter(canvas_, label, x, 183, &fonts::Font0, UiTheme::MUTED);
  }
  textRight(canvas_, "s", plot.x + plot.w - 1, 183, &fonts::Font0, UiTheme::MUTED);

  // Dashed target first, then the actual trace. Draw every observation so
  // same-column min/max excursions are not smoothed away at 320px width.
  for (int target = 1; target >= 0; --target) {
    bool previousValid = false;
    int previousX = 0, previousY = 0;
    uint32_t previousTimestamp = 0;
    float previousElapsed = 0;
    for (size_t i = 0; i < data.size(); ++i) {
      const auto& sample = data.at(i);
      const float value = target ? sample.targetPressureBar : sample.pressureBar;
      if (!isfinite(sample.elapsedS) || !isfinite(value)) {
        previousValid = false;
        continue;
      }
      const int x = xAt(sample.elapsedS), y = yAt(value);
      if (previousValid && (sample.sourceTimestampMs - previousTimestamp > 1000U ||
                            sample.elapsedS - previousElapsed > 1.0f ||
                            sample.elapsedS < previousElapsed))
        previousValid = false; // Do not draw across a telemetry outage.
      if (previousValid) {
        if (target) {
          const int dx = x - previousX, dy = y - previousY;
          const int steps = max(1, max(abs(dx), abs(dy)));
          for (int k = 0; k <= steps; ++k) {
            const int px = previousX + dx * k / steps;
            const int py = previousY + dy * k / steps;
            if (((px - plot.x) / 4) % 2 == 0)
              canvas_.drawPixel(px, py, UiTheme::MUTED);
          }
        } else canvas_.drawLine(previousX, previousY, x, y, UiTheme::CYAN);
      } else canvas_.drawPixel(x, y, target ? UiTheme::MUTED : UiTheme::CYAN);
      previousX = x; previousY = y; previousValid = true;
      previousTimestamp = sample.sourceTimestampMs;
      previousElapsed = sample.elapsedS; // Saved CSV has synthetic source timestamps.
    }
  }
}

// Brew and frozen End Shot share curves, scales and gap handling.
void ScreenManager::drawBrewChart(const ShotChart& data, int offsetY) {
  auto p = BrewLayout::Plot;
  p.y += offsetY;
  const float endTime = fmaxf(30.0f, ceilf(data.lastElapsedS() / 10.0f) * 10.0f);
  const uint16_t colors[] = {UiTheme::MUTED, UiTheme::CYAN, UiTheme::AMBER, UiTheme::RED};
  const char* labels[] = {"Pressure", "Pump flow", "Temp est.", "Target"};
  const int legendX[] = {8, 88, 168, 250};
  const uint16_t legendColors[] = {UiTheme::CYAN, UiTheme::AMBER, UiTheme::RED, UiTheme::MUTED};
  for (int i = 0; i < 4; ++i) {
    if (i == 3) {
      canvas_.drawFastHLine(legendX[i], 66 + offsetY, 3, legendColors[i]);
      canvas_.drawFastHLine(legendX[i] + 6, 66 + offsetY, 3, legendColors[i]);
    } else canvas_.drawFastHLine(legendX[i], 66 + offsetY, 9, legendColors[i]);
    textLeft(canvas_, labels[i], legendX[i] + 12, 66 + offsetY, &fonts::Font0, legendColors[i]);
  }
  for (int i = 0; i <= 5; ++i) {
    const int y = p.y + p.h - 1 - i * (p.h - 1) / 5;
    for (int x = p.x; x < p.x + p.w; x += 6)
      canvas_.drawFastHLine(x, y, min(2, p.x + p.w - x), UiTheme::GRID);
    char label[8];
    snprintf(label, sizeof(label), "%d", i * 2);
    textRight(canvas_, label, p.x - 4, y, &fonts::Font0, UiTheme::MUTED);
    snprintf(label, sizeof(label), "%d", 75 + i * 5);
    textLeft(canvas_, label, p.x + p.w + 3, y, &fonts::Font0, UiTheme::RED);
  }
  for (float t = 0; t <= endTime; t += endTime <= 40 ? 10 : 20) {
    const int x = p.x + static_cast<int>(t / endTime * (p.w - 1));
    for (int y = p.y; y < p.y + p.h; y += 6) canvas_.drawPixel(x, y, UiTheme::GRID);
    char label[8]; snprintf(label, sizeof(label), "%.0f", t);
    textCenter(canvas_, label, x, 190 + offsetY, &fonts::Font0, UiTheme::MUTED);
  }
  textRight(canvas_, "s", 319, 190 + offsetY, &fonts::Font0, UiTheme::MUTED);
  // Fixed scales: leave gaps outside range rather than fabricate a flat limit.
  // Preserve every observation and break across missing telemetry.
  for (int channel = 0; channel < 4; ++channel) {
    bool previous = false;
    int px = 0, py = 0;
    uint32_t timestamp = 0;
    float elapsed = 0;
    for (size_t i = 0; i < data.size(); ++i) {
      const auto& s = data.at(i);
      const float target = isfinite(s.targetPressureBar) ? s.targetPressureBar :
                           s.targetFlowGps > 0.0f ? s.targetFlowGps : NAN;
      const float v = channel == 0 ? target : channel == 1 ? s.pressureBar :
                      channel == 2 ? s.flowGps : s.brewTempC;
      const float n = BrewLayout::normalized(v, channel == 3);
      if (!isfinite(s.elapsedS) || !isfinite(n) || n < 0 || n > 1) { previous = false; continue; }
      const int x = p.x + constrain(static_cast<int>(s.elapsedS / endTime * (p.w - 1)), 0, p.w - 1);
      const int y = p.y + p.h - 1 - static_cast<int>(n * (p.h - 1));
      if (previous && (s.sourceTimestampMs - timestamp > 1000U || s.elapsedS - elapsed > 1.0f || s.elapsedS < elapsed)) previous = false;
      if (previous) {
        if (channel == 0) {
          const int steps = max(1, max(abs(x-px), abs(y-py)));
          for (int k=0;k<=steps;++k) {
            const int dx=px+(x-px)*k/steps, dy=py+(y-py)*k/steps;
            if (((dx-p.x)/4)%2==0) canvas_.drawPixel(dx,dy,colors[channel]);
          }
        } else canvas_.drawLine(px,py,x,y,colors[channel]);
      } else canvas_.drawPixel(x,y,colors[channel]);
      previous=true;px=x;py=y;timestamp=s.sourceTimestampMs;elapsed=s.elapsedS;
    }
  }
}
