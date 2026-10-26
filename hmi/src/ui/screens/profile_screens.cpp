#include "ui/screen_manager.h"

#include <math.h>
#include <stdio.h>

#include "ui/components.h"
#include "ui/theme.h"

using namespace UiComponents;

void ScreenManager::drawProfiles(const UiTelemetry& t) {
  header(canvas_, "BREW PROFILES", true, "MAIN");
  if (!t.mainConnected) {
    panel(canvas_, 16, 70, 288, 92, UiTheme::CARD, UiTheme::RED);
    textCenter(canvas_, "MAIN OFFLINE", 160, 107, &fonts::Font4,
               UiTheme::RED, UiTheme::CARD);
    textCenter(canvas_, "Profile list is unavailable", 160, 137,
               &fonts::Font0, UiTheme::MUTED, UiTheme::CARD);
    return;
  }
  if (!t.profiles || t.profileCount == 0) {
    panel(canvas_, 16, 70, 288, 92, UiTheme::CARD);
    textCenter(canvas_, "LOADING PROFILES...", 160, 116, &fonts::Font2,
               UiTheme::CYAN, UiTheme::CARD);
    return;
  }

  const uint8_t pageCount = max<uint8_t>(1, (t.profileCount + 3) / 4);
  if (profilePage_ >= pageCount) profilePage_ = pageCount - 1;
  const uint8_t first = profilePage_ * 4;
  const uint8_t current = t.profiles[0].currentIndex;
  for (uint8_t rowIndex = 0; rowIndex < 4; ++rowIndex) {
    const uint8_t index = first + rowIndex;
    if (index >= t.profileCount) break;
    const auto& profile = t.profiles[index];
    char value[30];
    if (profile.flags & 2U)
      snprintf(value, sizeof(value), "DISABLED");
    else if (profile.flags & 1U)
      snprintf(value, sizeof(value), "MANUAL");
    else if (profile.targetWeightG > 0.1f)
      snprintf(value, sizeof(value), "%u steps / %.0fg", profile.stepCount,
               profile.targetWeightG);
    else
      snprintf(value, sizeof(value), "%u steps", profile.stepCount);
    row(canvas_, 7, 42 + rowIndex * 40, 306, 36, profile.name, value, false,
        index == current ? "ACTIVE" : nullptr,
        index == current ? UiTheme::CYAN : UiTheme::TEXT);
  }

  char page[20];
  snprintf(page, sizeof(page), "%u / %u", profilePage_ + 1, pageCount);
  textLeft(canvas_, "WEB EDIT", 10, 226, &fonts::Font2, UiTheme::GREEN);
  textRight(canvas_, profilePage_ > 0 ? "<" : "", 130, 226,
            &fonts::Font2, UiTheme::CYAN);
  textCenter(canvas_, page, 160, 226, &fonts::Font2, UiTheme::MUTED);
  textRight(canvas_, profilePage_ + 1 < pageCount ? ">" : "", 310,
            226, &fonts::Font2, UiTheme::CYAN);
}

namespace {
const char* profileModeName(uint8_t mode) {
  return mode == 0 ? "PRESSURE" : mode == 1 ? "FLOW" : "DUTY";
}

const char* profileModeUnit(uint8_t mode) {
  return mode == 0 ? "bar" : mode == 1 ? "ml/s" : "%";
}

void adjustRow(lgfx::LGFX_Sprite& canvas, int y, const char* label,
               const char* value, uint16_t valueColor = UiTheme::CYAN) {
  panel(canvas, 7, y, 306, 40, UiTheme::CARD);
  textLeft(canvas, label, 15, y + 20, &fonts::Font2, UiTheme::MUTED,
           UiTheme::CARD);
  textRight(canvas, value, 211, y + 20, &fonts::Font2, valueColor,
            UiTheme::CARD);
  button(canvas, 220, y + 5, 42, 30, "-", UiTheme::MUTED, false);
  button(canvas, 270, y + 5, 42, 30, "+", UiTheme::CYAN, false);
}

void editorTabs(lgfx::LGFX_Sprite& canvas, uint8_t active) {
  const char* labels[] = {"CONTROL", "SAFETY", "EXIT"};
  for (uint8_t i = 0; i < 3; ++i)
    button(canvas, 3 + i * 106, 212, 102, 25, labels[i],
           i == active ? UiTheme::CYAN : UiTheme::MUTED, i == active);
}
}  // namespace

