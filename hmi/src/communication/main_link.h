#pragma once
#include "model/brew_stream.h"
#include "heater_sensor_wire.h"

#include <Arduino.h>
#include "hmi_protocol.h"
#include "pressure_capture.h"
#include "adc_probe.h"
#include "heater_wire.h"
#include "model/profile_draft.h"

enum class MainCommandState : uint8_t {
  Idle,
  Pending,
  Accepted,
  Rejected,
  TimedOut,
};

class MainLink {
 public:
  void begin();
  bool supportsHeater() const { return compatible() && (mainFeatureFlags_ & HmiProtocol::SupportsHeater); }
  bool heaterFresh() const { return supportsHeater() && heaterSeen_ && millis() - heaterReceivedMs_ < 2000; }
  bool heaterCommandPending() const { return heaterPendingTicket_ != 0 && heaterStatus_.ticket != heaterPendingTicket_; }
  bool heaterSeen() const { return heaterSeen_; }
  const HeaterWire::Status& heaterStatus() const { return heaterStatus_; }
  const HeaterSensorWire::Status& heaterSensorStatus() const { return heaterSensorStatus_; }
  bool heaterSensorFresh() const { return heaterFresh() && heaterSensorSeen_ && millis() - heaterSensorReceivedMs_ < 2000; }
  bool requestHeater(const HeaterWire::Request& request);
  void poll();
  bool connected() const;
  bool compatible() const;
  uint16_t remoteSchemaRevision() const { return mainSchemaRevision_; }
  bool supportsControlTrace() const {
    return (mainFeatureFlags_ &
            HmiProtocol::HeartbeatFlags::SupportsControlTrace) != 0;
  }
  uint32_t lastFrameAgeMs() const;
  const BrewStream& brewStream()const{return brewStream_;}
  uint32_t streamBoot()const{return streamBoot_;}
  uint32_t validFrameCount() const { return validFrameCount_; }
  uint32_t crcErrorCount() const { return crcErrorCount_; }
  uint32_t framingErrorCount() const { return framingErrorCount_; }
  uint32_t staleFrameResetCount() const { return staleFrameResetCount_; }
  uint32_t reconnectCount() const { return reconnectCount_; }
  uint32_t rxOverflowCount() const { return rxOverflowCount_; }
  bool beginWebTransaction();
  bool endWebTransaction() {
    webTransaction_ = false;
    profileRequestFromWeb_ = false;
    return true;
  }
  bool webTransaction() const { return webTransaction_; }
  bool profileRequestFromWeb() const { return profileRequestFromWeb_; }
  uint32_t profileListGeneration() const { return profileListGeneration_; }
  uint16_t profileRequestId() const { return profileTransferId_; }
  uint32_t lastErrorAgeMs() const;
  bool copyTelemetry(HmiProtocol::TelemetryPayload& telemetry) const;
  bool popControlTrace(HmiProtocol::ControlTracePayload& trace);
  bool popShotEnd(HmiProtocol::ShotEndPayload& shotEnd);
  uint32_t controlTraceQueueDropCount() const { return controlTraceQueueDrops_; }
  bool popScaleTareRequest(HmiProtocol::ScaleTareRequestPayload& request);
  bool sendScaleSample(const HmiProtocol::ScaleSamplePayload& sample);
  bool sendCommand(HmiProtocol::Command command, uint32_t argument = 0);
  bool requestProfiles();
  bool supportsAdcProbe() const { return compatible() && (mainFeatureFlags_ & HmiProtocol::SupportsAdcProbe); }
  bool requestAdcProbe(const AdcProbeWire::Request& request);
  const AdcProbeWire::Reply& adcProbeReply() const { return adcProbeReply_; }
  bool supportsPressureCapture() const {
    return compatible() && (mainFeatureFlags_ & HmiProtocol::SupportsPressureCapture);
  }
  bool requestPressureCapture(const PressureCaptureWire::Request& request);
  const PressureCaptureWire::Reply& pressureCaptureReply() const { return pressureCaptureReply_; }
  bool requestProfileDetail(uint8_t profileIndex);
  bool uploadProfile(const ProfileDetailData& draft);
  bool deleteProfile(uint8_t profileIndex);
  bool requestCalibration(HmiProtocol::CalibrationOperation operation,
                          float value1 = 0.0f, float value2 = 0.0f,
                          float value3 = 0.0f);
  bool sendFirmwareBegin(const HmiProtocol::FirmwareBeginPayload& begin);
  bool sendFirmwareChunk(const HmiProtocol::FirmwareChunkPayload& chunk);
  bool sendFirmwareEnd(const HmiProtocol::FirmwareEndPayload& end);
  bool sendFirmwareAbort(uint32_t transferId);
  const HmiProtocol::FirmwareReplyPayload& firmwareReply() const {
    return firmwareReply_;
  }
  uint32_t firmwareReplyGeneration() const { return firmwareReplyGeneration_; }
  uint8_t profileCount() const { return profileCount_; }
  const HmiProtocol::ProfileInfoPayload* profiles() const { return profiles_; }
  MainCommandState commandState() const { return commandState_; }
  HmiProtocol::CommandResult commandResult() const { return commandResult_; }
  HmiProtocol::Command lastCommand() const { return pendingCommand_; }
  uint32_t commandStateAgeMs() const;
  const ProfileDetailData& profileDetail() const { return profileDetail_; }
  uint32_t profileDetailGeneration() const { return profileDetailGeneration_; }
  HmiProtocol::ProfileTransferStatus profileTransferStatus() const {
    return profileTransferStatus_;
  }
  const HmiProtocol::ProfileCommitPayload& profileTransferResult() const {
    return profileTransferResult_;
  }
  uint32_t profileTransferGeneration() const {
    return profileTransferGeneration_;
  }
  uint32_t profileTransferStateAgeMs() const;
  const HmiProtocol::CalibrationReplyPayload& calibrationReply() const {
    return calibrationReply_;
  }
  uint16_t calibrationRequestId() const { return calibrationRequestId_; }
  uint32_t calibrationReplyGeneration() const {
    return calibrationReplyGeneration_;
  }

