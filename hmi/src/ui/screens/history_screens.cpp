#include "ui/screen_manager.h"

#include <math.h>
#include <stdio.h>

#include "ui/components.h"
#include "ui/theme.h"
#include "ui/result_layout.h"
#include <time.h>

using namespace UiComponents;
namespace {
void dashedLine(lgfx::LGFX_Sprite& c, int x0, int y0, int x1, int y1,
                uint16_t color, int dash = 3, int gap = 3) {
  const int dx = x1 - x0;
  const int dy = y1 - y0;
  const int steps = max(abs(dx), abs(dy));
  if (steps == 0) {
    c.drawPixel(x0, y0, color);
    return;
  }
  const int period = dash + gap;
  for (int i = 0; i <= steps; ++i) {
    if ((i % period) >= dash) continue;
    const int px = x0 + static_cast<int32_t>(dx) * i / steps;
    const int py = y0 + static_cast<int32_t>(dy) * i / steps;
    c.drawPixel(px, py, color);
  }
}

float chartTimeCeiling(float seconds) {
  if (seconds <= 30.0f) return 30.0f;
  const float quantum = seconds <= 60.0f ? 15.0f : 30.0f;
  return ceilf(seconds / quantum) * quantum;
}
}  // namespace
// End Shot uses the frozen Brew curves; saved History keeps its pressure view.
void ScreenManager::drawResultSummary(const ShotSummary* result, const ShotChart& data,
                                      const UiTelemetry& t, bool live) {
  char title[40];
  snprintf(title, sizeof(title), "%s", result ? result->profileName : "SHOT RESULT");
  canvas_.setFont(&fonts::Font2);
  size_t n = strlen(title);
  while (n && canvas_.textWidth(title) > 224) {
    --n;
    while (n && (static_cast<uint8_t>(title[n]) & 0xC0) == 0x80) --n;
    title[n] = 0;
  }
  textLeft(canvas_, title, 8, 10, &fonts::Font2, UiTheme::TEXT);
  textRight(canvas_, result && (result->resultFlags & 1U) ? "AT STOP" : "PARTIAL",
            312, 10, &fonts::Font0, UiTheme::AMBER);
  char info[48];
  if (live) {
    if (t.scaleConnected && isfinite(t.weightG))
      snprintf(info, sizeof(info), "Scale now %.1f g - includes drips", t.weightG);
    else snprintf(info, sizeof(info), "Scale now -- g");
  } else if (result) {
    snprintf(info, sizeof(info), "Shot #%lu - saved record", static_cast<unsigned long>(result->id));
  } else snprintf(info, sizeof(info), "Result unavailable");
  textLeft(canvas_, info, 8, 25, &fonts::Font0, UiTheme::MUTED);
  canvas_.drawFastHLine(0, 32, 320, UiTheme::BORDER);
  if (live) {
    drawBrewChart(data, -28);
    if (!data.size()) textCenter(canvas_, "NO CHART", 160, 102, &fonts::Font2, UiTheme::MUTED);
    canvas_.drawFastHLine(8, 169, 304, UiTheme::BORDER);
  } else {
    canvas_.drawFastVLine(216, 36, 153, UiTheme::BORDER);
    drawBrewPressurePlot(data);
    if (!data.size()) textCenter(canvas_, "NO CHART", 115, 113, &fonts::Font2, UiTheme::MUTED);
  }

  const bool scaleYield = result && hasStopYield(*result);
  const bool estimatedYield = live && result && !scaleYield && hasEstimatedYield(*result);
  const char* labels[] = {"TIME", scaleYield ? "SCALE YIELD" :
                         estimatedYield ? "EST. YIELD" : "YIELD AT STOP", "PEAK"};
  const char* units[] = {"s", "g", "bar"};
  const float values[] = {result ? result->durationS : NAN,
                         scaleYield ? result->weightG :
                         estimatedYield ? result->modelYieldG : NAN,
                         result && result->sampleCount ? result->peakPressureBar : NAN};
  for (int i = 0; i < 3; ++i) {
    if (live) {
      const int center = 53 + i * 107;
      textCenter(canvas_, labels[i], center, 176, &fonts::Font0, UiTheme::MUTED);
      char value[24];
      if (isfinite(values[i])) snprintf(value, sizeof(value), "%.1f %s", values[i], units[i]);
      else snprintf(value, sizeof(value), "-- %s", units[i]);
      textCenter(canvas_, value, center, 190, &fonts::Font2, UiTheme::TEXT);
      continue;
    }
    const int y = 47 + i * 49;
    textLeft(canvas_, labels[i], 224, y, &fonts::Font0, UiTheme::MUTED);
    char value[20];
    if (isfinite(values[i])) snprintf(value, sizeof(value), "%.1f", values[i]);
    else snprintf(value, sizeof(value), "--");
    canvas_.setFont(&fonts::Font4);
    const lgfx::IFont* font = canvas_.textWidth(value) <= 61
        ? static_cast<const lgfx::IFont*>(&fonts::Font4)
        : static_cast<const lgfx::IFont*>(&fonts::Font2);
    textRight(canvas_, value, 282, y + 20, font, UiTheme::TEXT);
    textLeft(canvas_, units[i], 287, y + 24, &fonts::Font0, UiTheme::MUTED);
  }
  if (live) {
    const auto& h = ResultLayout::History;
    const auto& home = ResultLayout::Home;
    button(canvas_, h.x, h.y, h.w, h.h, "HISTORY", UiTheme::CYAN, false);
    const char* homeLabel = t.commandAction == UiAction::DismissResult && t.commandStatus
        ? t.commandStatus : "HOME";
    button(canvas_, home.x, home.y, home.w, home.h, homeLabel, UiTheme::TEXT, false);
  } else {
    const auto& back = ResultLayout::Back;
    button(canvas_, back.x, back.y, back.w, back.h, "BACK TO HISTORY", UiTheme::TEXT, false);
  }
}

