#include "communication/main_link.h"

#include <string.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "board_pins.h"
#include "hmi_protocol.h"
#include "app/owner_context.h"
namespace HmiOwner { bool webCall=false; }

namespace {
constexpr uint32_t MAIN_BAUD = 230400;
constexpr uint32_t HEARTBEAT_PERIOD_MS = 500;
constexpr uint32_t LINK_TIMEOUT_MS = 3500;
constexpr uint32_t PARTIAL_FRAME_TIMEOUT_MS = 120;
constexpr uint32_t COMMAND_TIMEOUT_MS = 1500;
constexpr size_t FRAME_CAPACITY =
    sizeof(HmiProtocol::FrameHeader) + 512 + 2;
}  // namespace

void MainLink::begin() {
  if(!brewStream_.available()){
    streamBoot_=esp_random();
    brewStream_.attach(static_cast<BrewStream::Sample*>(heap_caps_malloc(
        sizeof(BrewStream::Sample)*BrewStream::Capacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)));
  }
  serial_.setRxBufferSize(4096);
  serial_.begin(MAIN_BAUD, SERIAL_8N1, BoardPins::MAIN_UART_RX,
                BoardPins::MAIN_UART_TX);
}

void MainLink::discardBytes(size_t count) {
  if (count >= rxCount_) {
    rxCount_ = 0;
    return;
  }
  memmove(rxBuffer_, rxBuffer_ + count, rxCount_ - count);
  rxCount_ -= count;
}

uint16_t MainLink::sendFrame(HmiProtocol::MessageType type,
                             const void* payload, uint16_t payloadLength) {
  if (payloadLength > MAX_PAYLOAD) return 0;
  uint8_t frame[FRAME_CAPACITY];
  ++txSequence_;
  if (txSequence_ == 0) ++txSequence_;
  HmiProtocol::FrameHeader header{
      HmiProtocol::MAGIC, HmiProtocol::VERSION, type, payloadLength,
      txSequence_};
  memcpy(frame, &header, sizeof(header));
  if (payloadLength > 0)
    memcpy(frame + sizeof(header), payload, payloadLength);
  const size_t bodyLength = sizeof(header) + payloadLength;
  const uint16_t crc = HmiProtocol::crc16Ccitt(frame, bodyLength);
  frame[bodyLength] = static_cast<uint8_t>(crc & 0xFF);
  frame[bodyLength + 1] = static_cast<uint8_t>(crc >> 8);
  serial_.write(frame, bodyLength + 2);
  return header.sequence;
}

void MainLink::sendHeartbeat(uint32_t nowMs) {
  HmiProtocol::HeartbeatPayload payload{
      nowMs, HmiProtocol::EndpointRole::Hmi,
      HmiProtocol::HeartbeatFlags::SupportsControlTrace,
      HmiProtocol::SCHEMA_REVISION};
  sendFrame(HmiProtocol::MessageType::Heartbeat, &payload, sizeof(payload));
}