void ScreenManager::drawProfileDetail(const UiTelemetry& t) {
  header(canvas_, "PROFILE DETAIL", true, "MAIN");
  if (!profileDraft_.valid()) {
    panel(canvas_, 16, 70, 288, 94, UiTheme::CARD);
    textCenter(canvas_, "LOADING FULL PROFILE...", 160, 107, &fonts::Font2,
               UiTheme::CYAN, UiTheme::CARD);
    const bool recentFailure = t.profileTransferStatusAgeMs < 2500 &&
        t.profileTransferStatus != HmiProtocol::ProfileTransferStatus::Pending &&
        t.profileTransferStatus != HmiProtocol::ProfileTransferStatus::Accepted;
    textCenter(canvas_, !t.mainConnected ? "MAIN OFFLINE"
                         : recentFailure ? "PROFILE TRANSFER REJECTED"
                                         : "Waiting for steps + checksum",
               160, 137, &fonts::Font0,
               (!t.mainConnected || recentFailure) ? UiTheme::RED
                                                    : UiTheme::MUTED,
               UiTheme::CARD);
    return;
  }
  const auto& p=profileDraft_.data().profile;
  textCenter(canvas_,p.name,160,75,&fonts::Font2,UiTheme::TEXT);
  char summary[48];snprintf(summary,sizeof(summary),"%u PHASES / MAX 60s",p.stepCount);
  textCenter(canvas_,summary,160,110,&fonts::Font2,UiTheme::CYAN);
  textCenter(canvas_,"Create and edit profiles on Web",160,142,&fonts::Font0,UiTheme::MUTED);
  button(canvas_,215,166,98,35,"ACTIVATE",UiTheme::GREEN,false);

}

void ScreenManager::drawProfileQuick() {
  char title[28];
  snprintf(title, sizeof(title), "QUICK EDIT  %u/2", quickPage_ + 1);
  header(canvas_, title, true, "DRAFT");
  if (!profileDraft_.valid()) return;
  const auto& p = profileDraft_.data().profile;
  char values[3][22];
  const char* labels[3];
  if (quickPage_ == 0) {
    labels[0] = "PRE-INFUSION";
    labels[1] = "BLOOM TIME";
    labels[2] = "MAIN PRESSURE";
    snprintf(values[0], sizeof(values[0]), "%.1f bar", p.basicPreinfusionBar);
    snprintf(values[1], sizeof(values[1]), "%.1f s", p.basicBloomS);
    snprintf(values[2], sizeof(values[2]), "%.1f bar", p.basicMainBar);
  } else {
    labels[0] = "TARGET YIELD";
    labels[1] = "MAX SHOT TIME";
    labels[2] = "FLOW CAP";
    snprintf(values[0], sizeof(values[0]), "%.1f g", p.targetWeightG);
    snprintf(values[1], sizeof(values[1]), "%.0f s", p.maxShotTimeS);
    if (p.basicFlowLimitGps > 0.0f)
      snprintf(values[2], sizeof(values[2]), "%.1f ml/s", p.basicFlowLimitGps);
    else
      snprintf(values[2], sizeof(values[2]), "OFF");
  }
  for (int i = 0; i < 3; ++i) adjustRow(canvas_, 43 + i * 48, labels[i], values[i]);
  if (quickPage_ == 0) {
    button(canvas_, 7, 208, 105, 29, "CANCEL", UiTheme::MUTED, false);
    button(canvas_, 208, 208, 105, 29, "NEXT", UiTheme::CYAN, true);
  } else {
    button(canvas_, 7, 208, 94, 29, "PREV", UiTheme::MUTED, false);
    button(canvas_, 108, 208, 104, 29, "PREVIEW", UiTheme::CYAN, false);
    button(canvas_, 219, 208, 94, 29, "DONE", UiTheme::GREEN, true);
  }
}

