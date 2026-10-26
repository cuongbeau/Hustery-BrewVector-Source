#pragma once

#include <stdint.h>

#include "protocol/hmi_protocol.h"
#include "interfaces/system_state.h"

namespace HmiLinkInternal {

uint16_t sendFrame(HmiProtocol::MessageType type, const void* payload,
                   uint16_t payloadLength);
HmiProtocol::MachineState protocolState(State state);

namespace CommandHandler {
bool handle(const HmiProtocol::FrameHeader& header, const uint8_t* payload,
            uint32_t nowMs);
void poll(uint32_t nowMs);
}

namespace ProfileHandler {
bool handle(const HmiProtocol::FrameHeader& header, const uint8_t* payload,
            uint32_t nowMs);
void sendList();
}

namespace CalibrationHandler {
bool handle(const HmiProtocol::FrameHeader& header, const uint8_t* payload,
            uint32_t nowMs);
}

namespace PressureCaptureHandler {
bool handle(const HmiProtocol::FrameHeader& header, const uint8_t* payload,
            uint32_t nowMs);
}

namespace AdcProbeHandler {
bool handle(const HmiProtocol::FrameHeader&, const uint8_t*, uint32_t);
}

namespace FirmwareReceiver {
bool handle(const HmiProtocol::FrameHeader& header, const uint8_t* payload,
            uint32_t nowMs);
void poll(uint32_t nowMs);
bool active();
}

}  // namespace HmiLinkInternal