void ScreenManager::drawHistory(const UiTelemetry& t) {
  header(canvas_, "SHOT HISTORY", true);
  if (!t.storageReady || !t.recentShots || !t.recentShotCount) {
    textCenter(canvas_, !t.storageReady ? "CARD NOT READY" : "NO SAVED SHOTS",
               160, 108, &fonts::Font4, UiTheme::MUTED);
    return;
  }
  const uint8_t pages = (t.recentShotCount + 3) / 4;
  if (historyPage_ >= pages) historyPage_ = pages - 1;
  for (uint8_t row = 0; row < 4; ++row) {
    const uint8_t index = historyPage_ * 4 + row;
    if (index >= t.recentShotCount) break;
    const auto& shot = t.recentShots[index];
    const int y = ResultLayout::RowY + row * ResultLayout::RowStep;
    panel(canvas_, 8, y, 304, ResultLayout::RowHeight, UiTheme::CARD_ALT, UiTheme::BORDER);
    char title[40];
    snprintf(title, sizeof(title), "#%lu %s", static_cast<unsigned long>(shot.id), shot.profileName);
    canvas_.setFont(&fonts::Font2);
    size_t n = strlen(title);
    while (n && canvas_.textWidth(title) > 286) {
      --n;
      while (n && (static_cast<uint8_t>(title[n]) & 0xC0) == 0x80) --n;
      title[n] = 0;
    }
    textLeft(canvas_, title, 16, y + 11, &fonts::Font2, UiTheme::TEXT, UiTheme::CARD_ALT);
    char date[24] = "Time unknown";
    if (shot.startedAtEpoch) {
      time_t wall = shot.startedAtEpoch;
      struct tm tm{};
      localtime_r(&wall, &tm);
      strftime(date, sizeof(date), "%d/%m %H:%M", &tm);
    }
    textLeft(canvas_, date, 16, y + 29, &fonts::Font0, UiTheme::MUTED, UiTheme::CARD_ALT);
    char stats[36];
    if (hasStopYield(shot)) snprintf(stats, sizeof(stats), "%.1f s / %.1f g", shot.durationS, shot.weightG);
    else snprintf(stats, sizeof(stats), "%.1f s / -- g", shot.durationS);
    textRight(canvas_, stats, 302, y + 29, &fonts::Font0, UiTheme::CYAN, UiTheme::CARD_ALT);
  }
  char page[16];
  snprintf(page, sizeof(page), "%u / %u", historyPage_ + 1, pages);
  textCenter(canvas_, page, 160, 228, &fonts::Font2, UiTheme::MUTED);
  textLeft(canvas_, historyPage_ ? "< PREV" : "", 12, 228, &fonts::Font2, UiTheme::CYAN);
  textRight(canvas_, historyPage_ + 1 < pages ? "NEXT >" : "", 308, 228, &fonts::Font2, UiTheme::CYAN);
}

