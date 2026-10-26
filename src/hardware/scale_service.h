#pragma once

#include <Arduino.h>

#include "protocol/hmi_protocol.h"

namespace ScaleService {

void begin();
void beginShotTare();
bool shotTarePending();
bool preparingShot();
void endShot();
bool allowedForYield();
void acceptHmiSample(const HmiProtocol::ScaleSamplePayload& sample,
                     uint32_t receivedAtMs);

bool isOnline();
bool isStable();
float getWeight();
HmiProtocol::ScaleSource source();
uint8_t batteryPercent();
uint32_t sampleAgeMs();
bool usingHmi();

}  // namespace ScaleService
