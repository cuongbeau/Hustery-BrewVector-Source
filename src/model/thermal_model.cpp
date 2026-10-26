#include "thermal_model.h"
#include <math.h>

void ThermalModel::begin() { reset(); }

void ThermalModel::reset(float blockTempC) {
  st.blockTempC = blockTempC;
  st.brewTempEstC = isfinite(blockTempC)
      ? ThermalCalibration::estimate(blockTempC, offsetC()) : NAN;
  st.pathTempEstC = st.brewTempEstC;
  st.recentWaterLoadG = 0.0f;
}

ThermalModelState ThermalModel::update(float blockTempC, float flowGps,
                                      bool waterRunning, float dtS) {
  (void)flowGps;
  (void)waterRunning;
  (void)dtS;
  reset(blockTempC);
  return st;
}