void ScreenManager::drawShotDetail(const UiTelemetry& t) {
  drawResultSummary(t.selectedShot, historyChart_, t, false);
}

void ScreenManager::drawSavedCompare(const UiTelemetry& t) {
  header(canvas_, "SAVED COMPARE", true, "TF");
  if (!t.selectedShot || !t.comparisonShot) {
    textCenter(canvas_, "NEED TWO SAVED SHOTS", 160, 116, &fonts::Font4,
               UiTheme::AMBER);
    return;
  }
  textCenter(canvas_, "SELECTED", 122, 48, &fonts::Font2, UiTheme::CYAN);
  textCenter(canvas_, "REFERENCE", 255, 48, &fonts::Font2, UiTheme::MUTED);
  const char* labels[] = {"SHOT", "TIME", "WEIGHT", "PEAK", "SAMPLES"};
  char selected[5][20];
  char reference[5][20];
  snprintf(selected[0], sizeof(selected[0]), "#%lu",
           static_cast<unsigned long>(t.selectedShot->id));
  snprintf(reference[0], sizeof(reference[0]), "#%lu",
           static_cast<unsigned long>(t.comparisonShot->id));
  snprintf(selected[1], sizeof(selected[1]), "%.1f s", t.selectedShot->durationS);
  snprintf(reference[1], sizeof(reference[1]), "%.1f s", t.comparisonShot->durationS);
  snprintf(selected[2], sizeof(selected[2]), "%.1f g", t.selectedShot->weightG);
  snprintf(reference[2], sizeof(reference[2]), "%.1f g", t.comparisonShot->weightG);
  snprintf(selected[3], sizeof(selected[3]), "%.1f bar", t.selectedShot->peakPressureBar);
  snprintf(reference[3], sizeof(reference[3]), "%.1f bar", t.comparisonShot->peakPressureBar);
  snprintf(selected[4], sizeof(selected[4]), "%u", t.selectedShot->sampleCount);
  snprintf(reference[4], sizeof(reference[4]), "%u", t.comparisonShot->sampleCount);
  for (int i = 0; i < 5; ++i) {
    const int rowY = 62 + i * 32;
    panel(canvas_, 7, rowY, 306, 28, UiTheme::CARD);
    textLeft(canvas_, labels[i], 15, rowY + 14, &fonts::Font0, UiTheme::MUTED,
             UiTheme::CARD);
    textRight(canvas_, selected[i], 181, rowY + 14, &fonts::Font2,
              UiTheme::CYAN, UiTheme::CARD);
    textRight(canvas_, reference[i], 304, rowY + 14, &fonts::Font2,
              UiTheme::TEXT, UiTheme::CARD);
  }
  textCenter(canvas_, "Reference is the adjacent older shot", 160, 229,
             &fonts::Font0, UiTheme::MUTED);
}

void ScreenManager::drawEraseHistoryConfirm() {
  header(canvas_, "ERASE HISTORY", true, "TF");
  panel(canvas_, 16, 58, 288, 106, UiTheme::CARD, UiTheme::RED);
  textCenter(canvas_, "DELETE ALL SAVED SHOTS?", 160, 91, &fonts::Font4,
             UiTheme::RED, UiTheme::CARD);
  textCenter(canvas_, "CSV files on TF cannot be recovered", 160, 126,
             &fonts::Font0, UiTheme::MUTED, UiTheme::CARD);
  button(canvas_, 7, 184, 148, 45, "CANCEL", UiTheme::TEXT, false);
  button(canvas_, 165, 184, 148, 45, "ERASE ALL", UiTheme::RED, true);
}

