#include "ui/screen_manager.h"

#include <math.h>
#include <stdio.h>

#include "ui/components.h"
#include "ui/theme.h"
#include "ui/ready_layout.h"
#include "ui/brew_layout.h"
#include "thermal_calibration.h"

using namespace UiComponents;

namespace {

void drawGear(lgfx::LGFX_Sprite& c, int cx, int cy, uint16_t color) {
  // Precision 6-tooth industrial gear
  c.fillCircle(cx, cy, 8, color);
  for (int i = 0; i < 6; ++i) {
    const float angle = i * 1.047197f;  // 60 degrees
    const int tx = cx + static_cast<int>(cosf(angle) * 9.5f);
    const int ty = cy + static_cast<int>(sinf(angle) * 9.5f);
    c.fillCircle(tx, ty, 2, color);
  }
  c.fillCircle(cx, cy, 3, UiTheme::BG);
  c.drawCircle(cx, cy, 3, color);
}

void drawCup(lgfx::LGFX_Sprite& c, int cx, int cy, uint16_t color) {
  // Elegant modern espresso cup with saucer & dual steam wisps
  c.fillRoundRect(cx - 16, cy + 8, 32, 2, 1, color);           // Saucer
  c.fillRoundRect(cx - 12, cy - 4, 24, 11, 3, color);          // Cup body
  c.fillRoundRect(cx - 10, cy - 3, 20, 3, 1, UiTheme::CARD_ALT); // Inner rim / liquid
  c.drawArc(cx + 12, cy + 1, 5, 3, 270, 90, color);            // Handle

  // Dual sleek steam wisps
  c.drawLine(cx - 4, cy - 8, cx - 2, cy - 12, color);
  c.drawLine(cx - 2, cy - 12, cx - 4, cy - 16, color);
  c.drawLine(cx + 4, cy - 8, cx + 6, cy - 12, color);
  c.drawLine(cx + 6, cy - 12, cx + 4, cy - 16, color);
}

void drawDrop(lgfx::LGFX_Sprite& c, int cx, int cy, uint16_t color) {
  // Solid sleek water droplet with inner gloss
  c.fillTriangle(cx, cy - 10, cx - 6, cy + 2, cx + 6, cy + 2, color);
  c.fillCircle(cx, cy + 2, 6, color);
  c.fillCircle(cx + 2, cy + 1, 1, UiTheme::CARD_ALT);
}

void drawFadersIcon(lgfx::LGFX_Sprite& c, int cx, int cy, uint16_t color) {
  // 3 Vertical Recipe Fader Tracks (Classic Equalizer / Mixer)
  // Track 1 (Left): Fader high
  c.drawFastVLine(cx - 8, cy - 8, 17, UiTheme::BORDER);
  c.fillRoundRect(cx - 11, cy - 6, 7, 5, 1, color);

  // Track 2 (Center): Fader low
  c.drawFastVLine(cx, cy - 8, 17, UiTheme::BORDER);
  c.fillRoundRect(cx - 3, cy + 1, 7, 5, 1, color);

  // Track 3 (Right): Fader middle
  c.drawFastVLine(cx + 8, cy - 8, 17, UiTheme::BORDER);
  c.fillRoundRect(cx + 5, cy - 2, 7, 5, 1, color);
}

void drawClockIcon(lgfx::LGFX_Sprite& c, int cx, int cy, uint16_t color) {
  // Precision analog clock (10:10 watchmaker position)
  c.drawCircle(cx, cy, 9, color);
  c.drawCircle(cx, cy, 8, color);
  c.drawLine(cx, cy, cx - 4, cy - 4, color);                   // Hour hand
  c.drawLine(cx, cy, cx + 5, cy - 4, color);                   // Minute hand
  c.fillCircle(cx, cy, 1, color);
}

uint16_t blend565(uint16_t from, uint16_t to, uint8_t amount) {
  const int fromR = (from >> 11) & 0x1f, toR = (to >> 11) & 0x1f;
  const int fromG = (from >> 5) & 0x3f, toG = (to >> 5) & 0x3f;
  const int fromB = from & 0x1f, toB = to & 0x1f;
  const int r = fromR + (toR - fromR) * amount / 255;
  const int g = fromG + (toG - fromG) * amount / 255;
  const int b = fromB + (toB - fromB) * amount / 255;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

}  // namespace

void ScreenManager::resetChart() {
  chart_.reset();
}

void ScreenManager::appendChartSample(const UiTelemetry& t) {
  // Capture by machine state rather than visible screen. Main may enter
  // Ending before the last telemetry samples are rendered; keeping that tail
  // makes the frozen End Shot chart match the live Brew chart.
  if (t.machineState == HmiProtocol::MachineState::Brew ||
      t.machineState == HmiProtocol::MachineState::Ending)
    chart_.append(t);
}

void ScreenManager::appendReadyTemperatureSample(const UiTelemetry& t,
                                                  uint32_t nowMs) {
  // The ready chart is a short, live history, never an old Main value held
  // through a disconnect or a shot transition.
  if (!t.mainConnected || !t.mainCompatible || t.mainFrameAgeMs > 1500U ||
      t.machineState != HmiProtocol::MachineState::Ready ||
      !isfinite(t.boilerTempC) || t.sourceTimestampMs == 0) {
    readyTemperatureHead_ = readyTemperatureCount_ = 0;
    readyTemperatureLastSourceMs_ = readyTemperatureLastSampleMs_ = 0;
    readyTemperatureObservedSourceMs_ = readyTemperatureObservedAtMs_ = 0;
    return;
  }
  if (t.sourceTimestampMs != readyTemperatureObservedSourceMs_) {
    readyTemperatureObservedSourceMs_ = t.sourceTimestampMs;
    readyTemperatureObservedAtMs_ = nowMs;
  }
  if (nowMs - readyTemperatureObservedAtMs_ > 1500U) {
    readyTemperatureHead_ = readyTemperatureCount_ = 0;
    // Keep the observed source timestamp: an unchanged cached packet must
    // not seed another chart just because other Main frames are still fresh.
    return;
  }
  if (readyTemperatureLastSourceMs_ &&
      t.sourceTimestampMs < readyTemperatureLastSourceMs_) {
    readyTemperatureHead_ = readyTemperatureCount_ = 0;
    readyTemperatureLastSampleMs_ = 0;
  }
  if (t.sourceTimestampMs == readyTemperatureLastSourceMs_ ||
      (readyTemperatureCount_ && nowMs - readyTemperatureLastSampleMs_ < 1000U))
    return;

  const uint8_t write = (readyTemperatureHead_ + readyTemperatureCount_) %
                        ReadyTemperatureCapacity;
  readyTemperatures_[write].atMs = nowMs;
  readyTemperatures_[write].brewC = t.boilerTempC;
  if (readyTemperatureCount_ < ReadyTemperatureCapacity)
    ++readyTemperatureCount_;
  else
    readyTemperatureHead_ = (readyTemperatureHead_ + 1U) % ReadyTemperatureCapacity;
  readyTemperatureLastSourceMs_ = t.sourceTimestampMs;
  readyTemperatureLastSampleMs_ = nowMs;
}

void ScreenManager::drawReadyTemperatureChart(const UiTelemetry& t,
                                               uint32_t nowMs) {
  constexpr int left = 176, right = 311, top = 58, bottom = 105;
  textLeft(canvas_, "TEMP / 60s", left, 35, &fonts::Font0, UiTheme::MUTED);
  float target = NAN;
  if (readyTemperatureCount_ &&
      nowMs - readyTemperatureObservedAtMs_ <= 1500U &&
      t.mainConnected && t.mainCompatible && t.mainFrameAgeMs <= 1500U &&
      t.heaterFresh && isfinite(t.blockTempC) && isfinite(t.boilerTempC) &&
      isfinite(t.heaterStatus.config.target) &&
      t.heaterStatus.config.target >= 30.0f &&
      t.heaterStatus.config.target <= 130.0f) {
    const float offset = t.blockTempC - t.boilerTempC;
    if (ThermalCalibration::valid(offset))
      target = ThermalCalibration::estimate(t.heaterStatus.config.target, offset);
  }

  float low = target, high = target;
  bool hasValue = isfinite(target);
  for (uint8_t i = 0; i < readyTemperatureCount_; ++i) {
    const auto& sample = readyTemperatures_[(readyTemperatureHead_ + i) %
                                            ReadyTemperatureCapacity];
    if (nowMs - sample.atMs > 60000U || !isfinite(sample.brewC)) continue;
    if (!hasValue) low = high = sample.brewC;
    else { low = fminf(low, sample.brewC); high = fmaxf(high, sample.brewC); }
    hasValue = true;
  }
  if (!hasValue) return;
  if (high - low < 20.0f) {
    const float middle = (high + low) * 0.5f;
    low = middle - 10.0f;
    high = middle + 10.0f;
  } else {
    low = floorf(low / 10.0f) * 10.0f;
    high = ceilf(high / 10.0f) * 10.0f;
  }
  const auto chartY = [&](float c) {
    return bottom - static_cast<int>(lroundf((c - low) * (bottom - top) /
                                            (high - low)));
  };
  canvas_.drawFastHLine(left, bottom, right - left + 1, UiTheme::GRID);
  if (isfinite(target)) {
    char label[20];
    snprintf(label, sizeof(label), "TARGET %.0fC", target);
    textRight(canvas_, label, right, 47, &fonts::Font0, UiTheme::CYAN);
    const int y = constrain(chartY(target), top, bottom);
    for (int x = left; x <= right; x += 8)
      canvas_.drawFastHLine(x, y, min(4, right - x + 1), UiTheme::CYAN);
  }

  bool previous = false;
  int previousX = left, previousY = bottom;
  uint32_t previousAt = 0;
  for (uint8_t i = 0; i < readyTemperatureCount_; ++i) {
    const auto& sample = readyTemperatures_[(readyTemperatureHead_ + i) %
                                            ReadyTemperatureCapacity];
    const uint32_t age = nowMs - sample.atMs;
    if (age > 60000U || !isfinite(sample.brewC)) { previous = false; continue; }
    const int x = left + static_cast<int>((60000U - age) * (right - left) / 60000U);
    const int y = constrain(chartY(sample.brewC), top, bottom);
    if (previous && sample.atMs - previousAt <= 2500U)
      canvas_.drawLine(previousX, previousY, x, y, UiTheme::ORANGE_RED);
    previous = true;
    previousX = x;
    previousY = y;
    previousAt = sample.atMs;
  }
  if (previous) canvas_.fillCircle(previousX, previousY, 2, UiTheme::ORANGE_RED);
  textLeft(canvas_, "-60s", left, 116, &fonts::Font0, UiTheme::DIM);
  textRight(canvas_, "NOW", right, 116, &fonts::Font0, UiTheme::DIM);
}

void ScreenManager::drawPanel(int x, int y, int w, int h, uint16_t fill) {
  panel(canvas_, x, y, w, h, fill);
}

void ScreenManager::drawMenuIcon(uint8_t index, int cx, int cy,
                                 uint16_t color) {
  if (index == 0) drawDrop(canvas_, cx, cy, color);
  else if (index == 1) drawFadersIcon(canvas_, cx, cy, color);
  else drawClockIcon(canvas_, cx, cy, color);
}

void ScreenManager::drawReady(const UiTelemetry& t, uint32_t nowMs) {
  using namespace ReadyLayout;
  readyControlsEnabled_=t.mainConnected && t.mainCompatible &&
      t.machineState==HmiProtocol::MachineState::Ready && !t.commandPending;
  const HmiProtocol::ProfileInfoPayload* active=nullptr;
  for(uint8_t i=0;t.profiles && i<t.profileCount;++i)
    if(t.profiles[i].index==t.selectedProfileIndex)active=&t.profiles[i];
  const bool profileReady=active && !(active->flags&2U);
  const bool brewReady=t.heaterReady && profileReady;
  readyStartEnabled_=readyControlsEnabled_ && t.heaterFresh &&
      (t.heaterMode==0 || brewReady);
  readyHeartbeatActive_=readyStartEnabled_ && brewReady &&
      !(t.commandAction==UiAction::StartBrew && t.commandStatus);
  readyYieldEnabled_=readyControlsEnabled_ && t.scaleConnected && (t.quickYieldFlags&1U);
  const char* status=!t.mainConnected?"OFFLINE":!t.mainCompatible?"LINK ERROR":
      t.commandPending?"WAIT":!t.heaterFresh?"HEATER LINK":t.heaterMode==0?"HEATER OFF":t.heaterReady?"READY":"PREHEATING";
  const uint16_t statusColor=!t.mainConnected || !t.mainCompatible?UiTheme::RED:
      t.heaterReady?UiTheme::GREEN:UiTheme::AMBER;
  textLeft(canvas_,status,8,12,&fonts::Font0,statusColor);
  canvas_.fillCircle(220,12,2,t.scaleConnected?UiTheme::GREEN:UiTheme::DIM);
  textLeft(canvas_,"Scale",228,12,&fonts::Font0,UiTheme::MUTED);
  canvas_.fillCircle(274,12,2,t.wifiConnected?UiTheme::GREEN:UiTheme::DIM);
  textLeft(canvas_,"Wi-Fi",282,12,&fonts::Font0,UiTheme::MUTED);
  char value[24];
  const bool temperatureFresh=readyTemperatureCount_ &&
      nowMs-readyTemperatureObservedAtMs_<=1500U;
  if(temperatureFresh && t.mainConnected && t.mainCompatible &&
     isfinite(t.boilerTempC))snprintf(value,sizeof(value),"%.1f",t.boilerTempC);
  else snprintf(value,sizeof(value),"--.-");
  textLeft(canvas_,"BREW EST.",10,35,&fonts::Font0,UiTheme::MUTED);
  canvas_.setFont(&fonts::FreeSansBold24pt7b);
  const int unitX = 10 + canvas_.textWidth(value) + 4;
  textLeft(canvas_,value,10,76,&fonts::FreeSansBold24pt7b,UiTheme::ORANGE_RED);
  canvas_.drawCircle(unitX+2,64,2,UiTheme::ORANGE_RED);
  textLeft(canvas_,"C",unitX+8,76,&fonts::Font4,UiTheme::ORANGE_RED);
  if(temperatureFresh && t.mainConnected && t.mainCompatible &&
     isfinite(t.blockTempC))snprintf(value,sizeof(value),"Block %.1f C",t.blockTempC);
  else snprintf(value,sizeof(value),"Block -- C");
  textLeft(canvas_,value,11,108,&fonts::Font2,UiTheme::MUTED);
  drawReadyTemperatureChart(t,nowMs);
  panel(canvas_,Profile.x,Profile.y,Profile.w,Profile.h,UiTheme::CARD_ALT,UiTheme::BORDER);
  textLeft(canvas_,"PROFILE",18,144,&fonts::Font0,UiTheme::MUTED,UiTheme::CARD_ALT);
  char name[32];
  snprintf(name,sizeof(name),"%s",active?active->name:t.mainConnected?t.profileName:"Connect to Main");
  canvas_.setFont(&fonts::Font2);
  size_t len=strlen(name);
  while(len && canvas_.textWidth(name)>166){
    --len;while(len && (static_cast<uint8_t>(name[len])&0xC0)==0x80)--len;name[len]=0;
  }
  textLeft(canvas_,name,18,163,&fonts::Font2,UiTheme::TEXT,UiTheme::CARD_ALT);
  chevron(canvas_,197,158,UiTheme::CYAN);
  panel(canvas_,Yield.x,Yield.y,Yield.w,Yield.h,UiTheme::CARD_ALT,
        readyYieldEnabled_?UiTheme::CYAN_DARK:UiTheme::BORDER);
  textLeft(canvas_,t.quickYieldFlags&4U?"YIELD *":"STOP YIELD",226,144,&fonts::Font0,UiTheme::MUTED,UiTheme::CARD_ALT);
  if(!t.scaleConnected || !t.mainConnected)snprintf(value,sizeof(value),"--");
  else if(t.quickYieldG<=0)snprintf(value,sizeof(value),"OFF");
  else snprintf(value,sizeof(value),"%.1f g",t.quickYieldG);
  textLeft(canvas_,value,226,163,&fonts::Font2,readyYieldEnabled_?UiTheme::CYAN:UiTheme::DIM,UiTheme::CARD_ALT);
  const char* startLabel=t.commandAction==UiAction::StartBrew && t.commandStatus?t.commandStatus:
      !t.heaterFresh?"UNAVAILABLE":t.heaterMode==0?"PREHEAT":
      !t.heaterReady?"PREHEATING":profileReady?"START BREW":"PROFILE REQUIRED";
  if(readyHeartbeatActive_){
    const uint8_t pulse=heartbeatIntensity(nowMs);
    const uint16_t outer=blend565(UiTheme::BG,UiTheme::CYAN,pulse/2U);
    const uint16_t inner=blend565(UiTheme::BG,UiTheme::CYAN,pulse);
    const uint16_t fill=blend565(UiTheme::CYAN_DARK,UiTheme::CYAN,
                                 static_cast<uint8_t>(112U+pulse/4U));
    canvas_.drawRoundRect(Start.x-3,Start.y-3,Start.w+6,Start.h+6,
                          UiTheme::RADIUS+3,outer);
    canvas_.drawRoundRect(Start.x-2,Start.y-2,Start.w+4,Start.h+4,
                          UiTheme::RADIUS+2,inner);
    panel(canvas_,Start.x,Start.y,Start.w,Start.h,fill,fill);
    textCenter(canvas_,startLabel,Start.x+Start.w/2,Start.y+Start.h/2,
               &fonts::Font2,UiTheme::BG,fill);
  }else{
    const uint16_t accent=readyStartEnabled_ && t.heaterMode==0?UiTheme::AMBER:
        readyStartEnabled_?UiTheme::CYAN:UiTheme::DIM;
    button(canvas_,Start.x,Start.y,Start.w,Start.h,startLabel,accent,readyStartEnabled_);
  }
  button(canvas_,Flush.x,Flush.y,Flush.w,Flush.h,
         t.commandAction==UiAction::StartFlush && t.commandStatus?t.commandStatus:"FLUSH",
         readyControlsEnabled_?UiTheme::CYAN:UiTheme::DIM,false);
  panel(canvas_,Menu.x,Menu.y,Menu.w,Menu.h,UiTheme::CARD_ALT,
        readyMenuOpen_?UiTheme::CYAN:UiTheme::BORDER);
  for (int row = 0; row < 3; ++row)
    canvas_.drawFastHLine(Menu.x+13,Menu.y+14+row*5,18,UiTheme::TEXT);
  textCenter(canvas_,"MENU",Menu.x+Menu.w/2,Menu.y+37,&fonts::Font0,
             UiTheme::MUTED,UiTheme::CARD_ALT);
  if (readyMenuOpen_) {
    panel(canvas_,168,98,144,80,UiTheme::CARD_ALT,UiTheme::CYAN_DARK);
    textCenter(canvas_,"HISTORY",240,118,&fonts::Font2,UiTheme::TEXT,
               UiTheme::CARD_ALT);
    canvas_.drawFastHLine(176,138,128,UiTheme::BORDER);
    textCenter(canvas_,"SETTINGS",240,158,&fonts::Font2,UiTheme::TEXT,
               UiTheme::CARD_ALT);
  }
}

void ScreenManager::drawYieldEditor(const UiTelemetry& t) {
  header(canvas_,"SHOT YIELD",false);
  // KeepYieldEdit is a background lease renewal. It must not dim the actions
  // every 600 ms, and a brief sample gap during physical tare must not close
  // or visually disable an already-owned editor session.
  const bool userActionPending=t.commandPending && t.commandAction!=UiAction::None;
  yieldActionsEnabled_=t.mainConnected && t.mainCompatible &&
      t.machineState==HmiProtocol::MachineState::Ready &&
      (t.quickYieldFlags&2U) && !userActionPending;
  textCenter(canvas_,"Rotate encoder: 1 g per step",160,55,&fonts::Font2,UiTheme::MUTED);
  char value[24];
  if(!(t.quickYieldFlags&2U))snprintf(value,sizeof(value),"WAIT");
  else if(t.quickYieldG<=0)snprintf(value,sizeof(value),"OFF");
  else snprintf(value,sizeof(value),"%.1f g",t.quickYieldG);
  textCenter(canvas_,value,160,106,&fonts::FreeSansBold24pt7b,UiTheme::CYAN);
  textCenter(canvas_,"Next shot only - profile stays unchanged",160,153,&fonts::Font0,UiTheme::MUTED);
  textCenter(canvas_,"Whole-shot limit; 0 g = OFF",160,173,&fonts::Font0,UiTheme::MUTED);
  const auto& c=ReadyLayout::YieldCancel; const auto& a=ReadyLayout::YieldApply;
  button(canvas_,c.x,c.y,c.w,c.h,"CANCEL",yieldActionsEnabled_?UiTheme::TEXT:UiTheme::DIM,false);
  button(canvas_,a.x,a.y,a.w,a.h,"APPLY",yieldActionsEnabled_?UiTheme::CYAN:UiTheme::DIM,false);
}

void ScreenManager::drawFlush(const UiTelemetry& t) {
  header(canvas_, "FLUSHING", false);
  drawDrop(canvas_, 160, 66, UiTheme::CYAN);
  char remaining[16];
  snprintf(remaining, sizeof(remaining), "%.1f s", t.flushRemainingS);
  textCenter(canvas_, remaining, 160, 111, &fonts::FreeSansBold24pt7b, UiTheme::TEXT);
  textCenter(canvas_, "CLEANING BREW GROUP", 160, 143,
             &fonts::FreeSansBold9pt7b, UiTheme::MUTED);
  canvas_.fillRoundRect(24, 158, 272, 9, 4, UiTheme::BORDER);
  const int progress = t.flushDurationS > 0 && isfinite(t.flushRemainingS)
      ? constrain(static_cast<int>((t.flushDurationS - t.flushRemainingS) *
                                   272.0f / t.flushDurationS), 0, 272) : 0;
  if (progress > 0)
    canvas_.fillRoundRect(24, 158, progress, 9, 4, UiTheme::CYAN);
  const char* stopLabel =
      t.commandAction == UiAction::Stop && t.commandStatus
          ? t.commandStatus
          : "STOP FLUSH";
  const auto& stop = ReadyLayout::StopFlush;
  button(canvas_, stop.x, stop.y, stop.w, stop.h, stopLabel, UiTheme::RED, true);
}

void ScreenManager::drawBrew(const UiTelemetry& t) {
  const bool live = t.mainConnected && t.mainCompatible;
  char title[48];
  snprintf(title, sizeof(title), "%s", t.manualMode ? "Manual" : t.profileName ? t.profileName : "BREW");
  canvas_.setFont(&fonts::Font2);
  size_t length = strlen(title);
  while (length && canvas_.textWidth(title) > 205) {
    --length;
    while (length && (static_cast<uint8_t>(title[length]) & 0xC0) == 0x80) --length;
    title[length] = 0;
  }
  textLeft(canvas_, title, 8, 12, &fonts::Font2, UiTheme::TEXT);
  char value[24];
  if (live && isfinite(t.boilerTempC)) snprintf(value,sizeof(value),"%.1f C",t.boilerTempC);
  else snprintf(value,sizeof(value),"-- C");
  textRight(canvas_,value,288,12,&fonts::Font2,UiTheme::RED);
  textRight(canvas_,"EST",312,12,&fonts::Font0,UiTheme::MUTED);
  canvas_.drawFastHLine(0,25,320,UiTheme::BORDER);
  if (live && isfinite(t.elapsedS)) snprintf(value,sizeof(value),"%.1f s",t.elapsedS);
  else snprintf(value,sizeof(value),"-- s");
  textLeft(canvas_,value,8,44,&fonts::Font4,UiTheme::TEXT);
  const char* status = !live ? "MAIN OFFLINE" :
      t.phaseName && strcmp(t.phaseName,"SCALE PREP")==0 ? "TARING SCALE" :
      !t.scaleConnected ? "SCALE OFFLINE" : t.phaseName ? t.phaseName : "BREW";
  char phase[32]; snprintf(phase,sizeof(phase),"%s",status);
  canvas_.setFont(&fonts::Font0);
  size_t n=strlen(phase);
  while(n && canvas_.textWidth(phase)>124){--n;while(n && (static_cast<uint8_t>(phase[n])&0xC0)==0x80)--n;phase[n]=0;}
  textLeft(canvas_,phase,112,39,&fonts::Font0,live && t.scaleConnected?UiTheme::MUTED:UiTheme::AMBER);
  if(t.manualMode && live){snprintf(value,sizeof(value),"Pump %.0f%%",t.pumpPercent);textLeft(canvas_,value,112,51,&fonts::Font0,UiTheme::MUTED);}
  const auto& stop=BrewLayout::Stop;
  const char* stopLabel=t.commandAction==UiAction::Stop && t.commandStatus?t.commandStatus:"STOP";
  canvas_.setFont(&fonts::Font2);
  const lgfx::IFont* stopFont = canvas_.textWidth(stopLabel) <= stop.w - 8
      ? static_cast<const lgfx::IFont*>(&fonts::Font2)
      : static_cast<const lgfx::IFont*>(&fonts::Font0);
  panel(canvas_,stop.x,stop.y,stop.w,stop.h,UiTheme::RED,UiTheme::RED);
  textCenter(canvas_,stopLabel,stop.x+stop.w/2,stop.y+stop.h/2,
             stopFont,UiTheme::TEXT,UiTheme::RED);
  drawBrewChart(chart_);
  canvas_.drawFastHLine(0,BrewLayout::FooterY,320,UiTheme::BORDER);
  const char* labels[]={"PRESSURE","PUMP FLOW","YIELD"};
  const char* units[]={"bar","ml/s","g"};
  const float values[]={t.pressureBar,t.flowGps,t.weightG};
  const uint16_t colors[]={UiTheme::CYAN,UiTheme::AMBER,UiTheme::TEXT};
  for(int i=0;i<3;++i){
    const int left=i*107, center=left+53;
    if(i)canvas_.drawFastVLine(left,BrewLayout::FooterY,42,UiTheme::BORDER);
    textCenter(canvas_,labels[i],center,207,&fonts::Font0,UiTheme::MUTED);
    const bool valid=(i==2?t.scaleConnected:live) && isfinite(values[i]);
    if(valid)snprintf(value,sizeof(value),"%.1f",values[i]);else snprintf(value,sizeof(value),"--");
    canvas_.setFont(&fonts::Font4);
    const lgfx::IFont* font=canvas_.textWidth(value)<=65?static_cast<const lgfx::IFont*>(&fonts::Font4):static_cast<const lgfx::IFont*>(&fonts::Font2);
    textRight(canvas_,value,left+72,226,font,valid?colors[i]:UiTheme::MUTED);
    textLeft(canvas_,units[i],left+77,230,&fonts::Font0,UiTheme::MUTED);
  }
}

void ScreenManager::drawResult(const UiTelemetry& t) {
  drawResultSummary(t.lastResult, chart_, t, true);
}