void ScreenManager::drawProfileSteps() {
  header(canvas_, "ADVANCED STEPS", true, "DRAFT");
  if (!profileDraft_.valid()) return;
  const auto& draft = profileDraft_.data();
  const uint8_t pageCount = max<uint8_t>(1, (draft.profile.stepCount + 3) / 4);
  if (stepPage_ >= pageCount) stepPage_ = pageCount - 1;
  for (uint8_t rowIndex = 0; rowIndex < 4; ++rowIndex) {
    const uint8_t index = stepPage_ * 4 + rowIndex;
    if (index >= draft.profile.stepCount) break;
    const auto& step = draft.steps[index];
    char label[30], value[42];
    snprintf(label, sizeof(label), "%u  %s", index + 1, step.name);
    snprintf(value, sizeof(value), "%c %.1f%s  %.1fs",
             step.mode == 0 ? 'P' : step.mode == 1 ? 'F' : 'D', step.target,
             profileModeUnit(step.mode), step.timeoutMs / 1000.0f);
    row(canvas_, 7, 42 + rowIndex * 40, 306, 36, label, value, true);
  }
  button(canvas_, 7, 207, 98, 30, "+ STEP", UiTheme::GREEN, false);
  button(canvas_, 111, 207, 98, 30, stepPage_ > 0 ? "PREV" : "", UiTheme::MUTED, false);
  button(canvas_, 215, 207, 98, 30,
         stepPage_ + 1 < pageCount ? "NEXT" : "", UiTheme::CYAN, false);
}

void ScreenManager::drawProfileStepControl() {
  header(canvas_, "STEP CONTROL", true, "DRAFT");
  if (!profileDraft_.valid() ||
      selectedStepIndex_ >= profileDraft_.data().profile.stepCount) return;
  const auto& step = profileDraft_.data().steps[selectedStepIndex_];
  char title[34], target[20], ramp[20], time[20];
  snprintf(title, sizeof(title), "%u/%u  %s", selectedStepIndex_ + 1,
           profileDraft_.data().profile.stepCount, step.name);
  row(canvas_, 7, 43, 306, 40, title, profileModeName(step.mode), true, nullptr,
      UiTheme::CYAN);
  snprintf(target, sizeof(target), "%.1f %s", step.target,
           profileModeUnit(step.mode));
  snprintf(ramp, sizeof(ramp), "%.1f /s", step.rampRate);
  snprintf(time, sizeof(time), "%.1f s", step.timeoutMs / 1000.0f);
  adjustRow(canvas_, 88, "TARGET", target);
  adjustRow(canvas_, 127, "RAMP", ramp);
  adjustRow(canvas_, 166, "MAX TIME", time);
  editorTabs(canvas_, 0);
}

void ScreenManager::drawProfileStepSafety() {
  header(canvas_, "STEP SAFETY", true, "DRAFT");
  if (!profileDraft_.valid() ||
      selectedStepIndex_ >= profileDraft_.data().profile.stepCount) return;
  const auto& step = profileDraft_.data().steps[selectedStepIndex_];
  const char* type = step.bumperType == 0 ? "NONE" :
                     step.bumperType == 1 ? "PRESSURE CAP" : "FLOW CAP";
  row(canvas_, 7, 54, 306, 46, "LIMIT TYPE", type, true, nullptr,
      step.bumperType == 0 ? UiTheme::MUTED : UiTheme::AMBER);
  char value[22];
  if (step.bumperType == 0) snprintf(value, sizeof(value), "DISABLED");
  else snprintf(value, sizeof(value), "%.1f %s", step.bumperValue,
                step.bumperType == 1 ? "bar" : "ml/s");
  adjustRow(canvas_, 108, "MAXIMUM", value,
            step.bumperType == 0 ? UiTheme::MUTED : UiTheme::AMBER);
  button(canvas_, 7, 166, 98, 34, "MOVE UP", UiTheme::MUTED, false);
  button(canvas_, 111, 166, 98, 34, "DELETE", UiTheme::RED, false);
  button(canvas_, 215, 166, 98, 34, "MOVE DOWN", UiTheme::MUTED, false);
  editorTabs(canvas_, 1);
}

void ScreenManager::drawProfileStepExit() {
  header(canvas_, "EXIT CONDITIONS (OR)", true, "DRAFT");
  if (!profileDraft_.valid() ||
      selectedStepIndex_ >= profileDraft_.data().profile.stepCount) return;
  const auto& step = profileDraft_.data().steps[selectedStepIndex_];
  const char* labels[] = {"PRESSURE >", "PRESSURE <", "FLOW >", "FLOW <",
                          "WEIGHT >=", "WATER >="};
  const char* units[] = {"bar", "bar", "ml/s", "ml/s", "g", "ml"};
  const float values[] = {step.pressureAbove, step.pressureBelow,
                          step.flowAbove, step.flowBelow, step.weight,
                          step.waterPumped};
  for (uint8_t rowIndex = 0; rowIndex < 3; ++rowIndex) {
    const uint8_t index = exitPage_ * 3 + rowIndex;
    char value[20];
    if (values[index] > 0.0f)
      snprintf(value, sizeof(value), "%.1f %s", values[index], units[index]);
    else
      snprintf(value, sizeof(value), "OFF");
    adjustRow(canvas_, 42 + rowIndex * 45, labels[index], value,
              values[index] > 0.0f ? UiTheme::CYAN : UiTheme::MUTED);
  }
  textLeft(canvas_, exitPage_ > 0 ? "< PREV" : "", 10, 195, &fonts::Font0,
           UiTheme::CYAN);
  textCenter(canvas_, exitPage_ == 0 ? "1 / 2" : "2 / 2", 160, 195,
             &fonts::Font0, UiTheme::MUTED);
  textRight(canvas_, exitPage_ == 0 ? "NEXT >" : "", 310, 195,
            &fonts::Font0, UiTheme::CYAN);
  editorTabs(canvas_, 2);
}

