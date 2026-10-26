#pragma once

#include <Arduino.h>
#include "protocol/hmi_protocol.h"

namespace HmiLink {

void begin();
void poll();
bool requestScaleTare(const HmiProtocol::ScaleTareRequestPayload& request);
bool connected();
bool compatible();
uint16_t remoteSchemaRevision();
uint32_t lastFrameAgeMs();
uint32_t validFrameCount();
uint32_t crcErrorCount();
uint32_t framingErrorCount();
uint32_t staleFrameResetCount();
uint32_t reconnectCount();
uint32_t lastErrorAgeMs();
bool firmwareUpdateActive();
uint32_t controlTraceDropCount();
void queueShotEnd(HmiProtocol::ShotStopReason stopReason,
                  uint8_t profileExitReason, uint8_t stepIndex,
                  bool valveActive);

}  // namespace HmiLink
