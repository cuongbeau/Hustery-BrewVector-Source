// File: src/app/calibration_store.h
// Tách từ calibration_store.h gốc — chỉ giữ SPIFFS load/save functions
// Struct definitions đã chuyển sang interfaces/
// Refactor: 2026-04-20

#ifndef CALIBRATION_STORE_H
#define CALIBRATION_STORE_H

#include <Arduino.h>
#include "interfaces/calibration_types.h"
#include "interfaces/profile_types.h"

bool ensureFS();

void loadPressureCal();
void savePressureCal();

void loadThermalCal();
bool saveThermalCal(float offsetC);

void loadFlowCal();
void saveFlowCal();

void loadProfilesJson();
bool saveProfilesJson();

void loadLastShot();
void saveLastShot();

#endif // CALIBRATION_STORE_H
