#include "update/main_firmware_updater.h"

#include <FS.h>
#include <SD_MMC.h>
#include "sha256_compat.h"

#include "communication/main_link.h"
#include "hmi_protocol.h"
#include "storage/history_root.h"

namespace {
constexpr char UPDATE_DIR[] = "/brewvector/update";
constexpr char IMAGE_PATH[] = "/brewvector/update/main_firmware.bin";
constexpr uint32_t MIN_IMAGE_SIZE = 1024;
constexpr uint32_t MAX_IMAGE_SIZE = 2U * 1024U * 1024U;
constexpr uint32_t REPLY_TIMEOUT_MS = 1400;
constexpr uint8_t MAX_RETRIES = 6;

enum class State : uint8_t {
  Empty,
  Uploading,
  Ready,
  BeginWait,
  ChunkWait,
  EndWait,
  Complete,
  Error,
};

MainLink* mainLink = nullptr;
State state = State::Empty;
File uploadFile;
File transferFile;
mbedtls_sha256_context uploadSha;
bool uploadShaInitialized = false;
uint8_t imageSha[32] = {};
uint32_t storedSize = 0;
uint32_t transferId = 0;
uint32_t transferOffset = 0;
uint32_t lastReplyGeneration = 0;
uint32_t lastSendMs = 0;
uint8_t retryCount = 0;
uint8_t sessionRecoveryCount = 0;
HmiProtocol::FirmwareChunkPayload currentChunk{};
char errorMessage[64] = {};

void setError(const char* message, bool notifyMain = true) {
  if (notifyMain && mainLink && transferId != 0 &&
      (state == State::BeginWait || state == State::ChunkWait ||
       state == State::EndWait))
    mainLink->sendFirmwareAbort(transferId);
  if (transferFile) transferFile.close();
  snprintf(errorMessage, sizeof(errorMessage), "%s", message ? message : "ERROR");
  state = State::Error;
}

bool ensureStorage() {
  if (SD_MMC.cardType() == CARD_NONE) {
    setError("TF CARD NOT READY", false);
    return false;
  }
  if (!HistoryRoot::ensure()) {
    setError("TF ROOT ERROR", false);
    return false;
  }
  if (!SD_MMC.exists(UPDATE_DIR) && !SD_MMC.mkdir(UPDATE_DIR)) {
    setError("TF UPDATE DIR ERROR", false);
    return false;
  }
  return true;
}

bool sendBegin() {
  HmiProtocol::FirmwareBeginPayload begin{};
  begin.transferId = transferId;
  begin.imageSize = storedSize;
  begin.target = HmiProtocol::FirmwareTarget::MainController;
  memcpy(begin.sha256, imageSha, sizeof(imageSha));
  if (!mainLink || !mainLink->sendFirmwareBegin(begin)) return false;
  lastSendMs = millis();
  return true;
}

bool sendChunk() {
  if (!transferFile || transferOffset >= storedSize) return false;
  if (!transferFile.seek(transferOffset)) return false;
  currentChunk = {};
  currentChunk.transferId = transferId;
  currentChunk.offset = transferOffset;
  currentChunk.dataLength = static_cast<uint16_t>(min<uint32_t>(
      HmiProtocol::MAX_FIRMWARE_CHUNK, storedSize - transferOffset));
  if (transferFile.read(currentChunk.data, currentChunk.dataLength) !=
      currentChunk.dataLength)
    return false;
  currentChunk.dataCrc =
      HmiProtocol::crc16Ccitt(currentChunk.data, currentChunk.dataLength);
  if (!mainLink->sendFirmwareChunk(currentChunk)) return false;
  lastSendMs = millis();
  return true;
}

bool resendChunk() {
  if (!mainLink || !mainLink->sendFirmwareChunk(currentChunk)) return false;
  lastSendMs = millis();
  return true;
}

bool sendEnd() {
  HmiProtocol::FirmwareEndPayload end{};
  end.transferId = transferId;
  end.imageSize = storedSize;
  memcpy(end.sha256, imageSha, sizeof(imageSha));
  if (!mainLink || !mainLink->sendFirmwareEnd(end)) return false;
  lastSendMs = millis();
  return true;
}

void advanceAfterOffset(uint32_t nextOffset) {
  if (nextOffset > storedSize) {
    setError("MAIN RETURNED BAD OFFSET");
    return;
  }
  transferOffset = nextOffset;
  retryCount = 0;
  errorMessage[0] = '\0';
  if (transferOffset == storedSize) {
    if (!sendEnd()) {
      setError("CANNOT SEND FINALIZE");
      return;
    }
    state = State::EndWait;
  } else {
    if (!sendChunk()) {
      setError("TF READ OR UART ERROR");
      return;
    }
    state = State::ChunkWait;
  }
}

const char* firmwareStatusName(HmiProtocol::FirmwareStatus status) {
  switch (status) {
    case HmiProtocol::FirmwareStatus::RejectedState: return "MAIN NOT IDLE";
    case HmiProtocol::FirmwareStatus::Busy: return "MAIN UPDATE BUSY";
    case HmiProtocol::FirmwareStatus::InvalidTarget: return "INVALID TARGET";
    case HmiProtocol::FirmwareStatus::InvalidSize: return "INVALID IMAGE SIZE";
    case HmiProtocol::FirmwareStatus::InvalidOffset: return "OFFSET MISMATCH";
    case HmiProtocol::FirmwareStatus::ChecksumMismatch: return "CHECKSUM MISMATCH";
    case HmiProtocol::FirmwareStatus::WriteError: return "MAIN FLASH WRITE ERROR";
    case HmiProtocol::FirmwareStatus::FinalizeError: return "MAIN FINALIZE ERROR";
    case HmiProtocol::FirmwareStatus::InvalidTransfer: return "INVALID TRANSFER";
    case HmiProtocol::FirmwareStatus::Aborted: return "UPDATE ABORTED";
    default: return "MAIN REJECTED UPDATE";
  }
}
}  // namespace

