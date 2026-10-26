#include <Arduino.h>

#include "app/demo_data.h"
#include "app/runtime_metrics.h"
#include "app/hmi_settings.h"
#include "communication/main_link.h"
#include "hardware/audio_service.h"
#include "hardware/hmi_display.h"
#include "hardware/touch_controller.h"
#include "network/ota_service.h"
#include "scale/scale_manager.h"
#include "update/main_firmware_updater.h"
#include "storage/shot_store.h"
#include "storage/control_trace_store.h"
#include "ui/screen_manager.h"
#include "model/shot_result.h"

namespace {
HmiDisplay display;
TouchController touch;
MainLink mainLink;
ScaleManager scaleManager;
ScreenManager screens(display);
DemoData demo;
ShotStore shotStore;
ControlTraceStore controlTraceStore;
HmiSettings settings;
AudioService audio;
UiTelemetry telemetry;
ShotResultSnapshot lastResult;
bool touchWasPressed = false;
char profileName[sizeof(HmiProtocol::TelemetryPayload::profileName)] = "OFF";
char phaseName[sizeof(HmiProtocol::TelemetryPayload::phaseName)] = "IDLE";
UiAction localErrorAction = UiAction::None;
bool manualSelectionCommand = false;
uint32_t localErrorUntilMs = 0;
const char* saveStatus = nullptr;
uint32_t saveStatusUntilMs = 0;
bool touchHardwareReady = false;
bool audioHardwareReady = false;
bool displayDimmed = false;
uint32_t lastInteractionMs = 0;
uint32_t restartAtMs = 0;
HmiProtocol::MachineState previousMachineState = HmiProtocol::MachineState::Boot;
char wifiSsid[33] = "-";
char wifiIp[16] = "0.0.0.0";
char wifiApIp[16] = {};
char wifiApName[33] = {};
ShotSummary selectedShot{};
ShotSummary comparisonShot{};
bool hasSelectedShot = false;
bool hasComparisonShot = false;
int selectedHistoryIndex = -1;
uint32_t seenProfileDetailGeneration = 0;
uint16_t seenProfileTransferResultId = 0;
uint32_t lastScaleTxMs = 0;
uint32_t seenCalibrationReplyGeneration = 0;
uint32_t lastCalibrationReadMs = 0;
uint32_t quickYieldToken=0, quickYieldStartedMs=0, quickYieldRenewMs=0;
bool quickYieldConfirmed=false, quickYieldClosing=false;

bool isOperatingScreen(ScreenId id) {
  return id == ScreenId::Ready || id == ScreenId::Flush ||
         id == ScreenId::Brew || id == ScreenId::ShotResult;
}

bool isProfileEditorScreen(ScreenId id) {
  return id == ScreenId::Profiles || id == ScreenId::ProfileDetail ||
         id == ScreenId::ProfileQuick || id == ScreenId::ProfileSteps ||
         id == ScreenId::ProfileStepControl ||
         id == ScreenId::ProfileStepSafety || id == ScreenId::ProfileStepExit ||
         id == ScreenId::ProfilePreview;
}

ScreenId screenForState(HmiProtocol::MachineState state) {
  if (state == HmiProtocol::MachineState::Brew) return ScreenId::Brew;
  if (state == HmiProtocol::MachineState::Flush) return ScreenId::Flush;
  if (state == HmiProtocol::MachineState::Done ||
      state == HmiProtocol::MachineState::Ending) return ScreenId::ShotResult;
  return ScreenId::Ready;
}

UiAction uiActionForCommand(HmiProtocol::Command command) {
  switch (command) {
    case HmiProtocol::Command::StartBrew: return UiAction::StartBrew;
    case HmiProtocol::Command::StartFlush: return UiAction::StartFlush;
    case HmiProtocol::Command::Stop: return UiAction::Stop;
    case HmiProtocol::Command::DismissResult: return UiAction::DismissResult;
    case HmiProtocol::Command::SelectProfile:
      return manualSelectionCommand ? UiAction::SelectManual : UiAction::SelectProfile;
    case HmiProtocol::Command::StartPressureCalibration:
      return UiAction::StartPressureCalibration;
    case HmiProtocol::Command::BeginYieldEdit: return UiAction::BeginYieldEdit;
    case HmiProtocol::Command::ApplyYieldEdit: return UiAction::ApplyYieldEdit;
    case HmiProtocol::Command::CancelYieldEdit: return UiAction::CancelYieldEdit;
    default: return UiAction::None;
  }
}

bool sendUiAction(UiAction action, uint32_t argument = 0) {
  switch (action) {
    case UiAction::StartBrew:
      return mainLink.sendCommand(HmiProtocol::Command::StartBrew);
    case UiAction::StartFlush:
      // Zero requests Main's default duration; HMI does not own the timer.
      return mainLink.sendCommand(HmiProtocol::Command::StartFlush, 0);
    case UiAction::SelectManual:
      if (!mainLink.compatible()) return false;
      for (uint8_t i = 0; i < mainLink.profileCount(); ++i) {
        const auto& profile = mainLink.profiles()[i];
        if (!(profile.flags & 1U)) continue;
        if (!mainLink.sendCommand(HmiProtocol::Command::SelectProfile, profile.index))
          return false;
        manualSelectionCommand = true;
        return true; // Selection only. Starting still requires StartBrew.
      }
      mainLink.requestProfiles();
      return false;
    case UiAction::Stop:
      return mainLink.sendCommand(HmiProtocol::Command::Stop);
    case UiAction::DismissResult:
      return mainLink.sendCommand(HmiProtocol::Command::DismissResult);
    case UiAction::SelectProfile:
      if (!mainLink.sendCommand(HmiProtocol::Command::SelectProfile, argument))
        return false;
      manualSelectionCommand = false;
      return true;
    case UiAction::StartPressureCalibration:
      return mainLink.sendCommand(HmiProtocol::Command::StartPressureCalibration);
    default:
      return false;
  }
}

void updateStorageStatus(uint32_t nowMs) {
  static const char* observedStatus=nullptr;
  const char* currentStatus=controlTraceStore.error();
  if(currentStatus!=observedStatus){
    observedStatus=currentStatus;saveStatus=currentStatus;saveStatusUntilMs=nowMs+2500;
  }
  telemetry.storageReady = controlTraceStore.ready();
  telemetry.recentShots = controlTraceStore.recent();
  telemetry.recentShotCount =
      static_cast<uint8_t>(controlTraceStore.recentCount());
  telemetry.selectedShot = hasSelectedShot ? &selectedShot : nullptr;
  telemetry.lastResult = lastResult.available ? &lastResult.summary : nullptr;
  telemetry.comparisonShot = hasComparisonShot ? &comparisonShot : nullptr;
  telemetry.saveStatus =
      saveStatusUntilMs != 0 &&
              static_cast<int32_t>(saveStatusUntilMs - nowMs) > 0
          ? saveStatus
          : nullptr;
  telemetry.storageTotalBytes = controlTraceStore.totalBytes();
  telemetry.storageUsedBytes = controlTraceStore.usedBytes();
}

void updateProfileStatus() {
  telemetry.profiles = mainLink.profiles();
  telemetry.profileCount = mainLink.profileCount();
  telemetry.profileTransferStatus = mainLink.profileTransferStatus();
  telemetry.profileTransferStatusAgeMs = mainLink.profileTransferStateAgeMs();
  // Standby needs the catalogue before opening Profiles (Manual lookup and
  // OFF/start gating). Refresh on startup or a Main-side selection change.
  bool selectionMatches = false;
  for (uint8_t i = 0; i < telemetry.profileCount; ++i) {
    const auto& profile = telemetry.profiles[i];
    if (profile.index == telemetry.profiles[0].currentIndex &&
        strcmp(profile.name, telemetry.profileName) == 0) selectionMatches = true;
  }
  static uint32_t lastReadyProfileRequestMs = 0;
  const uint32_t nowMs = millis();
  if (!selectionMatches &&
      (screens.screen() == ScreenId::Ready || screens.screen() == ScreenId::Brew) &&
      mainLink.compatible() && mainLink.commandState() != MainCommandState::Pending &&
      nowMs - lastReadyProfileRequestMs >= 1000) {
    lastReadyProfileRequestMs = nowMs;
    mainLink.requestProfiles();
  }
}

void updateCalibrationStatus(uint32_t nowMs) {
  const uint32_t generation = mainLink.calibrationReplyGeneration();
  if (generation != 0) {
    const auto& reply = mainLink.calibrationReply();
    telemetry.calibrationAvailable = true;
    telemetry.calibrationStatus = reply.status;
    telemetry.calibrationOperation = reply.operation;
    telemetry.pressureOffsetVolts = reply.pressureOffset;
    telemetry.pressureScaleBarPerVolt = reply.pressureScale;
    telemetry.pressureAdcVolts = reply.pressureAdcVolts;
    if (generation != seenCalibrationReplyGeneration) {
      seenCalibrationReplyGeneration = generation;
      screens.applyPressureCalibrationReply(reply.operation, reply.status);
      screens.applyThermalCalibrationReply(reply);
    }
  }

  if ((screens.screen() == ScreenId::PressureCal || screens.screen() == ScreenId::ThermalCal) && mainLink.compatible() &&
      nowMs - lastCalibrationReadMs >= 800) {
    lastCalibrationReadMs = nowMs;
    mainLink.requestCalibration(HmiProtocol::CalibrationOperation::ReadAll);
  }
}

void applyProfileTransfers() {
  const uint32_t generation = mainLink.profileDetailGeneration();
  if(mainLink.webTransaction() || mainLink.profileRequestFromWeb()) {
    seenProfileDetailGeneration=generation;
    seenProfileTransferResultId=mainLink.profileTransferResult().transferId;
    return;
  }
  if (generation != seenProfileDetailGeneration) {
    seenProfileDetailGeneration = generation;
    screens.loadProfileDraft(mainLink.profileDetail());
  }
  const auto& result = mainLink.profileTransferResult();
  if (result.transferId != 0 && result.transferId != seenProfileTransferResultId) {
    seenProfileTransferResultId = result.transferId;
    if (result.action == HmiProtocol::ProfileTransferAction::Upload &&
        result.status == HmiProtocol::ProfileTransferStatus::Accepted) {
      screens.markProfileSynced(result.profileIndex);
      mainLink.requestProfiles();
    }
  }
}

void updateLocalStatus(uint32_t nowMs) {
  const HmiSettingsData& data = settings.data();
  telemetry.brightness = data.brightness;
  telemetry.volume = data.volume;
  telemetry.autoDimMinutes = data.autoDimMinutes;
  telemetry.touchSounds = data.touchSounds;
  telemetry.autoSaveShots = data.autoSaveShots;
  telemetry.touchReady = touchHardwareReady;
  telemetry.audioReady = audioHardwareReady;
  telemetry.wifiConnected = HmiOta::wifiConnected();
  telemetry.wifiConnecting = HmiOta::wifiConnecting();
  telemetry.wifiAutoConnect = HmiOta::autoConnectEnabled();
  telemetry.wifiScanning = HmiOta::scanRunning();
  const String currentSsid = HmiOta::ssid();
  const String currentIp = HmiOta::localIp();
  snprintf(wifiSsid, sizeof(wifiSsid), "%s",
           currentSsid.isEmpty() ? "-" : currentSsid.c_str());
  snprintf(wifiIp, sizeof(wifiIp), "%s", currentIp.c_str());
  snprintf(wifiApIp, sizeof(wifiApIp), "%s", HmiOta::apIp().c_str());
  snprintf(wifiApName, sizeof(wifiApName), "%s", HmiOta::apName().c_str());
  telemetry.wifiApIp = wifiApIp;
  telemetry.wifiApName = wifiApName;
  telemetry.wifiSsid = wifiSsid;
  telemetry.wifiStatus = HmiOta::wifiStatusText();
  telemetry.wifiIp = wifiIp;
  telemetry.wifiRssi = HmiOta::rssi();
  telemetry.wifiNetworks = HmiOta::networks();
  telemetry.wifiNetworkCount = HmiOta::networkCount();
  telemetry.freeHeapBytes = ESP.getFreeHeap();
  telemetry.freePsramBytes = ESP.getFreePsram();
  telemetry.uptimeSeconds = nowMs / 1000;
  telemetry.mainConnected = mainLink.connected();
  telemetry.mainCompatible = mainLink.compatible();
  telemetry.mainSchemaRevision = mainLink.remoteSchemaRevision();
  telemetry.mainFrameAgeMs = mainLink.lastFrameAgeMs();
  telemetry.uartValidFrames = mainLink.validFrameCount();
  telemetry.uartCrcErrors = mainLink.crcErrorCount();
  telemetry.uartFramingErrors = mainLink.framingErrorCount();
  telemetry.uartStaleResets = mainLink.staleFrameResetCount();
  telemetry.uartReconnects = mainLink.reconnectCount();
  telemetry.uartLastErrorAgeMs = mainLink.lastErrorAgeMs();
  telemetry.scaleSimulatorEnabled = scaleManager.simulatorEnabled();
}

void applyAutoDim(uint32_t nowMs) {
  const uint8_t minutes = settings.data().autoDimMinutes;
  const bool operating = screens.screen() == ScreenId::Brew ||
                         screens.screen() == ScreenId::Flush;
  if (!displayDimmed && !operating && minutes > 0 &&
      nowMs - lastInteractionMs >= static_cast<uint32_t>(minutes) * 60000U) {
    display.setBacklight(min<uint8_t>(15, settings.data().brightness));
    displayDimmed = true;
  }
  if (operating && displayDimmed) {
    display.setBacklight(settings.data().brightness);
    displayDimmed = false;
    lastInteractionMs = nowMs;
  }
}

void updateCommandStatus(uint32_t nowMs) {
  telemetry.commandAction = UiAction::None;
  telemetry.commandStatus = nullptr;
  telemetry.commandPending = mainLink.commandState() == MainCommandState::Pending;
  if (localErrorUntilMs != 0 && static_cast<int32_t>(localErrorUntilMs - nowMs) > 0) {
    telemetry.commandAction = localErrorAction;
    telemetry.commandStatus = mainLink.connected() ? "BUSY" : "OFFLINE";
    return;
  }

  const MainCommandState state = mainLink.commandState();
  telemetry.commandAction = uiActionForCommand(mainLink.lastCommand());
  if (state == MainCommandState::Pending) {
    telemetry.commandStatus = "SENDING...";
  } else if (mainLink.commandStateAgeMs() < 1800) {
    if (state == MainCommandState::Accepted) {
      telemetry.commandStatus = "ACCEPTED";
    } else if (state == MainCommandState::TimedOut) {
      telemetry.commandStatus = "NO REPLY";
    } else if (state == MainCommandState::Rejected) {
      telemetry.commandStatus =
          mainLink.commandResult() == HmiProtocol::CommandResult::RejectedState
              ? "NOT READY"
              : "REJECTED";
    }
  }
}

void applyMainTelemetry() {
  HmiProtocol::TelemetryPayload packet;
  if (!mainLink.copyTelemetry(packet)) return;

  telemetry.sourceTimestampMs = packet.timestampMs;
  telemetry.blockTempC=packet.blockTempC;
  telemetry.quickYieldG=packet.quickYieldG;
  telemetry.yieldEditToken=packet.yieldEditToken;
  telemetry.quickYieldFlags=packet.quickYieldFlags;
  telemetry.selectedProfileIndex=packet.selectedProfileIndex;
  telemetry.pressureBar = packet.pressureBar;
  telemetry.flowGps = packet.flowGps;
  telemetry.weightG = packet.weightG;
  telemetry.boilerTempC = packet.brewTempC;
  telemetry.elapsedS = packet.elapsedS;
  telemetry.pumpPercent = packet.pumpPercent;
  telemetry.machineState =
      static_cast<HmiProtocol::MachineState>(packet.machineState);
  telemetry.stepIndex = packet.stepIndex;
  telemetry.stepCount = packet.stepCount == 0 ? 1 : packet.stepCount;
  telemetry.mainConnected = mainLink.connected();
  telemetry.scaleConnected =
      (packet.flags & HmiProtocol::TelemetryFlags::ScaleConnected) != 0;
  telemetry.scaleStable =
      (packet.flags & HmiProtocol::TelemetryFlags::ScaleStable) != 0;
  telemetry.scaleSource = packet.scaleSource;
  telemetry.scaleBatteryPercent = packet.scaleBatteryPercent;
  telemetry.mainSeesHmi =
      (packet.flags & HmiProtocol::TelemetryFlags::HmiPeerConnected) != 0;
  telemetry.mainSeesHmiCompatible =
      (packet.flags & HmiProtocol::TelemetryFlags::HmiPeerCompatible) != 0;
  telemetry.targetPressureBar = packet.controlMode == 0 ? packet.targetValue : 0.0f;
  telemetry.manualMode = false;
  for (uint8_t i = 0; i < mainLink.profileCount(); ++i) {
    const auto& profile = mainLink.profiles()[i];
    if (profile.index == mainLink.profiles()[0].currentIndex &&
        strcmp(profile.name, packet.profileName) == 0)
      telemetry.manualMode = (profile.flags & 1U) != 0;
  }
  telemetry.pressureTargetValid = packet.controlMode == 0 &&
      packet.targetValue > 0.0f && !telemetry.manualMode &&
      strcmp(packet.phaseName, "SCALE PREP") != 0;
  telemetry.targetFlowGps = packet.controlMode == 1 ? packet.targetValue : 0.0f;
  telemetry.flushRemainingS =
      packet.machineState == static_cast<uint8_t>(HmiProtocol::MachineState::Flush)
          ? fmaxf(0.0f, packet.targetValue - packet.elapsedS)
          : 0.0f;
  telemetry.flushDurationS =
      telemetry.machineState == HmiProtocol::MachineState::Flush &&
              isfinite(packet.targetValue) && packet.targetValue > 0
          ? packet.targetValue : 0.0f;

  snprintf(profileName, sizeof(profileName), "%s", packet.profileName);
  snprintf(phaseName, sizeof(phaseName), "%s", packet.phaseName);
  telemetry.profileName = profileName;
  telemetry.phaseName = phaseName;

  const auto state = static_cast<HmiProtocol::MachineState>(packet.machineState);
  // Do not close from Done telemetry: ShotEnd is the authoritative stop
  // sample. Missing events are bounded by the store's existing 2 s timeout.
  previousMachineState = state;
  const ScreenId desired = screenForState(state);
  if (desired == ScreenId::ShotResult &&
      (screens.screen() == ScreenId::Compare ||
       screens.screen() == ScreenId::History ||
       screens.screen() == ScreenId::ShotDetail ||
       screens.screen() == ScreenId::SavedCompare ||
       isProfileEditorScreen(screens.screen()))) {
    return;
  }
  if (desired == ScreenId::Brew || desired == ScreenId::Flush ||
      desired == ScreenId::ShotResult || isOperatingScreen(screens.screen())) {
    screens.show(desired);
  }
}
}

