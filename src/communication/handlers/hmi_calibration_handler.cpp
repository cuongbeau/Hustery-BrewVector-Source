#include "src/communication/hmi_link_internal.h"
#include "src/communication/hmi_link.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "src/app/calibration_store.h"
#include "src/app/orchestrator.h"

#include "src/hardware/ads1115_interface.h"
#include "src/hardware/pressure_capture.h"
#include "src/hardware/adc_probe.h"
#include "src/hardware/pump_control.h"
#include "src/hardware/valve_sense.h"
#include "src/model/flow_model.h"
#include "src/model/thermal_model.h"
#include "src/recovery/recovery_portal.h"

namespace HmiLinkInternal {
namespace CalibrationHandler {
namespace {void handleCalibration(
    const HmiProtocol::CalibrationRequestPayload& request) {
  using Operation = HmiProtocol::CalibrationOperation;
  using Status = HmiProtocol::CalibrationStatus;
  Status status = Status::Accepted;
  const State state = Orchestrator::getState();
  const bool idle = state == IDLE || state == SHOT_END;
  const float voltage = ADS1115_Interface::getAveragedVoltage();

  if (FirmwareReceiver::active() || ((PressureCapture::engaged() || AdcProbe::engaged()) &&
      request.operation != Operation::ReadAll)) {
    status = Status::RejectedState;
  } else if (request.operation == Operation::ZeroPressure) {
    if (!idle || ValveSense::isMachineValveActive()) {
      status = Status::RejectedState;
    } else if (!isfinite(voltage)) {
      status = Status::SensorError;
    } else {
      gPress.offset = voltage;
      savePressureCal();
    }
  } else if (request.operation == Operation::SetPressureAt) {
    const float delta = voltage - gPress.offset;
    if (state != CALIBRATION_RUN || !ValveSense::isMachineValveActive()) {
      status = Status::RejectedState;
    } else if (!isfinite(request.value1) || request.value1 <= 0.0f ||
        request.value1 > 16.0f) {
      status = Status::InvalidValue;
    } else if (!isfinite(voltage) || fabsf(delta) < 1e-4f) {
      status = Status::SensorError;
    } else {
      gPress.scale = request.value1 / delta;
      savePressureCal();
      Orchestrator::stopPressureCalibration("capture-saved");
    }
  } else if (request.operation == Operation::SetFlow) {
    if (!idle) {
      status = Status::RejectedState;
    } else if (!isfinite(request.value1) || request.value1 < 0.5f ||
               request.value1 > 20.0f || !isfinite(request.value2) ||
               request.value2 < 0.25f || request.value2 >= request.value1) {
      status = Status::InvalidValue;
    } else {
      gFlow.q1MlPerS = request.value1;
      gFlow.q9MlPerS = request.value2;
      saveFlowCal();
      g_flowCalNeedUpdate = true;
    }
  } else if (request.operation == Operation::SetThermalOffset) {
    if (!HmiLink::compatible() || !idle || ValveSense::isMachineValveActive() ||
        RecoveryPortal::updateActive()) {
      status = Status::RejectedState;
    } else if (!ThermalCalibration::valid(request.value1) ||
               !ThermalCalibration::valid(request.value2)) {
      status = Status::InvalidValue;
    } else if (fabsf(request.value2 - thermal.offsetC()) > 0.001f) {
      status = Status::Conflict;
    } else if (!saveThermalCal(request.value1)) {
      status = Status::StorageError;
    }
  } else if (request.operation != Operation::ReadAll) {
    status = Status::InvalidValue;
  }

  const float pressure =
      isfinite(voltage) ? max(0.0f, (voltage - gPress.offset) * gPress.scale)
                        : 0.0f;
  const float cps = PumpPSM::getCPS();
  const float rawFpc = flow.getRawFPCAtPressure(pressure, cps);
  HmiProtocol::CalibrationReplyPayload reply{};
  reply.requestId = request.requestId;
  reply.operation = request.operation;
  reply.status = status;
  reply.machineState = static_cast<uint8_t>(protocolState(state));
  reply.pressureOffset = gPress.offset;
  reply.pressureScale = gPress.scale;
  reply.pressureAdcVolts = voltage;
  reply.pressureBar = pressure;
  reply.flowQ1MlPerS = gFlow.q1MlPerS;
  reply.flowQ9MlPerS = gFlow.q9MlPerS;
  reply.flowModelReserved = 0.0f;
  reply.zeroCrossHz = PumpPSM::getZCHz();
  reply.pumpCps = cps;
  reply.pumpFlowPerClickMl = rawFpc;
  reply.freeFlowPerClickMl = flow.getFlowPerClickAtZeroBar(cps);
  reply.actualPumpFlowMlPerS = cps * rawFpc;
  reply.thermalOffsetC = thermal.offsetC();
  sendFrame(HmiProtocol::MessageType::CalibrationReply, &reply,
            sizeof(reply));
}

}  // namespace

bool handle(const HmiProtocol::FrameHeader& header, const uint8_t* payload,
            uint32_t nowMs) {
  (void)nowMs;
  if (header.type != HmiProtocol::MessageType::CalibrationRequest ||
      header.payloadLength != sizeof(HmiProtocol::CalibrationRequestPayload))
    return false;
  HmiProtocol::CalibrationRequestPayload request{};
  memcpy(&request, payload, sizeof(request));
  handleCalibration(request);
  return true;
}

}  // namespace CalibrationHandler
}  // namespace HmiLinkInternal
