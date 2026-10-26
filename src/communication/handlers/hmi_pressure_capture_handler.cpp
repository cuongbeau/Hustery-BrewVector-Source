#include "src/communication/hmi_link_internal.h"
#include "src/communication/hmi_link.h"
#include "src/hardware/pressure_capture.h"
#include "src/hardware/adc_probe.h"
#include "src/hardware/ads1115_interface.h"
#include "src/hardware/pump_control.h"
#include "src/hardware/valve_sense.h"
#include "src/app/orchestrator.h"
#include "src/recovery/recovery_portal.h"
#include <string.h>

namespace HmiLinkInternal { namespace PressureCaptureHandler {
bool handle(const HmiProtocol::FrameHeader& header, const uint8_t* payload, uint32_t) {
  using namespace PressureCaptureWire;
  if (header.type != HmiProtocol::MessageType::PressureCaptureRequest) return false;
  if (header.payloadLength != sizeof(Request) || !HmiLink::compatible()) return true;
  Request request{};
  memcpy(&request, payload, sizeof(request));
  const State state = Orchestrator::getState();
  const bool stopped = (state == IDLE || state == SHOT_END) &&
      !ValveSense::isMachineValveActive() && PumpPSM::getControllerTelemetry().appliedPowerPercent == 0;
  const bool available = !FirmwareReceiver::active() && !RecoveryPortal::updateActive() && !AdcProbe::engaged();
  Reply reply{};
  PressureCapture::request(request, reply, state == IDLE && available,
      stopped && available, ADS1115_Interface::getPressureSnapshot().valid);
  // Only metadata while recording; the bulk rows are fetched after stopping.
  const uint16_t bytes = offsetof(Reply, samples) + reply.count * sizeof(Sample);
  sendFrame(HmiProtocol::MessageType::PressureCaptureReply, &reply, bytes);
  return true;
}
} }