void MainLink::parseFrames(uint32_t nowMs) {
  while (rxCount_ >= sizeof(HmiProtocol::FrameHeader)) {
    HmiProtocol::FrameHeader header;
    memcpy(&header, rxBuffer_, sizeof(header));
    if (header.magic != HmiProtocol::MAGIC) {
      ++framingErrorCount_;
      lastUartErrorMs_ = nowMs;
      discardBytes(1);
      continue;
    }
    if (header.version != HmiProtocol::VERSION ||
        header.payloadLength > MAX_PAYLOAD) {
      ++framingErrorCount_;
      lastUartErrorMs_ = nowMs;
      discardBytes(1);
      continue;
    }
    const size_t total = sizeof(header) + header.payloadLength + 2;
    if (rxCount_ < total) return;
    const uint16_t receivedCrc = static_cast<uint16_t>(rxBuffer_[total - 2]) |
                                 static_cast<uint16_t>(rxBuffer_[total - 1]) << 8;
    const uint16_t calculatedCrc =
        HmiProtocol::crc16Ccitt(rxBuffer_, total - 2);
    if (receivedCrc != calculatedCrc) {
      ++crcErrorCount_;
      lastUartErrorMs_ = nowMs;
      // payloadLength may itself be corrupt. Scan forward one byte so a valid
      // frame already buffered behind this one is not thrown away.
      discardBytes(1);
      continue;
    }
    if (lastValidFrameMs_ == 0 ||
        nowMs - lastValidFrameMs_ >= LINK_TIMEOUT_MS)
      ++reconnectCount_;
    lastValidFrameMs_ = nowMs;
    ++validFrameCount_;
    if (header.type == HmiProtocol::MessageType::Heartbeat &&
        header.payloadLength == sizeof(HmiProtocol::HeartbeatPayload)) {
      HmiProtocol::HeartbeatPayload heartbeat;
      memcpy(&heartbeat, rxBuffer_ + sizeof(header), sizeof(heartbeat));
      if (heartbeat.role == HmiProtocol::EndpointRole::MainController) {
        lastValidFrameMs_ = nowMs;
        mainSchemaRevision_ = heartbeat.reserved;
        mainFeatureFlags_ = heartbeat.flags;
      }
    } else if (header.type == HmiProtocol::MessageType::ScaleTareRequest &&
               header.payloadLength == sizeof(scaleTareRequest_) && compatible()) {
      memcpy(&scaleTareRequest_, rxBuffer_ + sizeof(header), sizeof(scaleTareRequest_));
      hasScaleTareRequest_ = true;
    } else if (header.type == HmiProtocol::MessageType::Telemetry &&
               header.payloadLength == sizeof(HmiProtocol::TelemetryPayload)) {
      memcpy(&telemetry_, rxBuffer_ + sizeof(header), sizeof(telemetry_));
      telemetry_.profileName[sizeof(telemetry_.profileName) - 1] = '\0';
      telemetry_.phaseName[sizeof(telemetry_.phaseName) - 1] = '\0';
      hasTelemetry_ = true;
      brewStream_.push(telemetry_);
      lastValidFrameMs_ = nowMs;
    } else if (header.type == HmiProtocol::MessageType::ControlTrace &&
               header.payloadLength ==
                   sizeof(HmiProtocol::ControlTracePayload)) {
      HmiProtocol::ControlTracePayload trace{};
      memcpy(&trace, rxBuffer_ + sizeof(header), sizeof(trace));
      size_t writeIndex = 0;
      if (traceQueueCount_ < TRACE_QUEUE_CAPACITY) {
        writeIndex = (traceQueueHead_ + traceQueueCount_) % TRACE_QUEUE_CAPACITY;
        ++traceQueueCount_;
      } else {
        writeIndex = traceQueueHead_;
        traceQueueHead_ = (traceQueueHead_ + 1) % TRACE_QUEUE_CAPACITY;
        ++controlTraceQueueDrops_;
      }
      traceQueue_[writeIndex] = trace;
      lastValidFrameMs_ = nowMs;
    } else if (header.type == HmiProtocol::MessageType::ShotEnd &&
               header.payloadLength == sizeof(HmiProtocol::ShotEndPayload)) {
      memcpy(&shotEnd_, rxBuffer_ + sizeof(header), sizeof(shotEnd_));
      hasShotEnd_ = true;
      lastValidFrameMs_ = nowMs;
    } else if (header.type == HmiProtocol::MessageType::CommandReply &&
               header.payloadLength ==
                   sizeof(HmiProtocol::CommandReplyPayload)) {
      HmiProtocol::CommandReplyPayload reply{};
      memcpy(&reply, rxBuffer_ + sizeof(header), sizeof(reply));
      lastValidFrameMs_ = nowMs;
      if (commandState_ == MainCommandState::Pending &&
          reply.requestSequence == pendingCommandSequence_ &&
          reply.command == pendingCommand_) {
        commandResult_ = reply.result;
        commandState_ = reply.result == HmiProtocol::CommandResult::Accepted
                            ? MainCommandState::Accepted
                            : MainCommandState::Rejected;
        commandStateChangedMs_ = nowMs;
      }
    } else if (header.type == HmiProtocol::MessageType::ProfileInfo &&
               header.payloadLength ==
                   sizeof(HmiProtocol::ProfileInfoPayload)) {
      HmiProtocol::ProfileInfoPayload info{};
      memcpy(&info, rxBuffer_ + sizeof(header), sizeof(info));
      info.name[sizeof(info.name) - 1] = '\0';
      if (info.count <= HmiProtocol::MAX_PROFILE_ITEMS &&
          info.index < info.count) {
        if (info.index == 0) {profileCount_ = 0;profileListMask_=0;}
        profiles_[info.index] = info;
        profileListMask_|=1UL<<info.index;
        if(profileListMask_==((1UL<<info.count)-1))++profileListGeneration_;
        if (profileCount_ < info.index + 1) profileCount_ = info.index + 1;
        lastValidFrameMs_ = nowMs;
      }
    } else if (header.type == HmiProtocol::MessageType::ProfileBegin &&
               header.payloadLength ==
                   sizeof(HmiProtocol::ProfileBeginPayload)) {
      HmiProtocol::ProfileBeginPayload begin{};
      memcpy(&begin, rxBuffer_ + sizeof(header), sizeof(begin));
      if (begin.action == HmiProtocol::ProfileTransferAction::Download &&
          begin.stepCount <= HmiProtocol::MAX_PROFILE_STEPS) {
        profileDetail_ = ProfileDetailData{};
        profileDetail_.profile = begin;
        profileDetail_.profile.name[sizeof(profileDetail_.profile.name) - 1] = '\0';
        profileDetail_.profile.note[sizeof(profileDetail_.profile.note) - 1] = '\0';
        profileDetailStepMask_ = 0;
        Serial.printf("[PROFILE] begin id=%u index=%u steps=%u crc=%04X\n",
                      begin.transferId, begin.profileIndex, begin.stepCount,
                      begin.checksum);
        lastValidFrameMs_ = nowMs;
      }
    } else if (header.type == HmiProtocol::MessageType::ProfileChunk &&
               header.payloadLength ==
                   sizeof(HmiProtocol::ProfileChunkPayload)) {
      HmiProtocol::ProfileChunkPayload step{};
      memcpy(&step, rxBuffer_ + sizeof(header), sizeof(step));
      if (step.transferId == profileDetail_.profile.transferId &&
          step.stepIndex < profileDetail_.profile.stepCount) {
        step.name[sizeof(step.name) - 1] = '\0';
        profileDetail_.steps[step.stepIndex] = step;
        profileDetailStepMask_ |= static_cast<uint8_t>(1U << step.stepIndex);
        lastValidFrameMs_ = nowMs;
      }
    } else if (header.type == HmiProtocol::MessageType::ProfileTransferResult &&
               header.payloadLength ==
                   sizeof(HmiProtocol::ProfileCommitPayload)) {
      HmiProtocol::ProfileCommitPayload result{};
      memcpy(&result, rxBuffer_ + sizeof(header), sizeof(result));
      profileTransferResult_ = result;
      profileTransferStatus_ = result.status;
      ++profileTransferGeneration_;
      profileTransferStateChangedMs_ = nowMs;
      lastValidFrameMs_ = nowMs;
      if (result.action == HmiProtocol::ProfileTransferAction::Download &&
          result.status == HmiProtocol::ProfileTransferStatus::Accepted &&
          result.transferId == profileDetail_.profile.transferId) {
        const uint8_t expectedMask = static_cast<uint8_t>(
            (1U << profileDetail_.profile.stepCount) - 1U);
        const uint16_t checksum = HmiProtocol::profileChecksum(
            profileDetail_.profile, profileDetail_.steps,
            profileDetail_.profile.stepCount);
        profileDetail_.valid =
            (profileDetailStepMask_ & expectedMask) == expectedMask &&
            checksum == result.checksum &&
            checksum == profileDetail_.profile.checksum;
        Serial.printf("[PROFILE] download id=%u mask=%02X crc=%04X result=%u valid=%u\n",
                      result.transferId, profileDetailStepMask_, checksum,
                      static_cast<unsigned>(result.status),
                      profileDetail_.valid ? 1U : 0U);
        if (profileDetail_.valid) ++profileDetailGeneration_;
      } else if (result.action == HmiProtocol::ProfileTransferAction::Upload) {
        Serial.printf("[PROFILE] upload id=%u result=%u index=%u steps=%u\n",
                      result.transferId, static_cast<unsigned>(result.status),
                      result.profileIndex, result.receivedSteps);
      }
    } else if (header.type == HmiProtocol::MessageType::HeaterSensorStatus && compatible() && header.payloadLength == sizeof(heaterSensorStatus_)) {
      HeaterSensorWire::Status s{}; memcpy(&s, rxBuffer_ + sizeof(header), sizeof(s));
      if (s.version == HeaterSensorWire::VERSION) {
        heaterSensorStatus_ = s; heaterSensorReceivedMs_ = millis(); heaterSensorSeen_ = true;
      }
    } else if (header.type == HmiProtocol::MessageType::HeaterStatus && compatible() && header.payloadLength == sizeof(heaterStatus_)) {
      HeaterWire::Status s{}; memcpy(&s, rxBuffer_ + sizeof(header), sizeof(s));
      if (s.version == HeaterWire::VERSION) { heaterStatus_ = s; heaterReceivedMs_ = millis(); heaterSeen_ = true; }
    } else if (header.type == HmiProtocol::MessageType::AdcProbeReply && compatible() &&
               header.payloadLength == sizeof(adcProbeReply_)) {
      AdcProbeWire::Reply reply{};
      memcpy(&reply, rxBuffer_ + sizeof(header), sizeof(reply));
      if (reply.version == AdcProbeWire::VERSION && reply.count <= AdcProbeWire::SAMPLES)
        adcProbeReply_ = reply;
    } else if (header.type == HmiProtocol::MessageType::PressureCaptureReply &&
               compatible() && header.payloadLength >= offsetof(PressureCaptureWire::Reply, samples) &&
               header.payloadLength <= sizeof(pressureCaptureReply_)) {
      PressureCaptureWire::Reply reply{};
      memcpy(&reply, rxBuffer_ + sizeof(header), header.payloadLength);
      if (reply.version == PressureCaptureWire::VERSION &&
          reply.count <= PressureCaptureWire::CHUNK_ROWS &&
          header.payloadLength == offsetof(PressureCaptureWire::Reply, samples) +
              reply.count * sizeof(PressureCaptureWire::Sample)) pressureCaptureReply_ = reply;
    } else if (header.type == HmiProtocol::MessageType::FirmwareReply &&
               header.payloadLength ==
                   sizeof(HmiProtocol::FirmwareReplyPayload)) {
      memcpy(&firmwareReply_, rxBuffer_ + sizeof(header),
             sizeof(firmwareReply_));
      ++firmwareReplyGeneration_;
      lastValidFrameMs_ = nowMs;
    } else if (header.type == HmiProtocol::MessageType::CalibrationReply &&
               header.payloadLength ==
                   sizeof(HmiProtocol::CalibrationReplyPayload)) {
      HmiProtocol::CalibrationReplyPayload reply{};
      memcpy(&reply,rxBuffer_+sizeof(header),sizeof(reply));
      if(reply.requestId==calibrationRequestId_){
        calibrationReply_=reply;++calibrationReplyGeneration_;calibrationPending_=false;
      }
      lastValidFrameMs_ = nowMs;
    }
    discardBytes(total);
  }
}