namespace MainFirmwareUpdater {

void begin(MainLink& link) {
  mainLink = &link;
  lastReplyGeneration = link.firmwareReplyGeneration();
  // Storage is mounted after Wi-Fi/WebServer startup. A staged image from an
  // earlier boot cannot be installed without a fresh SHA anyway, so defer all
  // TF access until the next explicit upload.
  storedSize = 0;
  state = State::Empty;
  errorMessage[0] = '\0';
}

bool beginUpload(const String& filename) {
  if (busy() || !filename.endsWith(".bin") || !ensureStorage()) return false;
  if (uploadFile) uploadFile.close();
  SD_MMC.remove(IMAGE_PATH);
  uploadFile = SD_MMC.open(IMAGE_PATH, FILE_WRITE);
  if (!uploadFile) {
    setError("CANNOT CREATE IMAGE FILE", false);
    return false;
  }
  mbedtls_sha256_init(&uploadSha);
  if (BrewVectorSha256::starts(&uploadSha) != 0) {
    uploadFile.close();
    setError("SHA256 INIT ERROR", false);
    return false;
  }
  uploadShaInitialized = true;
  storedSize = 0;
  errorMessage[0] = '\0';
  state = State::Uploading;
  return true;
}

bool writeUpload(const uint8_t* data, size_t length) {
  if (state != State::Uploading || !uploadFile || !data || length == 0 ||
      storedSize + length > MAX_IMAGE_SIZE)
    return false;
  if (uploadFile.write(data, length) != length ||
      BrewVectorSha256::update(&uploadSha, data, length) != 0)
    return false;
  storedSize += length;
  return true;
}

bool finishUpload() {
  if (state != State::Uploading || !uploadFile) return false;
  uploadFile.close();
  bool valid = storedSize >= MIN_IMAGE_SIZE && storedSize <= MAX_IMAGE_SIZE;
  if (uploadShaInitialized) {
    valid = valid && BrewVectorSha256::finish(&uploadSha, imageSha) == 0;
    mbedtls_sha256_free(&uploadSha);
    uploadShaInitialized = false;
  } else {
    valid = false;
  }
  File verify = SD_MMC.open(IMAGE_PATH, FILE_READ);
  const int imageMagic = verify ? verify.read() : -1;
  if (verify) verify.close();
  valid = valid && imageMagic == 0xE9;
  if (!valid) {
    SD_MMC.remove(IMAGE_PATH);
    storedSize = 0;
    setError("INVALID ESP32 FIRMWARE BIN", false);
    return false;
  }
  state = State::Ready;
  errorMessage[0] = '\0';
  return true;
}

void abortUpload() {
  if (uploadFile) uploadFile.close();
  if (uploadShaInitialized) {
    mbedtls_sha256_free(&uploadSha);
    uploadShaInitialized = false;
  }
  SD_MMC.remove(IMAGE_PATH);
  storedSize = 0;
  setError("UPLOAD ABORTED", false);
}

bool startTransfer() {
  const bool retryingStagedImage = state == State::Error && storedSize > 0;
  if ((state != State::Ready && !retryingStagedImage) || !mainLink ||
      !mainLink->connected()) {
    if (!mainLink || !mainLink->connected()) setError("MAIN OFFLINE", false);
    return false;
  }
  transferFile = SD_MMC.open(IMAGE_PATH, FILE_READ);
  if (!transferFile || transferFile.size() != storedSize) {
    setError("STAGED IMAGE MISSING", false);
    return false;
  }
  transferId = esp_random();
  if (transferId == 0) transferId = 1;
  transferOffset = 0;
  retryCount = 0;
  sessionRecoveryCount = 0;
  lastReplyGeneration = mainLink->firmwareReplyGeneration();
  if (!sendBegin()) {
    setError("CANNOT CONTACT MAIN", false);
    return false;
  }
  state = State::BeginWait;
  errorMessage[0] = '\0';
  return true;
}

void cancelTransfer() {
  if (mainLink && transferId != 0) mainLink->sendFirmwareAbort(transferId);
  if (transferFile) transferFile.close();
  state = storedSize > 0 ? State::Ready : State::Empty;
  transferId = 0;
  transferOffset = 0;
  errorMessage[0] = '\0';
}

void poll() {
  if (!mainLink ||
      (state != State::BeginWait && state != State::ChunkWait &&
       state != State::EndWait))
    return;

  const uint32_t generation = mainLink->firmwareReplyGeneration();
  if (generation != lastReplyGeneration) {
    lastReplyGeneration = generation;
    const auto& reply = mainLink->firmwareReply();
    if (reply.transferId == transferId) {
      if (reply.status != HmiProtocol::FirmwareStatus::Accepted) {
        if ((state == State::ChunkWait || state == State::EndWait) &&
            reply.status == HmiProtocol::FirmwareStatus::InvalidTransfer &&
            sessionRecoveryCount < 4) {
          ++sessionRecoveryCount;
          transferOffset = 0;
          retryCount = 0;
          snprintf(errorMessage, sizeof(errorMessage), "RESYNC MAIN %u/4",
                   sessionRecoveryCount);
          if (!sendBegin()) {
            setError("CANNOT RESYNC MAIN", false);
            return;
          }
          state = State::BeginWait;
          return;
        }
        if (state == State::ChunkWait &&
            reply.status == HmiProtocol::FirmwareStatus::InvalidOffset &&
            reply.nextOffset <= storedSize) {
          advanceAfterOffset(reply.nextOffset);
          return;
        }
        setError(firmwareStatusName(reply.status), false);
        return;
      }
      if (state == State::BeginWait &&
          reply.stage == HmiProtocol::FirmwareStage::Begin) {
        advanceAfterOffset(reply.nextOffset);
        return;
      }
      if (state == State::ChunkWait &&
          reply.stage == HmiProtocol::FirmwareStage::Chunk) {
        advanceAfterOffset(reply.nextOffset);
        return;
      }
      if (state == State::EndWait &&
          reply.stage == HmiProtocol::FirmwareStage::End) {
        transferOffset = storedSize;
        transferFile.close();
        state = State::Complete;
        return;
      }
    }
  }

  if (millis() - lastSendMs < REPLY_TIMEOUT_MS) return;
  if (++retryCount > MAX_RETRIES) {
    setError("MAIN UART TIMEOUT");
    return;
  }
  bool sent = false;
  if (state == State::BeginWait) sent = sendBegin();
  else if (state == State::ChunkWait) sent = resendChunk();
  else if (state == State::EndWait) sent = sendEnd();
  if (!sent) setError("MAIN UART SEND ERROR");
}

bool fileReady() {
  return state == State::Ready || (state == State::Error && storedSize > 0);
}

bool busy() {
  return state == State::Uploading || state == State::BeginWait ||
         state == State::ChunkWait || state == State::EndWait;
}

uint32_t imageSize() { return storedSize; }

uint8_t progressPercent() {
  if (storedSize == 0) return 0;
  if (state == State::Complete) return 100;
  return static_cast<uint8_t>(min<uint32_t>(99, transferOffset * 100U / storedSize));
}

const char* stateText() {
  switch (state) {
    case State::Empty: return "NO FILE";
    case State::Uploading: return "UPLOADING TO TF";
    case State::Ready: return "READY TO INSTALL";
    case State::BeginWait: return "PREPARING MAIN";
    case State::ChunkWait: return "WRITING MAIN";
    case State::EndWait: return "VERIFYING MAIN";
    case State::Complete: return "COMPLETE - MAIN REBOOTING";
    case State::Error: return "FAILED";
    default: return "UNKNOWN";
  }
}

const char* errorText() { return errorMessage; }

String statusJson() {
  String json;
  json.reserve(220);
  json += F("{\"state\":\"");
  json += stateText();
  json += F("\",\"error\":\"");
  json += errorMessage;
  json += F("\",\"ready\":");
  json += fileReady() ? F("true") : F("false");
  json += F(",\"busy\":");
  json += busy() ? F("true") : F("false");
  json += F(",\"size\":");
  json += storedSize;
  json += F(",\"offset\":");
  json += transferOffset;
  json += F(",\"progress\":");
  json += progressPercent();
  json += F(",\"transferId\":");
  json += transferId;
  json += F(",\"recoveries\":");
  json += sessionRecoveryCount;
  if (mainLink) {
    const auto& reply = mainLink->firmwareReply();
    json += F(",\"replyTransferId\":");
    json += reply.transferId;
    json += F(",\"replyStage\":");
    json += static_cast<unsigned>(reply.stage);
    json += F(",\"replyStatus\":");
    json += static_cast<unsigned>(reply.status);
    json += F(",\"replyNextOffset\":");
    json += reply.nextOffset;
  }
  json += F("}");
  return json;
}

}  // namespace MainFirmwareUpdater
