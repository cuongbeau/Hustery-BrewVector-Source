#pragma once

#include <Arduino.h>

#include "model/shot_record.h"
#include "hmi_protocol.h"
#include "network/ota_service.h"
#include "heater_wire.h"

enum class ScreenId : uint8_t {
  Ready,
  Flush,
  Brew,
  ShotResult,
  Settings,
  Wifi,
  Networks,
  WifiKeyboard,
  ConfirmRestart,
  Scale,
  ScaleDevices,
  PressureCal,
  Machine,
  DisplaySound,
  StorageLogs,
  System,
  Diagnostics,
  Firmware,
  Profiles,
  ProfileDetail,
  ProfileQuick,
  ProfileSteps,
  ProfileStepControl,
  ProfileStepSafety,
  ProfileStepExit,
  ProfilePreview,
  ProfileNameKeyboard,
  ConfirmQuickConvert,
  History,
  ShotDetail,
  SavedCompare,
  ConfirmEraseHistory,
  Compare,
  Connections,
  WebAccess,
  YieldEditor,
  HeaterSettings,
  Calibration,
  ThermalCal,
};

enum class UiAction : uint8_t {
  None,
  StartBrew,
  StartFlush,
  Stop,
  DismissResult,
  SaveShot,
  ShowHistory,
  ShowCompare,
  OpenProfile,
  SelectProfile,
  UploadProfile,
  RescanWifi,
  ConnectWifi,
  ForgetWifi,
  ToggleWifiAutoConnect,
  ToggleTouchSounds,
  TestSound,
  ChangeBrightness,
  ChangeVolume,
  ChangeAutoDim,
  ToggleAutoSave,
  RestartHmi,
  LoadHistoryShot,
  CompareHistoryShot,
  EraseHistory,
  TareScale,
  ScanScale,
  RescanScales,
  PairScale,
  UnpairScale,
  ZeroPressure,
  SetPressureAt,
  StartPressureCalibration,
  SelectManual,
  BeginYieldEdit,
  ApplyYieldEdit,
  CancelYieldEdit,
  HeaterOff, HeaterEnable, HeaterClear, HeaterSave,
  SaveThermalOffset,
};

struct BleDiscoveredScale {
  char name[32];
  char mac[18];
  int rssi;
  HmiProtocol::ScaleSource sourceType;
};

struct UiTelemetry {

  uint32_t sourceTimestampMs = 0;
  float boilerTempC = 93.0f;
  float pressureBar = 0.0f;
  float flowGps = 0.0f;
  float weightG = 0.0f;
  float elapsedS = 0.0f;
  float targetPressureBar = 9.0f;
  bool pressureTargetValid = false;
  bool manualMode = false;
  float targetFlowGps = 2.2f;
  float flushRemainingS = 5.0f;
  float flushDurationS = 5.0f;
  float pumpPercent = 0.0f;
  HmiProtocol::MachineState machineState = HmiProtocol::MachineState::Boot;
  uint8_t stepIndex = 0;
  uint8_t stepCount = 5;
  bool mainConnected = false;
  bool heaterFresh = false, heaterReady = false;
  uint8_t heaterMode = 0;
  bool heaterAlarm = false;
  bool heaterPaused = false;
  HeaterWire::Status heaterStatus{};
  bool heaterPending = false;
  float blockTempC = NAN;
  float quickYieldG = 0;
  uint32_t yieldEditToken = 0;
  uint8_t quickYieldFlags = 0;
  uint8_t selectedProfileIndex = 0;
  bool mainCompatible = false;
  bool mainSeesHmi = false;
  bool mainSeesHmiCompatible = false;
  uint16_t mainSchemaRevision = 0;
  uint32_t mainFrameAgeMs = UINT32_MAX;
  uint32_t uartValidFrames = 0;
  uint32_t uartCrcErrors = 0;
  uint32_t uartFramingErrors = 0;
  uint32_t uartStaleResets = 0;
  uint32_t uartReconnects = 0;
  uint32_t uartLastErrorAgeMs = UINT32_MAX;
  bool calibrationAvailable = false;
  HmiProtocol::CalibrationStatus calibrationStatus =
      HmiProtocol::CalibrationStatus::Accepted;
  HmiProtocol::CalibrationOperation calibrationOperation =
      HmiProtocol::CalibrationOperation::ReadAll;
  float pressureOffsetVolts = 0.0f;
  float pressureScaleBarPerVolt = 0.0f;
  float pressureAdcVolts = NAN;
  bool scaleConnected = true;
  bool scaleStable = true;
  HmiProtocol::ScaleSource scaleSource = HmiProtocol::ScaleSource::None;
  uint8_t scaleBatteryPercent = 0xFF;
  const char* scaleDeviceName = "--";
  const char* scaleDeviceMac = "--";
  int scaleRssi = 0;
  bool scaleScanning = false;
  const BleDiscoveredScale* scaleDiscoveredDevices = nullptr;
  uint8_t scaleDiscoveredCount = 0;
  bool scaleSimulatorEnabled = false;

  UiAction commandAction = UiAction::None;
  const char* commandStatus = nullptr;
  bool commandPending = false;
  const char* saveStatus = nullptr;
  bool storageReady = false;
  const ShotSummary* recentShots = nullptr;
  uint8_t recentShotCount = 0;
  const ShotSummary* selectedShot = nullptr;
  const ShotSummary* lastResult = nullptr;
  const ShotSummary* comparisonShot = nullptr;
  const HmiProtocol::ProfileInfoPayload* profiles = nullptr;
  uint8_t profileCount = 0;
  HmiProtocol::ProfileTransferStatus profileTransferStatus =
      HmiProtocol::ProfileTransferStatus::Pending;
  uint32_t profileTransferStatusAgeMs = UINT32_MAX;
  bool wifiConnected = false;
  bool wifiConnecting = false;
  bool wifiAutoConnect = true;
  bool wifiScanning = false;
  const char* wifiSsid = "-";
  const char* wifiStatus = "NOT CONNECTED";
  const char* wifiIp = "0.0.0.0";
  const char* wifiApIp = "";
  const char* wifiApName = "";
  int32_t wifiRssi = -127;
  const HmiOta::NetworkInfo* wifiNetworks = nullptr;
  uint8_t wifiNetworkCount = 0;
  uint8_t brightness = 75;
  uint8_t volume = 60;
  uint8_t autoDimMinutes = 2;
  bool touchSounds = true;
  bool autoSaveShots = true;
  bool touchReady = false;
  bool audioReady = false;
  uint64_t storageTotalBytes = 0;
  uint64_t storageUsedBytes = 0;
  uint32_t freeHeapBytes = 0;
  uint32_t freePsramBytes = 0;
  uint32_t uptimeSeconds = 0;
  const char* profileName = "LEVER 9 BAR";
  const char* phaseName = "DECLINE";
};