void MainLink::poll() {
  const uint32_t nowMs = millis();
  if (rxCount_ > 0 && lastRxByteMs_ != 0 &&
      nowMs - lastRxByteMs_ >= PARTIAL_FRAME_TIMEOUT_MS && serial_.available() == 0) {
    rxCount_ = 0;
    ++staleFrameResetCount_;
    lastUartErrorMs_ = nowMs;
  }
  while (serial_.available() > 0) {
    // Parse before backlog can evict complete frames from the smaller parser buffer.
    if (rxCount_ >= sizeof(HmiProtocol::FrameHeader)) parseFrames(nowMs);
    const int value = serial_.read();
    if (value < 0) break;
    if (rxCount_ == RX_CAPACITY) { ++rxOverflowCount_; discardBytes(1); }
    rxBuffer_[rxCount_++] = static_cast<uint8_t>(value);
    lastRxByteMs_ = nowMs;
  }
  parseFrames(nowMs);
  if (nowMs - lastHeartbeatTxMs_ >= HEARTBEAT_PERIOD_MS) {
    lastHeartbeatTxMs_ = nowMs;
    sendHeartbeat(nowMs);
  }
  if (commandState_ == MainCommandState::Pending &&
      nowMs - commandStateChangedMs_ >= COMMAND_TIMEOUT_MS) {
    commandState_ = MainCommandState::TimedOut;
    commandStateChangedMs_ = nowMs;
  }
}

