#include "src/communication/hmi_link_internal.h"
#include "src/hardware/heater_control.h"

#include <Arduino.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include "protocol/sha256_compat.h"
#include <string.h>

#include "src/app/orchestrator.h"
#include "src/hardware/valve_sense.h"
#include "src/hardware/pump_control.h"
#include "src/hardware/pump_control.h"

namespace HmiLinkInternal {
namespace FirmwareReceiver {
namespace {
constexpr uint32_t FIRMWARE_TIMEOUT_MS = 10000;
bool firmwareActive = false;
uint32_t firmwareTransferId = 0;
uint32_t firmwareSize = 0;
uint32_t firmwareWritten = 0;
uint32_t firmwareLastActivityMs = 0;
uint32_t firmwareRestartAtMs = 0;
uint32_t firmwareLastChunkOffset = UINT32_MAX;
uint16_t firmwareLastChunkLength = 0;
uint16_t firmwareLastChunkCrc = 0;
uint8_t firmwareExpectedSha[32] = {};
mbedtls_sha256_context firmwareSha;
bool firmwareShaInitialized = false;
void resetFirmwareState(bool abortUpdate) {
  if (abortUpdate && Update.isRunning()) Update.abort();
  if (firmwareShaInitialized) {
    mbedtls_sha256_free(&firmwareSha);
    firmwareShaInitialized = false;
  }
  firmwareActive = false;
  firmwareTransferId = 0;
  firmwareSize = 0;
  firmwareWritten = 0;
  firmwareLastActivityMs = 0;
  firmwareLastChunkOffset = UINT32_MAX;
  firmwareLastChunkLength = 0;
  firmwareLastChunkCrc = 0;
  memset(firmwareExpectedSha, 0, sizeof(firmwareExpectedSha));
}

void sendFirmwareReply(uint32_t transferId, HmiProtocol::FirmwareStage stage,
                       HmiProtocol::FirmwareStatus status,
                       uint32_t nextOffset, uint32_t imageSize) {
  HmiProtocol::FirmwareReplyPayload reply{transferId, stage, status, 0,
                                           nextOffset, imageSize};
  sendFrame(HmiProtocol::MessageType::FirmwareReply, &reply, sizeof(reply));
}


void beginFirmwareUpdate(const HmiProtocol::FirmwareBeginPayload& begin) {
  if (begin.target != HmiProtocol::FirmwareTarget::MainController) {
    sendFirmwareReply(begin.transferId, HmiProtocol::FirmwareStage::Begin,
                      HmiProtocol::FirmwareStatus::InvalidTarget, 0,
                      begin.imageSize);
    return;
  }
  if (firmwareActive) {
    const bool duplicate = begin.transferId == firmwareTransferId &&
                           begin.imageSize == firmwareSize &&
                           memcmp(begin.sha256, firmwareExpectedSha, 32) == 0;
    sendFirmwareReply(begin.transferId, HmiProtocol::FirmwareStage::Begin,
                      duplicate ? HmiProtocol::FirmwareStatus::Accepted
                                : HmiProtocol::FirmwareStatus::Busy,
                      duplicate ? firmwareWritten : 0, firmwareSize);
    return;
  }
  if (Orchestrator::getState() != IDLE ||
      ValveSense::isMachineValveActive()) {
    sendFirmwareReply(begin.transferId, HmiProtocol::FirmwareStage::Begin,
                      HmiProtocol::FirmwareStatus::RejectedState, 0,
                      begin.imageSize);
    return;
  }
  const esp_partition_t* partition = esp_ota_get_next_update_partition(nullptr);
  if (!partition || begin.imageSize < 1024 || begin.imageSize > partition->size) {
    sendFirmwareReply(begin.transferId, HmiProtocol::FirmwareStage::Begin,
                      HmiProtocol::FirmwareStatus::InvalidSize, 0,
                      partition ? partition->size : 0);
    return;
  }
  // Cancel an idle diagnostic before flash work; owner cannot execute it late.
  PumpPSM::cancelStoppedReset();
  // Cancel an idle diagnostic before flash work; owner cannot execute it late.
  PumpPSM::cancelStoppedReset();
  Orchestrator::forceIdle();
  Heater::inhibit();
  if (!Update.begin(begin.imageSize, U_FLASH)) {
    sendFirmwareReply(begin.transferId, HmiProtocol::FirmwareStage::Begin,
                      HmiProtocol::FirmwareStatus::WriteError, 0,
                      begin.imageSize);
    return;
  }
  firmwareActive = true;
  firmwareTransferId = begin.transferId;
  firmwareSize = begin.imageSize;
  firmwareWritten = 0;
  firmwareLastActivityMs = millis();
  firmwareLastChunkOffset = UINT32_MAX;
  memcpy(firmwareExpectedSha, begin.sha256, 32);
  mbedtls_sha256_init(&firmwareSha);
  if (BrewVectorSha256::starts(&firmwareSha) != 0) {
    resetFirmwareState(true);
    sendFirmwareReply(begin.transferId, HmiProtocol::FirmwareStage::Begin,
                      HmiProtocol::FirmwareStatus::WriteError, 0,
                      begin.imageSize);
    return;
  }
  firmwareShaInitialized = true;
  sendFirmwareReply(begin.transferId, HmiProtocol::FirmwareStage::Begin,
                    HmiProtocol::FirmwareStatus::Accepted, 0,
                    begin.imageSize);
}

void receiveFirmwareChunk(const uint8_t* payload, uint16_t payloadLength) {
  constexpr size_t prefixLength = offsetof(HmiProtocol::FirmwareChunkPayload, data);
  if (payloadLength < prefixLength) return;
  HmiProtocol::FirmwareChunkPayload chunk{};
  memcpy(&chunk, payload, prefixLength);
  if (chunk.dataLength == 0 ||
      chunk.dataLength > HmiProtocol::MAX_FIRMWARE_CHUNK ||
      payloadLength != prefixLength + chunk.dataLength) {
    sendFirmwareReply(chunk.transferId, HmiProtocol::FirmwareStage::Chunk,
                      HmiProtocol::FirmwareStatus::InvalidSize,
                      firmwareWritten, firmwareSize);
    return;
  }
  const uint8_t* data = payload + prefixLength;
  memcpy(chunk.data, data, chunk.dataLength);
  const uint16_t dataCrc = HmiProtocol::crc16Ccitt(data, chunk.dataLength);
  if (!firmwareActive || chunk.transferId != firmwareTransferId) {
    sendFirmwareReply(chunk.transferId, HmiProtocol::FirmwareStage::Chunk,
                      HmiProtocol::FirmwareStatus::InvalidTransfer,
                      firmwareWritten, firmwareSize);
    return;
  }
  firmwareLastActivityMs = millis();
  const bool duplicate = chunk.offset == firmwareLastChunkOffset &&
                         chunk.dataLength == firmwareLastChunkLength &&
                         chunk.dataCrc == firmwareLastChunkCrc &&
                         dataCrc == chunk.dataCrc;
  if (duplicate) {
    sendFirmwareReply(chunk.transferId, HmiProtocol::FirmwareStage::Chunk,
                      HmiProtocol::FirmwareStatus::Accepted,
                      firmwareWritten, firmwareSize);
    return;
  }
  if (chunk.offset != firmwareWritten || dataCrc != chunk.dataCrc ||
      firmwareWritten + chunk.dataLength > firmwareSize) {
    sendFirmwareReply(chunk.transferId, HmiProtocol::FirmwareStage::Chunk,
                      dataCrc != chunk.dataCrc
                          ? HmiProtocol::FirmwareStatus::ChecksumMismatch
                          : HmiProtocol::FirmwareStatus::InvalidOffset,
                      firmwareWritten, firmwareSize);
    return;
  }
  if (Update.write(chunk.data, chunk.dataLength) != chunk.dataLength ||
      BrewVectorSha256::update(&firmwareSha, chunk.data,
                               chunk.dataLength) != 0) {
    const uint32_t id = firmwareTransferId;
    const uint32_t size = firmwareSize;
    resetFirmwareState(true);
    sendFirmwareReply(id, HmiProtocol::FirmwareStage::Chunk,
                      HmiProtocol::FirmwareStatus::WriteError,
                      firmwareWritten, size);
    return;
  }
  firmwareLastChunkOffset = chunk.offset;
  firmwareLastChunkLength = chunk.dataLength;
  firmwareLastChunkCrc = chunk.dataCrc;
  firmwareWritten += chunk.dataLength;
  sendFirmwareReply(chunk.transferId, HmiProtocol::FirmwareStage::Chunk,
                    HmiProtocol::FirmwareStatus::Accepted,
                    firmwareWritten, firmwareSize);
}

void endFirmwareUpdate(const HmiProtocol::FirmwareEndPayload& end) {
  if (!firmwareActive || end.transferId != firmwareTransferId) {
    sendFirmwareReply(end.transferId, HmiProtocol::FirmwareStage::End,
                      HmiProtocol::FirmwareStatus::InvalidTransfer,
                      firmwareWritten, firmwareSize);
    return;
  }
  firmwareLastActivityMs = millis();
  if (end.imageSize != firmwareSize || firmwareWritten != firmwareSize ||
      memcmp(end.sha256, firmwareExpectedSha, 32) != 0) {
    sendFirmwareReply(end.transferId, HmiProtocol::FirmwareStage::End,
                      HmiProtocol::FirmwareStatus::InvalidSize,
                      firmwareWritten, firmwareSize);
    return;
  }
  uint8_t digest[32] = {};
  const bool hashOk = BrewVectorSha256::finish(&firmwareSha, digest) == 0 &&
                      memcmp(digest, firmwareExpectedSha, 32) == 0;
  mbedtls_sha256_free(&firmwareSha);
  firmwareShaInitialized = false;
  if (!hashOk) {
    const uint32_t id = firmwareTransferId;
    const uint32_t size = firmwareSize;
    resetFirmwareState(true);
    sendFirmwareReply(id, HmiProtocol::FirmwareStage::End,
                      HmiProtocol::FirmwareStatus::ChecksumMismatch,
                      firmwareWritten, size);
    return;
  }
  if (!Update.end(false) || Update.hasError()) {
    const uint32_t id = firmwareTransferId;
    const uint32_t size = firmwareSize;
    resetFirmwareState(false);
    sendFirmwareReply(id, HmiProtocol::FirmwareStage::End,
                      HmiProtocol::FirmwareStatus::FinalizeError,
                      firmwareWritten, size);
    return;
  }
  sendFirmwareReply(firmwareTransferId, HmiProtocol::FirmwareStage::End,
                    HmiProtocol::FirmwareStatus::Accepted,
                    firmwareWritten, firmwareSize);
  firmwareRestartAtMs = millis() + 1200;
  // Keep firmwareActive true until restart so no physical or remote command
  // can start the machine after the new boot partition has been selected.
}

void abortFirmwareUpdate(const HmiProtocol::FirmwareAbortPayload& abort) {
  if (firmwareActive && abort.transferId == firmwareTransferId) {
    const uint32_t size = firmwareSize;
    resetFirmwareState(true);
    sendFirmwareReply(abort.transferId, HmiProtocol::FirmwareStage::Abort,
                      HmiProtocol::FirmwareStatus::Aborted, 0, size);
  }
}

}  // namespace

bool handle(const HmiProtocol::FrameHeader& header, const uint8_t* payload,
            uint32_t nowMs) {
  (void)nowMs;
  if (header.type == HmiProtocol::MessageType::FirmwareBegin &&
      header.payloadLength == sizeof(HmiProtocol::FirmwareBeginPayload)) {
    HmiProtocol::FirmwareBeginPayload begin{};
    memcpy(&begin, payload, sizeof(begin));
    beginFirmwareUpdate(begin);
    return true;
  }
  if (header.type == HmiProtocol::MessageType::FirmwareChunk) {
    receiveFirmwareChunk(payload, header.payloadLength);
    return true;
  }
  if (header.type == HmiProtocol::MessageType::FirmwareEnd &&
      header.payloadLength == sizeof(HmiProtocol::FirmwareEndPayload)) {
    HmiProtocol::FirmwareEndPayload end{};
    memcpy(&end, payload, sizeof(end));
    endFirmwareUpdate(end);
    return true;
  }
  if (header.type == HmiProtocol::MessageType::FirmwareAbort &&
      header.payloadLength == sizeof(HmiProtocol::FirmwareAbortPayload)) {
    HmiProtocol::FirmwareAbortPayload abort{};
    memcpy(&abort, payload, sizeof(abort));
    abortFirmwareUpdate(abort);
    return true;
  }
  return false;
}

void poll(uint32_t nowMs) {
  if (firmwareActive && firmwareRestartAtMs == 0 &&
      nowMs - firmwareLastActivityMs >= FIRMWARE_TIMEOUT_MS)
    resetFirmwareState(true);
  if (firmwareRestartAtMs != 0 &&
      static_cast<int32_t>(nowMs - firmwareRestartAtMs) >= 0)
    ESP.restart();
}

bool active() { return firmwareActive; }

}  // namespace FirmwareReceiver
}  // namespace HmiLinkInternal