void setup() {
  Serial.begin(115200);
  delay(100);

  settings.begin();
  display.beginPanel();
  display.setBacklight(settings.data().brightness);
  touchHardwareReady = touch.begin();
  audioHardwareReady = audio.begin(settings.data().volume);
  const bool spriteReady = screens.begin();
  mainLink.begin();
  scaleManager.begin();
  demo.begin(millis());
  // Start station association and the HTTP worker before touching the TF card.
  // SD mount/index latency must not postpone DHCP or the first static page.
  MainFirmwareUpdater::begin(mainLink);
  HmiOta::attachDataSources(mainLink, shotStore, controlTraceStore, settings,
                            display, audio, scaleManager);
  HmiOta::begin();
  const uint32_t networkStartedAtMs = millis();

  const uint32_t storageStartedAtMs = millis();
  shotStore.begin();
  controlTraceStore.attachLegacy(shotStore);
  controlTraceStore.begin();
  lastInteractionMs = millis();

  Serial.printf("[HMI] touch=%s sprite=%s psram=%u bytes\n",
                touchHardwareReady ? "OK" : "FAIL", spriteReady ? "OK" : "FAIL",
                ESP.getPsramSize());
  Serial.printf("[HMI boot] network-start=%lu ms storage=%lu ms setup-ready=%lu ms\n",
                static_cast<unsigned long>(networkStartedAtMs),
                static_cast<unsigned long>(millis() - storageStartedAtMs),
                static_cast<unsigned long>(millis()));
}

