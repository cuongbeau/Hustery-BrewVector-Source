#pragma once
#include "protocol/heater_wire.h"
#include "protocol/heater_sensor_wire.h"
namespace Heater {
void begin();
void sample(float celsius, uint8_t sensorIssues = 0, uint32_t sensorFrame = 0);
void heartbeat(bool waterRunning, float estimatedFlow = 0, bool valveOpen = false, bool flowValid = false,
               float pressure = 0, float duty = 0);
void inhibit();
void releaseInhibit();
HeaterWire::Status status();
void diagnostics(HeaterSensorWire::Status&);
void request(const HeaterWire::Request&, bool allowed);
bool brewReady();
bool preheat();
}
