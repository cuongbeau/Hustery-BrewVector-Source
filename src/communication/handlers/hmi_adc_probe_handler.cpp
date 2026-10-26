#include "src/communication/hmi_link_internal.h"
#include "src/communication/hmi_link.h"
#include "src/hardware/adc_probe.h"
#include "src/hardware/pressure_capture.h"
#include "src/hardware/ads1115_interface.h"
#include "src/app/orchestrator.h"
#include "src/recovery/recovery_portal.h"
#include <string.h>
namespace HmiLinkInternal { namespace AdcProbeHandler {
bool handle(const HmiProtocol::FrameHeader& header, const uint8_t* payload, uint32_t) {
  using namespace AdcProbeWire;
  if (header.type != HmiProtocol::MessageType::AdcProbeRequest) return false;
  if (header.payloadLength != sizeof(Request) || !HmiLink::compatible()) return true;
  Request request{}; memcpy(&request, payload, sizeof(request));
  const auto pressure = ADS1115_Interface::getPressureSnapshot();
  const bool allowed = Orchestrator::getState() == IDLE &&
      !PressureCapture::engaged() && !FirmwareReceiver::active() &&
      !RecoveryPortal::updateActive() && pressure.valid && millis() - pressure.sampledAtMs < 250U;
  Reply reply{}; AdcProbe::request(request, reply, allowed);
  sendFrame(HmiProtocol::MessageType::AdcProbeReply, &reply, sizeof(reply));
  return true;
}
} }