void ScreenManager::drawProfilePreview(const UiTelemetry& t) {
  header(canvas_, "PROFILE PREVIEW", true, "DRAFT");
  if (!profileDraft_.valid()) return;
  drawProfileGraph(7, 42, 306, 145);
  const char* error = profileDraft_.validate();
  if (error)
    textCenter(canvas_, error, 160, 194, &fonts::Font0, UiTheme::RED);
  else if (t.profileTransferStatusAgeMs < 2200 &&
           t.profileTransferStatus != HmiProtocol::ProfileTransferStatus::Pending)
    textCenter(canvas_,
               t.profileTransferStatus == HmiProtocol::ProfileTransferStatus::Accepted
                   ? "SAVED ON MAIN"
                   : "MAIN REJECTED PROFILE",
               160, 194, &fonts::Font0,
               t.profileTransferStatus == HmiProtocol::ProfileTransferStatus::Accepted
                   ? UiTheme::GREEN
                   : UiTheme::RED);
  button(canvas_, 7, 202, 306, 35,
         error ? "FIX PROFILE" : profileDraft_.dirty() ? "SEND TO MAIN"
                                                        : "SYNCED",
         error ? UiTheme::RED : UiTheme::CYAN, !error && profileDraft_.dirty());
}

void ScreenManager::drawProfileGraph(int x, int y, int w, int h) {
  panel(canvas_, x, y, w, h, UiTheme::CARD_ALT);
  if (!profileDraft_.valid() || profileDraft_.data().profile.stepCount == 0) return;
  const auto& draft = profileDraft_.data();
  float totalS = 0.0f;
  for (uint8_t i = 0; i < draft.profile.stepCount; ++i)
    totalS += draft.steps[i].timeoutMs / 1000.0f;
  totalS = max(1.0f, totalS);
  const int plotX = x + 23, plotY = y + 10;
  const int plotW = w - 32, plotH = h - 24;
  canvas_.drawFastHLine(plotX, plotY + plotH, plotW, UiTheme::GRID);
  canvas_.drawFastVLine(plotX, plotY, plotH, UiTheme::GRID);
  float elapsed = 0.0f;
  float previous = 0.0f;
  int previousX = plotX;
  int previousY = plotY + plotH;
  for (uint8_t i = 0; i < draft.profile.stepCount; ++i) {
    const auto& step = draft.steps[i];
    const float duration = step.timeoutMs / 1000.0f;
    elapsed += duration;
    const int endX = plotX + static_cast<int>(elapsed / totalS * plotW);
    const float maximum = step.mode == 0 ? 12.0f : step.mode == 1 ? 15.0f : 100.0f;
    const int endY = plotY + plotH - static_cast<int>(step.target / maximum * plotH);
    const uint16_t color = step.mode == 0 ? UiTheme::CYAN
                           : step.mode == 1 ? UiTheme::AMBER : UiTheme::TEXT;
    if (step.rampRate > 0.01f && step.rampRate < 900.0f)
      canvas_.drawLine(previousX, previousY, endX, endY, color);
    else {
      canvas_.drawLine(previousX, previousY, previousX, endY, color);
      canvas_.drawLine(previousX, endY, endX, endY, color);
    }
    if (i + 1 < draft.profile.stepCount)
      canvas_.drawFastVLine(endX, plotY, plotH, UiTheme::GRID);
    previousX = endX;
    previousY = endY;
    previous = step.target;
    (void)previous;
  }
  char duration[18];
  snprintf(duration, sizeof(duration), "%.0fs", totalS);
  textRight(canvas_, duration, x + w - 7, y + h - 7, &fonts::Font0,
            UiTheme::MUTED, UiTheme::CARD_ALT);
}
