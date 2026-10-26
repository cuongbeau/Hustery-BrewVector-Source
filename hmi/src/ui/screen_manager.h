#pragma once

#include <LovyanGFX.hpp>

#include "hardware/hmi_display.h"
#include "model/profile_draft.h"
#include "ui/shot_chart.h"
#include "ui/ui_types.h"
#include "ui/heater_settings.h"

class ScreenManager {
 public:
  explicit ScreenManager(HmiDisplay& display);

  bool begin();
  void show(ScreenId screen);
  void update(const UiTelemetry& telemetry, uint32_t nowMs);
  ScreenId screen() const { return screen_; }
  ScreenId handleTap(int x, int y, uint32_t nowMs);
  UiAction actionForTap(int x, int y) const;
  int profileIndexForTap(int x, int y) const;
  uint8_t selectedProfileIndex() const { return selectedProfileIndex_; }
  void beginProfileLoad(uint8_t index);
  void loadProfileDraft(const ProfileDetailData& source);
  const ProfileDetailData& profileDraft() const { return profileDraft_.data(); }
  bool profileDraftValid() const { return profileDraft_.valid(); }
  void markProfileSynced(uint8_t runtimeIndex);
  int historyIndexForTap(int x, int y) const;
  const char* wifiSsid() const { return wifiSsid_; }
  const char* wifiPassword() const { return wifiPassword_; }
  int selectedScaleDevice() const { return selectedScaleDevice_; }
  uint8_t sliderValueForTap(int x) const;
  float pressureCalibrationReferenceBar() const {
    return pressureCalibrationReferenceBar_;
  }
  float thermalOffsetDraft() const { return thermalOffsetDraft_; }
  float thermalOffsetBase() const { return thermalOffsetBase_; }
  void markThermalCalibrationRequest(uint16_t requestId, uint32_t nowMs);
  void applyThermalCalibrationReply(const HmiProtocol::CalibrationReplyPayload& reply);
  void markPressureCalibrationRequest(UiAction action);
  void applyPressureCalibrationReply(HmiProtocol::CalibrationOperation operation,
                                     HmiProtocol::CalibrationStatus status);

  const ShotChart& chart() const { return chart_; }
  const HeaterWire::Config& heaterDraft() const { return heaterEditor_.draft; }
  bool heaterDraftValid() const { return heaterEditor_.valid(); }
  void heaterRequestSent(const HeaterWire::Request& r,bool sent,uint32_t now) {
    heaterEditor_.message=sent?"Waiting for Main ACK":"Not sent: Main unavailable";
    if(sent){heaterEditor_.ticket=r.ticket;heaterEditor_.sentAt=now;heaterEditor_.op=r.op;}
  }
  ShotChart& historyChart() { return historyChart_; }

 private:
  HmiDisplay& display_;
  lgfx::LGFX_Sprite canvas_;
  ShotChart chart_;
  ShotChart historyChart_;
  ScreenId screen_ = ScreenId::Ready;
  ScreenId settingsReturn_ = ScreenId::Ready;
  uint8_t profilePage_ = 0;
  uint8_t historyPage_ = 0;
  uint8_t visibleProfileCount_ = 0;
  uint8_t visibleHistoryCount_ = 0;
  uint8_t selectedProfileIndex_ = 0xFF;
  uint8_t selectedStepIndex_ = 0;
  uint8_t quickPage_ = 0;
  uint8_t stepPage_ = 0;
  uint8_t exitPage_ = 0;
  ProfileDraft profileDraft_{};
  const HmiOta::NetworkInfo* wifiNetworks_ = nullptr;
  uint8_t wifiNetworkCount_ = 0;
  bool wifiScanning_ = false;
  int8_t selectedNetwork_ = -1;
  const BleDiscoveredScale* scaleDevices_ = nullptr;
  uint8_t scaleDeviceCount_ = 0;
  bool scaleScanning_ = false;
  int8_t selectedScaleDevice_ = -1;

  float thermalOffsetDraft_ = 21.0f, thermalOffsetBase_ = 21.0f;
  bool thermalCalLoaded_ = false, thermalCalDirty_ = false;
  bool thermalCalConflict_ = false, thermalCalEditable_ = false;
  uint16_t thermalCalPendingId_ = 0;
  uint32_t thermalCalRequestedAt_ = 0;
  const char* thermalCalMessage_ = nullptr;
  void drawCalibration();
  void drawThermalCal(const UiTelemetry& t);

  enum class PressureCalStep : uint8_t { Overview, Zero, Reference, Review };
  PressureCalStep pressureCalStep_ = PressureCalStep::Overview;
  float pressureCalibrationReferenceBar_ = 9.0f;
  UiAction pressureCalPendingAction_ = UiAction::None;
  const char* pressureCalMessage_ = nullptr;
  bool pressureCalTestRunning_ = false;