 private:
  BrewStream brewStream_;
  uint32_t streamBoot_=0;
  static constexpr size_t MAX_PAYLOAD = 512;
  // A complete 8-step profile transfer is ~650 bytes. Keep one full burst so
  // parseFrames never has to discard the Begin frame while later chunks arrive.
  static constexpr size_t RX_CAPACITY = 2048;

  HmiProtocol::ScaleTareRequestPayload scaleTareRequest_{};
  bool hasScaleTareRequest_ = false;
  HardwareSerial serial_{1};
  uint32_t lastValidFrameMs_ = 0;
  uint32_t lastRxByteMs_ = 0;
  uint32_t lastHeartbeatTxMs_ = 0;
  uint16_t mainSchemaRevision_ = 0;
  uint8_t mainFeatureFlags_ = 0;
  uint16_t txSequence_ = 0;
  uint8_t rxBuffer_[RX_CAPACITY] = {};
  size_t rxCount_ = 0;
  uint32_t validFrameCount_ = 0;
  uint32_t crcErrorCount_ = 0;
  uint32_t framingErrorCount_ = 0;
  uint32_t staleFrameResetCount_ = 0;
  uint32_t reconnectCount_ = 0;
  uint32_t rxOverflowCount_ = 0;
  bool webTransaction_ = false;
  bool profileRequestFromWeb_ = false;
  uint32_t profileListGeneration_ = 0, profileListMask_ = 0;
  uint32_t lastUartErrorMs_ = 0;
  HmiProtocol::TelemetryPayload telemetry_{};
  bool hasTelemetry_ = false;
  static constexpr size_t TRACE_QUEUE_CAPACITY = 96;
  HmiProtocol::ControlTracePayload traceQueue_[TRACE_QUEUE_CAPACITY] = {};
  size_t traceQueueHead_ = 0;
  size_t traceQueueCount_ = 0;
  uint32_t controlTraceQueueDrops_ = 0;
  HmiProtocol::ShotEndPayload shotEnd_{};
  bool hasShotEnd_ = false;
  MainCommandState commandState_ = MainCommandState::Idle;
  HmiProtocol::CommandResult commandResult_ =
      HmiProtocol::CommandResult::Accepted;
  HmiProtocol::Command pendingCommand_ = HmiProtocol::Command::Stop;
  uint16_t pendingCommandSequence_ = 0;
  uint32_t commandStateChangedMs_ = 0;
  HmiProtocol::ProfileInfoPayload
      profiles_[HmiProtocol::MAX_PROFILE_ITEMS] = {};
  uint8_t profileCount_ = 0;
  ProfileDetailData profileDetail_{};
  uint8_t profileDetailStepMask_ = 0;
  uint32_t profileDetailGeneration_ = 0;
  uint16_t profileTransferId_ = 0;
  HmiProtocol::ProfileTransferStatus profileTransferStatus_ =
      HmiProtocol::ProfileTransferStatus::Pending;
  HmiProtocol::ProfileCommitPayload profileTransferResult_{};
  uint32_t profileTransferGeneration_ = 0;
  uint32_t profileTransferStateChangedMs_ = 0;
  HmiProtocol::FirmwareReplyPayload firmwareReply_{};
  uint32_t firmwareReplyGeneration_ = 0;
  uint16_t calibrationRequestId_ = 0;
  uint32_t calibrationRequestedAt_ = 0;
  bool calibrationPending_ = false;
  HmiProtocol::CalibrationReplyPayload calibrationReply_{};
  uint32_t calibrationReplyGeneration_ = 0;
  PressureCaptureWire::Reply pressureCaptureReply_{};
  AdcProbeWire::Reply adcProbeReply_{};
  HeaterWire::Status heaterStatus_{};
  HeaterSensorWire::Status heaterSensorStatus_{};
  uint32_t heaterSensorReceivedMs_ = 0;
  bool heaterSensorSeen_ = false;
  uint32_t heaterReceivedMs_ = 0;
  bool heaterSeen_ = false;
  uint32_t heaterPendingTicket_ = 0;

  void discardBytes(size_t count);
  void parseFrames(uint32_t nowMs);
  void sendHeartbeat(uint32_t nowMs);
  uint16_t sendFrame(HmiProtocol::MessageType type, const void* payload,
                     uint16_t payloadLength);
};