bool MainLink::requestPressureCapture(const PressureCaptureWire::Request& request) {
  if (!connected() || !supportsPressureCapture()) return false;
  return sendFrame(HmiProtocol::MessageType::PressureCaptureRequest, &request, sizeof(request)) != 0;
}

bool MainLink::requestAdcProbe(const AdcProbeWire::Request& request) {
  if (!connected() || !supportsAdcProbe()) return false;
  return sendFrame(HmiProtocol::MessageType::AdcProbeRequest, &request, sizeof(request)) != 0;
}

bool MainLink::requestHeater(const HeaterWire::Request& request) {
  // OFF is still deliverable when optional status frames are stale.
  if (request.op == HeaterWire::Op::Off ? !supportsHeater() : !heaterFresh()) return false;
  if (sendFrame(HmiProtocol::MessageType::HeaterRequest, &request, sizeof(request)) == 0) return false;
  heaterPendingTicket_ = request.ticket;
  return true;
}

bool MainLink::connected() const {
  return lastValidFrameMs_ != 0 && millis() - lastValidFrameMs_ < LINK_TIMEOUT_MS;
}

bool MainLink::compatible() const {
  return connected() && mainSchemaRevision_ == HmiProtocol::SCHEMA_REVISION;
}

uint32_t MainLink::lastFrameAgeMs() const {
  return lastValidFrameMs_ == 0 ? UINT32_MAX : millis() - lastValidFrameMs_;
}