void loop() {
  HmiLoopProbe loopProbe;
  const uint32_t nowMs = millis();
  scaleManager.poll(nowMs);
  loopProbe.stage(0);
  mainLink.poll();
  loopProbe.stage(1);
  HmiProtocol::TelemetryPayload currentPacket{};
  if (mainLink.copyTelemetry(currentPacket)) {
    const auto state = static_cast<HmiProtocol::MachineState>(currentPacket.machineState);
    if ((state == HmiProtocol::MachineState::Brew ||
         state == HmiProtocol::MachineState::HotWater) && state != previousMachineState)
      lastResult.clear(); // Even a shot with no trace must not show the old result.
  }
  HmiProtocol::ScaleTareRequestPayload tareRequest{};
  if (mainLink.popScaleTareRequest(tareRequest))
    scaleManager.requestShotTare(tareRequest);
  HmiProtocol::ControlTracePayload controlTrace{};
  while (mainLink.popControlTrace(controlTrace)) {
    HmiProtocol::TelemetryPayload latest{};
    const bool hasTelemetry = mainLink.copyTelemetry(latest);
    const char* traceProfile = hasTelemetry ? latest.profileName : telemetry.profileName;
    const char* tracePhase = hasTelemetry && latest.stepIndex == controlTrace.stepIndex &&
        fabsf(latest.elapsedS - controlTrace.elapsedS) < 1.0f
        ? latest.phaseName : nullptr;
    lastResult.observe(controlTrace, traceProfile);
    if (settings.data().autoSaveShots)
      controlTraceStore.append(controlTrace, traceProfile, tracePhase);
  }
  HmiProtocol::ShotEndPayload shotEnd{};
  if (mainLink.popShotEnd(shotEnd)) {
    lastResult.finish(shotEnd);
    const bool hadTrace = controlTraceStore.active();
    if (settings.data().autoSaveShots) controlTraceStore.finish(shotEnd);
    if (hadTrace) {
      saveStatus = controlTraceStore.error();
      saveStatusUntilMs = nowMs + 2500;
    }
  }
  controlTraceStore.poll(nowMs);
  const uint32_t scaleTxPeriodMs = scaleManager.connected() ? 50U : 250U;
  if (mainLink.compatible() && nowMs - lastScaleTxMs >= scaleTxPeriodMs) {
    lastScaleTxMs = nowMs;
    mainLink.sendScaleSample(scaleManager.packet(nowMs));
  }
  MainFirmwareUpdater::poll();
  applyProfileTransfers();
  loopProbe.stage(2);
  HmiOta::poll();
  loopProbe.stage(3);

#if HMI_DEMO_MODE
  if (isOperatingScreen(screens.screen())) {
    const ScreenId demoScreen = demo.update(nowMs, telemetry);
    screens.show(demoScreen);
  }
  // UI data remains simulated, but link status always comes from real UART.
  telemetry.mainConnected = mainLink.connected();
#else
  applyMainTelemetry();
  telemetry.mainConnected = mainLink.connected();
  // Live scale status on HMI must expire even while Main telemetry is offline.
  telemetry.scaleConnected = scaleManager.connected();
  if (telemetry.scaleConnected) {
    telemetry.scaleConnected = true;
    telemetry.scaleStable = scaleManager.stable();
    telemetry.weightG = scaleManager.weightG();
    telemetry.scaleSource = scaleManager.source();
    telemetry.scaleBatteryPercent = scaleManager.batteryPercent();
  }
  telemetry.scaleDeviceName = scaleManager.activeScaleName();
  telemetry.scaleDeviceMac = scaleManager.activeScaleMac();
  if (!scaleManager.connected() && scaleManager.savedMac() && scaleManager.savedMac()[0]) {
    telemetry.scaleDeviceMac = scaleManager.savedMac();
  }
  telemetry.scaleRssi = scaleManager.activeScaleRssi();
  telemetry.scaleScanning = scaleManager.isScanning();
  telemetry.scaleDiscoveredDevices = scaleManager.discoveredScales();
  telemetry.scaleDiscoveredCount = scaleManager.discoveredCount();
#endif



  TouchPoint point;
  const bool pressed = touch.read(point);
  bool consumeWakeTouch = false;
  if (pressed && !touchWasPressed) {
    if (displayDimmed) {
      display.setBacklight(settings.data().brightness);
      displayDimmed = false;
      consumeWakeTouch = true;
    }
    lastInteractionMs = nowMs;
    if (!consumeWakeTouch && settings.data().touchSounds) audio.touchClick();
  }
#if HMI_DEMO_MODE
  if (pressed && !touchWasPressed && !consumeWakeTouch) {
    const ScreenId before = screens.screen();
    const ScreenId after = screens.handleTap(point.x, point.y, nowMs);
    if (after != before && isOperatingScreen(after)) {
      demo.setState(after, nowMs);
      demo.update(nowMs, telemetry);
    }
  }
#else
  if (pressed && !touchWasPressed && !consumeWakeTouch) {
    const UiAction action = screens.actionForTap(point.x, point.y);
    if(action==UiAction::HeaterOff || action==UiAction::HeaterEnable ||
       action==UiAction::HeaterClear || action==UiAction::HeaterSave) {
      HeaterWire::Request request;
      request.ticket=esp_random();if(!request.ticket)request.ticket=1;
      request.op=action==UiAction::HeaterOff?HeaterWire::Op::Off:
        action==UiAction::HeaterEnable?HeaterWire::Op::Enable:
        action==UiAction::HeaterClear?HeaterWire::Op::ClearFault:HeaterWire::Op::Configure;
      request.config=screens.heaterDraft();
      const bool allowed=request.op==HeaterWire::Op::Off ||
        (mainLink.heaterFresh() && !mainLink.heaterCommandPending() &&
         telemetry.machineState==HmiProtocol::MachineState::Ready &&
         (request.op!=HeaterWire::Op::Configure || screens.heaterDraftValid()));
      const bool sent=allowed && mainLink.requestHeater(request);
      screens.heaterRequestSent(request,sent,nowMs);
    } else if(action==UiAction::BeginYieldEdit) {
      uint32_t token=esp_random();if(!token)token=1;
      if(mainLink.sendCommand(HmiProtocol::Command::BeginYieldEdit,token)){
        quickYieldToken=token;quickYieldStartedMs=quickYieldRenewMs=nowMs;
        quickYieldConfirmed=quickYieldClosing=false;
        screens.show(ScreenId::YieldEditor);
      }
    } else if(action==UiAction::ApplyYieldEdit || action==UiAction::CancelYieldEdit) {
      if(quickYieldToken && mainLink.sendCommand(action==UiAction::ApplyYieldEdit?
          HmiProtocol::Command::ApplyYieldEdit:HmiProtocol::Command::CancelYieldEdit,quickYieldToken))
        quickYieldClosing=true;
    } else if (action == UiAction::SaveShot) {
      saveStatus = settings.data().autoSaveShots
                       ? (controlTraceStore.latestId() > 0 ? "SAVED"
                                                           : "NO TRACE")
                       : "AUTO SAVE OFF";
      saveStatusUntilMs = nowMs + 2500;
    } else if (action == UiAction::ShowHistory) {
      controlTraceStore.begin();
      screens.show(ScreenId::History);
    } else if (action == UiAction::LoadHistoryShot) {
      const int index = screens.historyIndexForTap(point.x, point.y);
      if (index >= 0 && index < telemetry.recentShotCount) {
        selectedHistoryIndex = index;
        selectedShot = telemetry.recentShots[index];
        hasSelectedShot = true; // Keep the saved summary even if its CSV is missing.
        screens.historyChart().reset();
        controlTraceStore.load(selectedShot.id, screens.historyChart());
        hasComparisonShot = false;
        screens.show(ScreenId::ShotDetail);
      }
    } else if (action == UiAction::CompareHistoryShot) {
      hasComparisonShot = false;
      if (hasSelectedShot && telemetry.recentShotCount > 1) {
        int referenceIndex = selectedHistoryIndex + 1;
        if (referenceIndex >= telemetry.recentShotCount)
          referenceIndex = selectedHistoryIndex - 1;
        if (referenceIndex >= 0) {
          comparisonShot = telemetry.recentShots[referenceIndex];
          hasComparisonShot = true;
        }
      }
      screens.show(ScreenId::SavedCompare);
    } else if (action == UiAction::ShowCompare) {
      controlTraceStore.begin();
      screens.show(ScreenId::Compare);
    } else if (action == UiAction::OpenProfile) {
      const int profileIndex = screens.profileIndexForTap(point.x, point.y);
      if (profileIndex >= 2) {
        screens.beginProfileLoad(static_cast<uint8_t>(profileIndex));
        if (!mainLink.requestProfileDetail(static_cast<uint8_t>(profileIndex))) {
          localErrorAction = action;
          localErrorUntilMs = nowMs + 1500;
        }
      } else if (profileIndex >= 0 &&
                 !sendUiAction(UiAction::SelectProfile,
                               static_cast<uint32_t>(profileIndex))) {
        localErrorAction = UiAction::SelectProfile;
        localErrorUntilMs = nowMs + 1500;
      }
    } else if (action == UiAction::SelectProfile) {
      const int profileIndex = screens.selectedProfileIndex();
      if (profileIndex < 0 || profileIndex >= telemetry.profileCount ||
          !sendUiAction(action, static_cast<uint32_t>(profileIndex))) {
        localErrorAction = action;
        localErrorUntilMs = nowMs + 1500;
      }
    } else if (action == UiAction::UploadProfile) {
      if (!screens.profileDraftValid() ||
          !mainLink.uploadProfile(screens.profileDraft())) {
        localErrorAction = action;
        localErrorUntilMs = nowMs + 1500;
      }
    } else if (action == UiAction::RescanWifi) {
      HmiOta::scanNetworks();
    } else if (action == UiAction::ConnectWifi) {
      HmiOta::connectWifi(screens.wifiSsid(), screens.wifiPassword());
      screens.show(ScreenId::Wifi);
    } else if (action == UiAction::ForgetWifi) {
      HmiOta::forgetWifi();
    } else if (action == UiAction::ToggleWifiAutoConnect) {
      HmiOta::setAutoConnect(!HmiOta::autoConnectEnabled());
    } else if (action == UiAction::ToggleTouchSounds) {
      settings.setTouchSounds(!settings.data().touchSounds);
    } else if (action == UiAction::TestSound) {
      audio.testTone();
    } else if (action == UiAction::ChangeBrightness) {
      uint8_t value = point.x >= 135 ? screens.sliderValueForTap(point.x)
                                     : settings.data().brightness + 10;
      if (value > 100) value = 10;
      settings.setBrightness(value);
      display.setBacklight(value);
    } else if (action == UiAction::ChangeVolume) {
      uint8_t value = point.x >= 135 ? screens.sliderValueForTap(point.x)
                                     : settings.data().volume + 10;
      if (value > 100) value = 0;
      settings.setVolume(value);
      audio.setVolume(value);
    } else if (action == UiAction::ChangeAutoDim) {
      const uint8_t options[] = {0, 1, 2, 5, 10};
      uint8_t next = options[0];
      for (size_t i = 0; i < sizeof(options); ++i) {
        if (options[i] == settings.data().autoDimMinutes) {
          next = options[(i + 1) % sizeof(options)];
          break;
        }
      }
      settings.setAutoDimMinutes(next);
    } else if (action == UiAction::ToggleAutoSave) {
      settings.setAutoSaveShots(!settings.data().autoSaveShots);
    } else if (action == UiAction::ScanScale) {
      scaleManager.startScan();
      screens.show(ScreenId::ScaleDevices);
    } else if (action == UiAction::RescanScales) {
      scaleManager.startScan();
    } else if (action == UiAction::PairScale) {
      const int selected = screens.selectedScaleDevice();
      if (selected >= 0 && selected < scaleManager.discoveredCount()) {
        const auto& dev = scaleManager.discoveredScales()[selected];
        scaleManager.pairWithMac(dev.mac);
        screens.show(ScreenId::Scale);
      }
    } else if (action == UiAction::UnpairScale) {
      scaleManager.unpair();
    } else if (action == UiAction::SaveThermalOffset) {
      const bool sent = mainLink.requestCalibration(HmiProtocol::CalibrationOperation::SetThermalOffset,
          screens.thermalOffsetDraft(), screens.thermalOffsetBase());
      screens.markThermalCalibrationRequest(sent ? mainLink.calibrationRequestId() : 0, nowMs);
    } else if (action == UiAction::ZeroPressure ||
               action == UiAction::SetPressureAt) {
      const auto operation = action == UiAction::ZeroPressure
                                 ? HmiProtocol::CalibrationOperation::ZeroPressure
                                 : HmiProtocol::CalibrationOperation::SetPressureAt;
      const float referenceBar =
          action == UiAction::SetPressureAt
              ? screens.pressureCalibrationReferenceBar()
              : 0.0f;
      if (!mainLink.requestCalibration(operation, referenceBar)) {
        localErrorAction = action;
        localErrorUntilMs = nowMs + 1500;
      } else {
        screens.markPressureCalibrationRequest(action);
      }
    } else if (action == UiAction::EraseHistory) {

      const bool queuedErase = controlTraceStore.eraseAll();
      saveStatus = queuedErase ? "ERASING..." : "ERASE QUEUE FULL";
      saveStatusUntilMs = nowMs + 2500;
      hasSelectedShot = false;
      hasComparisonShot = false;
      selectedHistoryIndex = -1;
      screens.show(ScreenId::StorageLogs);
    }
 else if (action != UiAction::None) {
      if (!sendUiAction(action)) {
        localErrorAction = action;
        localErrorUntilMs = nowMs + 1500;
      }
    } else {
      const ScreenId before = screens.screen();
      const ScreenId after = screens.handleTap(point.x, point.y, nowMs);
      if (after == ScreenId::Profiles && before != ScreenId::Profiles)
        mainLink.requestProfiles();
      if (after == ScreenId::Networks && before != ScreenId::Networks)
        HmiOta::scanNetworks();
    }
  }
#endif
  touchWasPressed = pressed;

  if(quickYieldToken) {
    const bool matched=telemetry.yieldEditToken==quickYieldToken;
    if(matched)quickYieldConfirmed=true;
    const auto status=mainLink.commandState();
    const bool expired=!mainLink.connected() || !mainLink.compatible() ||
        telemetry.machineState!=HmiProtocol::MachineState::Ready ||
        (quickYieldConfirmed && !matched) ||
        (!quickYieldConfirmed && nowMs-quickYieldStartedMs>2200);
    const bool failed=status==MainCommandState::Rejected || status==MainCommandState::TimedOut;
    const bool closed=quickYieldClosing && status==MainCommandState::Accepted;
    if(expired || failed || closed) {
      if(expired && mainLink.compatible() && status!=MainCommandState::Pending)
        mainLink.sendCommand(HmiProtocol::Command::CancelYieldEdit,quickYieldToken);
      quickYieldToken=0;
      if(screens.screen()==ScreenId::YieldEditor)screens.show(ScreenId::Ready);
    } else if(!quickYieldClosing && nowMs-quickYieldRenewMs>=600 &&
              status!=MainCommandState::Pending) {
      if(mainLink.sendCommand(HmiProtocol::Command::KeepYieldEdit,quickYieldToken))quickYieldRenewMs=nowMs;
    }
  }

  updateCommandStatus(nowMs);
  updateStorageStatus(nowMs);
  updateProfileStatus();
  updateCalibrationStatus(nowMs);
  updateLocalStatus(nowMs);
  // Heater state belongs to Main; UI never decides readiness from temperature.
  telemetry.heaterFresh = mainLink.heaterFresh();
  telemetry.heaterStatus = mainLink.heaterStatus();
  telemetry.heaterPending = mainLink.heaterCommandPending();
  telemetry.heaterReady = telemetry.heaterFresh && mainLink.heaterStatus().ready;
  telemetry.heaterMode = static_cast<uint8_t>(mainLink.heaterStatus().mode);
  telemetry.heaterAlarm = mainLink.heaterSeen() && (!telemetry.heaterFresh ||
      mainLink.heaterStatus().fault != HeaterWire::Fault::None);
  telemetry.heaterPaused = telemetry.heaterFresh && !telemetry.heaterAlarm &&
      (telemetry.heaterMode == 1 || telemetry.heaterMode == 2) &&
      mainLink.heaterSensorFresh() && mainLink.heaterSensorStatus().paused;
  loopProbe.stage(4);
  screens.update(telemetry, nowMs);
  loopProbe.stage(5);
  applyAutoDim(nowMs);
  if (telemetry.heaterAlarm) {
    display.setBacklight(settings.data().brightness);
  }
  if (restartAtMs != 0 && static_cast<int32_t>(nowMs - restartAtMs) >= 0)
    ESP.restart();
  delay(2);
}