  bool keyboardSymbols_ = false;
  bool keyboardUpper_ = false;
  bool passwordVisible_ = true;
  char wifiSsid_[33] = {};
  char wifiPassword_[65] = {};
  char profileNameEdit_[24] = {};
  uint32_t lastDrawMs_ = 0;
  struct ReadyTemperatureSample {
    uint32_t atMs = 0;
    float brewC = NAN;
  };
  static constexpr uint8_t ReadyTemperatureCapacity = 61;
  ReadyTemperatureSample readyTemperatures_[ReadyTemperatureCapacity]{};
  uint8_t readyTemperatureHead_ = 0;
  uint8_t readyTemperatureCount_ = 0;
  uint32_t readyTemperatureLastSourceMs_ = 0;
  uint32_t readyTemperatureLastSampleMs_ = 0;
  uint32_t readyTemperatureObservedSourceMs_ = 0;
  uint32_t readyTemperatureObservedAtMs_ = 0;
  bool readyMenuOpen_ = false;
  bool readyControlsEnabled_ = false;
  bool readyStartEnabled_ = false;
  bool readyHeartbeatActive_ = false;
  bool readyManualEnabled_ = false;
  bool readyYieldEnabled_ = false;
  bool yieldActionsEnabled_ = false;
  HeaterSettings::Editor heaterEditor_{};
  HeaterWire::Status heaterApplied_{};
  uint8_t heaterPage_=0;
  bool heaterEditable_=false, heaterApplyAllowed_=false, heaterFresh_=false, heaterBusy_=false;
  void drawHeaterSettings(const UiTelemetry& t);
  void handleHeaterTap(int x,int y);

  void resetChart();
  void appendChartSample(const UiTelemetry& telemetry);
  void appendReadyTemperatureSample(const UiTelemetry& telemetry, uint32_t nowMs);
  void drawReadyTemperatureChart(const UiTelemetry& telemetry, uint32_t nowMs);
  void drawReady(const UiTelemetry& telemetry, uint32_t nowMs);
  void drawYieldEditor(const UiTelemetry& telemetry);
  void drawFlush(const UiTelemetry& telemetry);
  void drawBrew(const UiTelemetry& telemetry);
  void drawBrewPressurePlot(const ShotChart& data);
  void drawBrewChart(const ShotChart& data, int offsetY = 0);
  void drawResultSummary(const ShotSummary* result, const ShotChart& data,
                         const UiTelemetry& telemetry, bool live);
  void drawResult(const UiTelemetry& telemetry);
  void drawSettings(const UiTelemetry& telemetry);
  void drawConnections(const UiTelemetry& telemetry);
  void drawWebAccess(const UiTelemetry& telemetry);
  void drawWifi(const UiTelemetry& telemetry);
  void drawNetworks();
  void drawWifiKeyboard();
  void drawRestartConfirm();
  void drawScale(const UiTelemetry& telemetry);
  void drawScaleDevices(const UiTelemetry& telemetry);

  void drawPressureCal(const UiTelemetry& telemetry);

  void drawMachine();
  void drawDisplaySound(const UiTelemetry& telemetry);
  void drawStorageLogs(const UiTelemetry& telemetry);
  void drawSystem(const UiTelemetry& telemetry);
  void drawDiagnostics(const UiTelemetry& telemetry);
  void drawFirmware(const UiTelemetry& telemetry);
  void drawProfiles(const UiTelemetry& telemetry);
  void drawProfileDetail(const UiTelemetry& telemetry);
  void drawProfileQuick();
  void drawProfileSteps();
  void drawProfileStepControl();
  void drawProfileStepSafety();
  void drawProfileStepExit();
  void drawProfilePreview(const UiTelemetry& telemetry);
  void drawProfileNameKeyboard();
  void drawQuickConvertConfirm();
  void drawProfileGraph(int x, int y, int w, int h);
  void drawHistory(const UiTelemetry& telemetry);
  void drawShotDetail(const UiTelemetry& telemetry);
  void drawSavedCompare(const UiTelemetry& telemetry);
  void drawEraseHistoryConfirm();
  void drawCompare(const UiTelemetry& telemetry);
  void drawPanel(int x, int y, int w, int h, uint16_t fill);
  void drawMenuIcon(uint8_t index, int centerX, int centerY, uint16_t color);
  void drawChart(int x, int y, int w, int h, const UiTelemetry& telemetry,
                 bool includeTarget);
  void handleKeyboardTap(int x, int y);
  void handleProfileNameKeyboardTap(int x, int y);
  bool renderHistoryChart_ = false;
};