uint32_t MainLink::lastErrorAgeMs() const {
  return lastUartErrorMs_ == 0 ? UINT32_MAX : millis() - lastUartErrorMs_;
}

bool MainLink::copyTelemetry(HmiProtocol::TelemetryPayload& telemetry) const {
  if (!hasTelemetry_) return false;
  telemetry = telemetry_;
  return true;
}

bool MainLink::popControlTrace(HmiProtocol::ControlTracePayload& trace) {
  if (traceQueueCount_ == 0) return false;
  trace = traceQueue_[traceQueueHead_];
  traceQueueHead_ = (traceQueueHead_ + 1) % TRACE_QUEUE_CAPACITY;
  --traceQueueCount_;
  return true;
}

bool MainLink::popShotEnd(HmiProtocol::ShotEndPayload& shotEnd) {
  if (!hasShotEnd_) return false;
  shotEnd = shotEnd_;
  hasShotEnd_ = false;
  return true;
}

bool MainLink::sendScaleSample(
    const HmiProtocol::ScaleSamplePayload& sample) {
  return compatible() &&
         sendFrame(HmiProtocol::MessageType::ScaleSample, &sample,
                   sizeof(sample)) != 0;
}

bool MainLink::sendCommand(HmiProtocol::Command command, uint32_t argument) {
  if (!connected()) return false;
  if (commandState_ == MainCommandState::Pending &&
      command != HmiProtocol::Command::Stop)
    return false;

  HmiProtocol::CommandRequestPayload request{};
  request.command = command;
  request.argument = argument;
  pendingCommandSequence_ = sendFrame(
      HmiProtocol::MessageType::CommandRequest, &request, sizeof(request));
  if (pendingCommandSequence_ == 0) return false;
  pendingCommand_ = command;
  commandState_ = MainCommandState::Pending;
  commandStateChangedMs_ = millis();
  return true;
}

