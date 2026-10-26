#pragma once
#include "protocol/pressure_capture.h"

namespace PressureCapture {
// Main-loop request endpoint; only Arm/Clear allocate/free, while stopped.
void request(const PressureCaptureWire::Request& request,
             PressureCaptureWire::Reply& reply, bool idle, bool stopped,
             bool available);
// ADC task is the sole recorder. No SD/serial work or allocation here.
bool tick(bool brew, bool idle, uint32_t nowMs, uint32_t nowUs);
void record(int16_t raw, bool published, uint32_t nowUs,
            float controllerFilteredBar, float pumpPercent);
void busy();
bool engaged();
}
