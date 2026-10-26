// File: src/hardware/sensor_reader.h
// Tách từ control_logic.h — đọc sensor (temp, pressure, flow, derivatives)
// Refactor: 2026-04-20

#ifndef SENSOR_READER_H
#define SENSOR_READER_H

#include <Arduino.h>
#include "interfaces/shot_types.h"
#include "protocol/heater_sensor_wire.h"

namespace SensorReader {
  void init();
  void update(ShotData& shot, bool isShotActive, unsigned long shotStart);
  float getTempFiltered();
  HeaterSensorWire::Status temperatureDiagnostics();
  float adcVoltAveragedWindow(uint32_t win_ms = 10);
}

#endif // SENSOR_READER_H
