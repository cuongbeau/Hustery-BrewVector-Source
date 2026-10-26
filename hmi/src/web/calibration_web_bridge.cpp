#include "web/calibration_web_bridge.h"

#include "communication/main_link.h"
#include "web/owner_call.h"
#include "web/web_transaction.h"

namespace {
constexpr uint32_t RESPONSE_TIMEOUT_MS = 1000;

const char* statusText(HmiProtocol::CalibrationStatus status) {
  using Status = HmiProtocol::CalibrationStatus;
  switch (status) {
    case Status::RejectedState:
      return "Main state does not allow this calibration";
    case Status::InvalidValue: return "Calibration value is out of range";
    case Status::SensorError: return "Pressure sensor reading is invalid";
    case Status::Conflict: return "Offset changed; reload before saving";
    case Status::StorageError: return "Cannot save calibration on Main";
    default: return "Calibration request failed";
  }
}
}  // namespace

namespace CalibrationWebBridge {

bool execute(MainLink& link, HmiProtocol::CalibrationOperation operation,
             HmiProtocol::CalibrationReplyPayload& reply, String& error,
             float value1, float value2, float value3) {
  WebTransaction transaction(link);
  if(!transaction){error="Main transaction busy";return false;}
  if (!ownerValue([&](){return link.connected();})) {
    error = "Main offline";
    return false;
  }
  const uint32_t generation = ownerValue([&](){return link.calibrationReplyGeneration();});
  if (!ownerValue([&](){return link.requestCalibration(operation, value1, value2, value3);})) {
    error = "Cannot send calibration request";
    return false;
  }
  const uint32_t started = millis();
  while (millis() - started < RESPONSE_TIMEOUT_MS) {

    if (ownerValue([&](){return link.calibrationReplyGeneration();}) != generation) {
      reply = ownerValue([&](){return link.calibrationReply();});
      if (reply.operation != operation) {
        error = "Unexpected calibration reply";
        return false;
      }
      if (reply.status != HmiProtocol::CalibrationStatus::Accepted) {
        error = statusText(reply.status);
        return false;
      }
      return true;
    }
    delay(2);
  }
  error = "Main calibration response timeout";
  return false;
}

}  // namespace CalibrationWebBridge
