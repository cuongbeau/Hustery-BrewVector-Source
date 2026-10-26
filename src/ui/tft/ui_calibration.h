#pragma once

#include "interfaces/shot_types.h"

void uiCalibrationStart();
void uiCalibrationUpdate(const ShotData& shotData);
void uiCalibrationDeinit();

