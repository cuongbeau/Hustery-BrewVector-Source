#pragma once

#include <Arduino.h>

#include "hmi_protocol.h"

class MainLink;

namespace CalibrationWebBridge {

bool execute(MainLink& link, HmiProtocol::CalibrationOperation operation,
             HmiProtocol::CalibrationReplyPayload& reply, String& error,
             float value1 = 0.0f, float value2 = 0.0f,
             float value3 = 0.0f);

}  // namespace CalibrationWebBridge
