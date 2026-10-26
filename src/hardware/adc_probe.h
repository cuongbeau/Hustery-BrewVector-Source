#pragma once
#include "protocol/adc_probe.h"
namespace AdcProbe {
void request(const AdcProbeWire::Request&, AdcProbeWire::Reply&, bool allowed);
bool engaged();
// ADC owner only. true = consumes this cycle, skip normal/capture sampling.
bool tick(bool safe, void (*publish)(int16_t));
}
