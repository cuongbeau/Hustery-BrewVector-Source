#include "src/model/pump_flow_calibration.h"

using namespace PumpFlowCalibration;

static_assert(interpolate(8.0f, 4.0f, 1.0f) == 8.0f,
              "Q1 must be the 1 bar anchor");
static_assert(interpolate(8.0f, 4.0f, 9.0f) == 4.0f,
              "Q9 must be the 9 bar anchor");
static_assert(interpolate(8.0f, 4.0f, 5.0f) == 6.0f,
              "The pump model must interpolate linearly");
static_assert(legacyQ1(0.156f) > 7.37f && legacyQ1(0.156f) < 7.38f,
              "Legacy migration must preserve the previous 1 bar flow");
static_assert(legacyQ9(0.156f) > 3.99f && legacyQ9(0.156f) < 4.00f,
              "Legacy migration must preserve the previous 9 bar flow");