bool MainLink::beginWebTransaction() {
  if(webTransaction_ || (profileTransferStateChangedMs_ &&
      profileTransferStatus_==HmiProtocol::ProfileTransferStatus::Pending &&
      profileTransferStateAgeMs()<1500) ||
      (calibrationPending_ && millis()-calibrationRequestedAt_<1000))return false;
  webTransaction_=true;return true;
}

bool MainLink::requestProfiles() {
  if(webTransaction_ && !HmiOwner::webCall)return false;
  if (!connected()) return false;
  profileListMask_=0;
  return sendFrame(HmiProtocol::MessageType::ProfileListRequest, nullptr, 0) !=
         0;
}

bool MainLink::requestProfileDetail(uint8_t profileIndex) {
  if(webTransaction_ && !HmiOwner::webCall)return false;
  if (!connected()) return false;
  profileRequestFromWeb_=HmiOwner::webCall;
  HmiProtocol::ProfileDetailRequestPayload request{profileIndex, {0, 0, 0}};
  profileDetail_.valid = false;
  profileTransferStatus_ = HmiProtocol::ProfileTransferStatus::Pending;
  profileTransferStateChangedMs_ = millis();
  const bool sent = sendFrame(HmiProtocol::MessageType::ProfileDetailRequest,
                              &request, sizeof(request)) != 0;
  Serial.printf("[PROFILE] request index=%u sent=%u\n", profileIndex,
                sent ? 1U : 0U);
  return sent;
}

bool MainLink::uploadProfile(const ProfileDetailData& draft) {
  if(webTransaction_ && !HmiOwner::webCall)return false;
  if (!connected() || !draft.valid || draft.profile.stepCount == 0 ||
      draft.profile.stepCount > HmiProtocol::MAX_PROFILE_STEPS)
    return false;
  ProfileDetailData transfer = draft;
  profileRequestFromWeb_=HmiOwner::webCall;
  if (++profileTransferId_ == 0) ++profileTransferId_;
  transfer.profile.transferId = profileTransferId_;
  transfer.profile.action = HmiProtocol::ProfileTransferAction::Upload;
  for (uint8_t i = 0; i < transfer.profile.stepCount; ++i) {
    transfer.steps[i].transferId = profileTransferId_;
    transfer.steps[i].stepIndex = i;
  }
  transfer.profile.checksum = HmiProtocol::profileChecksum(
      transfer.profile, transfer.steps, transfer.profile.stepCount);
  if (sendFrame(HmiProtocol::MessageType::ProfileBegin, &transfer.profile,
                sizeof(transfer.profile)) == 0)
    return false;
  for (uint8_t i = 0; i < transfer.profile.stepCount; ++i) {
    if (sendFrame(HmiProtocol::MessageType::ProfileChunk, &transfer.steps[i],
                  sizeof(transfer.steps[i])) == 0)
      return false;
  }
  HmiProtocol::ProfileCommitPayload commit{
      transfer.profile.transferId, HmiProtocol::ProfileTransferAction::Upload,
      HmiProtocol::ProfileTransferStatus::Pending, transfer.profile.profileIndex,
      transfer.profile.stepCount, transfer.profile.checksum};
  if (sendFrame(HmiProtocol::MessageType::ProfileCommit, &commit,
                sizeof(commit)) == 0)
    return false;
  profileTransferStatus_ = HmiProtocol::ProfileTransferStatus::Pending;
  profileTransferStateChangedMs_ = millis();
  return true;
}