void ScreenManager::drawCompare(const UiTelemetry& t) {
  header(canvas_, "COMPARE", true, "TF");
  const ShotSummary* reference = nullptr;
  if (t.recentShots && t.recentShotCount > 0) {
    reference = &t.recentShots[0];
    if (t.recentShotCount > 1 &&
        fabsf(reference->durationS - chart_.lastElapsedS()) < 0.15f &&
        fabsf(reference->weightG - chart_.lastWeightG()) < 0.15f)
      reference = &t.recentShots[1];
  }
  if (!reference || chart_.size() < 2) {
    panel(canvas_, 16, 66, 288, 102, UiTheme::CARD);
    textCenter(canvas_, "NEED A PREVIOUS SHOT", 160, 103, &fonts::Font4,
               UiTheme::AMBER, UiTheme::CARD);
    textCenter(canvas_, "Save at least one earlier shot", 160, 133,
               &fonts::Font0, UiTheme::MUTED, UiTheme::CARD);
    return;
  }

  textCenter(canvas_, "CURRENT", 108, 48, &fonts::Font2, UiTheme::CYAN);
  textCenter(canvas_, "PREVIOUS", 250, 48, &fonts::Font2, UiTheme::MUTED);
  const char* labels[] = {"TIME", "WEIGHT", "PEAK", "SAMPLES"};
  char current[4][16];
  char previous[4][16];
  snprintf(current[0], sizeof(current[0]), "%.1f s", chart_.lastElapsedS());
  snprintf(previous[0], sizeof(previous[0]), "%.1f s", reference->durationS);
  snprintf(current[1], sizeof(current[1]), "%.1f g", chart_.lastWeightG());
  snprintf(previous[1], sizeof(previous[1]), "%.1f g", reference->weightG);
  snprintf(current[2], sizeof(current[2]), "%.1f bar", chart_.peakPressureBar());
  snprintf(previous[2], sizeof(previous[2]), "%.1f bar",
           reference->peakPressureBar);
  snprintf(current[3], sizeof(current[3]), "%u",
           static_cast<unsigned int>(chart_.size()));
  snprintf(previous[3], sizeof(previous[3]), "%u", reference->sampleCount);
  for (int i = 0; i < 4; ++i) {
    const int rowY = 65 + i * 39;
    panel(canvas_, 7, rowY, 306, 34, UiTheme::CARD);
    textLeft(canvas_, labels[i], 16, rowY + 17, &fonts::Font0,
             UiTheme::MUTED, UiTheme::CARD);
    textRight(canvas_, current[i], 171, rowY + 17, &fonts::Font2,
              UiTheme::CYAN, UiTheme::CARD);
    textRight(canvas_, previous[i], 302, rowY + 17, &fonts::Font2,
              UiTheme::TEXT, UiTheme::CARD);
  }
  textCenter(canvas_, reference->profileName, 160, 228, &fonts::Font0,
             UiTheme::MUTED);
}

void ScreenManager::drawChart(int x, int y, int w, int h,
                              const UiTelemetry& t, bool includeTarget) {
  (void)t;
  const ShotChart& renderedChart = renderHistoryChart_ ? historyChart_ : chart_;
  canvas_.fillRect(x, y, w, h, UiTheme::BG);
  constexpr int left = 24;
  constexpr int right = 22;
  constexpr int top = 15;
  constexpr int bottom = 21;
  const int plotX = x + left;
  const int plotY = y + top;
  const int plotW = w - left - right;
  const int plotH = h - top - bottom;

  float timeMinS = 0.0f;
  const float lastTimeS = renderedChart.lastElapsedS();
  float timeMaxS = chartTimeCeiling(lastTimeS);
  if (renderedChart.size() == renderedChart.capacity() &&
      renderedChart.firstElapsedS() > 0.0f) {
    timeMinS = floorf(renderedChart.firstElapsedS() / 30.0f) * 30.0f;
    if (timeMaxS - timeMinS < 30.0f) timeMaxS = timeMinS + 30.0f;
  }
  const float timeSpanS = max(1.0f, timeMaxS - timeMinS);

  for (int i = 0; i <= 4; ++i)
    canvas_.drawFastHLine(plotX, plotY + plotH * i / 4, plotW, UiTheme::GRID);
  for (int i = 0; i <= 6; ++i)
    canvas_.drawFastVLine(plotX + plotW * i / 6, plotY, plotH, UiTheme::GRID);
  canvas_.drawFastVLine(plotX, plotY, plotH, UiTheme::BORDER);
  canvas_.drawFastHLine(plotX, plotY + plotH, plotW, UiTheme::BORDER);
  textLeft(canvas_, "bar", x + 1, y + 5, &fonts::Font0, UiTheme::CYAN);
  textRight(canvas_, "ml/s", x + w - 1, y + 5, &fonts::Font0,
            UiTheme::AMBER);
  for (int i = 0; i <= 4; ++i) {
    char p[4];
    snprintf(p, sizeof(p), "%d", 12 - i * 3);
    textRight(canvas_, p, plotX - 3, plotY + plotH * i / 4, &fonts::Font0,
              UiTheme::CYAN);
  }
  for (int i = 0; i <= 3; ++i) {
    char f[4];
    snprintf(f, sizeof(f), "%d", 6 - i * 2);
    textLeft(canvas_, f, plotX + plotW + 3, plotY + plotH * i / 3,
             &fonts::Font0, UiTheme::AMBER);
  }
  for (int i = 0; i <= 6; ++i) {
    char sec[8];
    const float tick = timeMinS + timeSpanS * i / 6.0f;
    snprintf(sec, sizeof(sec), "%.0f", tick);
    textCenter(canvas_, sec, plotX + plotW * i / 6, plotY + plotH + 8,
               &fonts::Font0, UiTheme::MUTED);
  }
  textCenter(canvas_, "TIME (s)", plotX + plotW / 2, plotY + plotH + 17,
             &fonts::Font0, UiTheme::MUTED);

  const auto xTime = [&](float elapsedS) {
    const float ratio = (elapsedS - timeMinS) / timeSpanS;
    return plotX + constrain(static_cast<int>(ratio * (plotW - 1) + 0.5f),
                             0, plotW - 1);
  };
  const auto yPressure = [&](float v) {
    return plotY + plotH - constrain(static_cast<int>(v / 12.0f * plotH),
                                     0, plotH - 1);
  };
  const auto yFlow = [&](float v) {
    return plotY + plotH - constrain(static_cast<int>(v / 6.0f * plotH),
                                     0, plotH - 1);
  };

  // Step boundaries are rendered below the curves, once per transition.
  if (renderedChart.size() >= 2) {
    uint8_t previousStep = renderedChart.at(0).stepIndex;
    for (size_t i = 1; i < renderedChart.size(); ++i) {
      const ShotChartSample& sample = renderedChart.at(i);
      if (sample.stepIndex == previousStep) continue;
      const int markerX = xTime(sample.elapsedS);
      for (int sy = plotY; sy < plotY + plotH; sy += 7)
        canvas_.drawFastVLine(markerX, sy, 3, UiTheme::DIM);
      previousStep = sample.stepIndex;
    }
  }

  // Target curves are deliberately dashed and drawn first.
  if (includeTarget && renderedChart.size() >= 2) {
    bool hasPreviousPressureTarget = false;
    bool hasPreviousFlowTarget = false;
    int previousPressureX = 0, previousPressureY = 0;
    int previousFlowX = 0, previousFlowY = 0;
    for (size_t i = 0; i < renderedChart.size(); ++i) {
      const ShotChartSample& sample = renderedChart.at(i);
      const int sx = xTime(sample.elapsedS);
      if (sample.targetPressureBar > 0.01f) {
        const int sy = yPressure(sample.targetPressureBar);
        if (hasPreviousPressureTarget)
          dashedLine(canvas_, previousPressureX, previousPressureY, sx, sy,
                     UiTheme::CYAN_DARK);
        previousPressureX = sx;
        previousPressureY = sy;
        hasPreviousPressureTarget = true;
      } else {
        hasPreviousPressureTarget = false;
      }
      if (sample.targetFlowGps > 0.01f) {
        const int sy = yFlow(sample.targetFlowGps);
        if (hasPreviousFlowTarget)
          dashedLine(canvas_, previousFlowX, previousFlowY, sx, sy,
                     UiTheme::AMBER_DARK);
        previousFlowX = sx;
        previousFlowY = sy;
        hasPreviousFlowTarget = true;
      } else {
        hasPreviousFlowTarget = false;
      }
    }
  }

  // Actual data is decimated by screen column. Each bucket keeps min/max so
  // brief pressure or flow spikes remain visible even in long shots.
  int bucketX = -1;
  float minPressure = 0.0f, maxPressure = 0.0f, lastPressure = 0.0f;
  float minFlow = 0.0f, maxFlow = 0.0f, lastFlow = 0.0f;
  int previousX = -1, previousPressureY = 0, previousFlowY = 0;
  const auto flushBucket = [&]() {
    if (bucketX < 0) return;
    const int pressureY = yPressure(lastPressure);
    const int flowY = yFlow(lastFlow);
    if (previousX >= 0) {
      canvas_.drawLine(previousX, previousPressureY, bucketX, pressureY,
                       UiTheme::CYAN);
      canvas_.drawLine(previousX, previousFlowY, bucketX, flowY,
                       UiTheme::AMBER);
    }
    canvas_.drawFastVLine(bucketX, yPressure(maxPressure),
                          max(1, yPressure(minPressure) - yPressure(maxPressure) + 1),
                          UiTheme::CYAN);
    canvas_.drawFastVLine(bucketX, yFlow(maxFlow),
                          max(1, yFlow(minFlow) - yFlow(maxFlow) + 1),
                          UiTheme::AMBER);
    previousX = bucketX;
    previousPressureY = pressureY;
    previousFlowY = flowY;
  };

  for (size_t i = 0; i < renderedChart.size(); ++i) {
    const ShotChartSample& sample = renderedChart.at(i);
    const int sx = xTime(sample.elapsedS);
    if (sx != bucketX) {
      flushBucket();
      bucketX = sx;
      minPressure = maxPressure = lastPressure = sample.pressureBar;
      minFlow = maxFlow = lastFlow = sample.flowGps;
    } else {
      minPressure = min(minPressure, sample.pressureBar);
      maxPressure = max(maxPressure, sample.pressureBar);
      lastPressure = sample.pressureBar;
      minFlow = min(minFlow, sample.flowGps);
      maxFlow = max(maxFlow, sample.flowGps);
      lastFlow = sample.flowGps;
    }
  }
  flushBucket();

  if (renderedChart.size() > 0) {
    const int cursorX = xTime(renderedChart.lastElapsedS());
    for (int cy = plotY; cy < plotY + plotH; cy += 8)
      canvas_.drawFastVLine(cursorX, cy, 4, UiTheme::TEXT);
  }

  const int legendX = plotX + plotW - 67;
  canvas_.drawFastHLine(legendX, plotY + 4, 10, UiTheme::CYAN);
  textLeft(canvas_, "P", legendX + 12, plotY + 4, &fonts::Font0,
           UiTheme::MUTED);
  dashedLine(canvas_, legendX + 25, plotY + 4, legendX + 35, plotY + 4,
             UiTheme::CYAN_DARK, 2, 2);
  textLeft(canvas_, "T", legendX + 37, plotY + 4, &fonts::Font0,
           UiTheme::MUTED);
  canvas_.drawFastHLine(legendX, plotY + 12, 10, UiTheme::AMBER);
  textLeft(canvas_, "F", legendX + 12, plotY + 12, &fonts::Font0,
           UiTheme::MUTED);
  dashedLine(canvas_, legendX + 25, plotY + 12, legendX + 35, plotY + 12,
             UiTheme::AMBER_DARK, 2, 2);
  textLeft(canvas_, "T", legendX + 37, plotY + 12, &fonts::Font0,
           UiTheme::MUTED);
}