bool MainLink::deleteProfile(uint8_t profileIndex) {
  if(webTransaction_ && !HmiOwner::webCall)return false;
  if (!connected() || profileIndex < 2) return false;
  profileRequestFromWeb_=HmiOwner::webCall;
  if (++profileTransferId_ == 0) ++profileTransferId_;
  HmiProtocol::ProfileDeleteRequestPayload request{
      profileTransferId_, profileIndex, 0};
  if (sendFrame(HmiProtocol::MessageType::ProfileDeleteRequest, &request,
                sizeof(request)) == 0)
    return false;
  profileTransferStatus_ = HmiProtocol::ProfileTransferStatus::Pending;
  profileTransferStateChangedMs_ = millis();
  return true;
}

bool MainLink::requestCalibration(
    HmiProtocol::CalibrationOperation operation, float value1, float value2,
    float value3) {
  if(webTransaction_ && !HmiOwner::webCall)return false;
  if (!connected()) return false;
  if (!compatible()) return false;
  if(calibrationPending_ && millis()-calibrationRequestedAt_<1000)return false;
  if (++calibrationRequestId_ == 0) ++calibrationRequestId_;
  HmiProtocol::CalibrationRequestPayload request{
      calibrationRequestId_, operation, 0, value1, value2, value3};
  calibrationPending_=sendFrame(HmiProtocol::MessageType::CalibrationRequest, &request,
                   sizeof(request)) != 0;
  calibrationRequestedAt_=millis();return calibrationPending_;
}

bool MainLink::sendFirmwareBegin(
    const HmiProtocol::FirmwareBeginPayload& begin) {
  return connected() &&
         sendFrame(HmiProtocol::MessageType::FirmwareBegin, &begin,
                   sizeof(begin)) != 0;
}

bool MainLink::sendFirmwareChunk(
    const HmiProtocol::FirmwareChunkPayload& chunk) {
  if (!connected() || chunk.dataLength == 0 ||
      chunk.dataLength > HmiProtocol::MAX_FIRMWARE_CHUNK)
    return false;
  const uint16_t length = static_cast<uint16_t>(
      offsetof(HmiProtocol::FirmwareChunkPayload, data) + chunk.dataLength);
  return sendFrame(HmiProtocol::MessageType::FirmwareChunk, &chunk, length) != 0;
}

bool MainLink::sendFirmwareEnd(const HmiProtocol::FirmwareEndPayload& end) {
  return connected() &&
         sendFrame(HmiProtocol::MessageType::FirmwareEnd, &end,
                   sizeof(end)) != 0;
}

bool MainLink::sendFirmwareAbort(uint32_t transferId) {
  HmiProtocol::FirmwareAbortPayload abort{transferId};
  return sendFrame(HmiProtocol::MessageType::FirmwareAbort, &abort,
                   sizeof(abort)) != 0;
}

uint32_t MainLink::commandStateAgeMs() const {
  return commandStateChangedMs_ == 0 ? UINT32_MAX
                                     : millis() - commandStateChangedMs_;
}

uint32_t MainLink::profileTransferStateAgeMs() const {
  return profileTransferStateChangedMs_ == 0
             ? UINT32_MAX
             : millis() - profileTransferStateChangedMs_;
}

bool MainLink::popScaleTareRequest(HmiProtocol::ScaleTareRequestPayload& request) {
  if (!hasScaleTareRequest_) return false;
  request = scaleTareRequest_;
  hasScaleTareRequest_ = false;
  return true;
}
