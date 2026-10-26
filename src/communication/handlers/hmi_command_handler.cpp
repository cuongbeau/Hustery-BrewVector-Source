#include "src/communication/hmi_link_internal.h"

#include <Arduino.h>
#include <string.h>

#include "src/app/orchestrator.h"
#include "src/hardware/heater_control.h"
#include "src/app/quick_yield.h"

#include "src/control/profile_engine.h"
#include "src/hardware/valve_sense.h"
#include "src/hardware/pump_control.h"
#include "src/recovery/recovery_portal.h"

namespace HmiLinkInternal {
namespace CommandHandler {
namespace {
bool hasLastCommand = false;
uint16_t lastCommandSequence = 0;
HmiProtocol::CommandReplyPayload lastCommandReply{};
uint32_t remoteStartWindowEndMs = 0;
uint32_t remoteStopGuardEndMs = 0;
bool deferredStop = false;
bool stoppedResetReplyPending = false;
uint32_t stoppedResetRequestedMs = 0;
HmiProtocol::CommandResult executeCommand(
    const HmiProtocol::CommandRequestPayload& request) {
  if (FirmwareReceiver::active())
    return HmiProtocol::CommandResult::RejectedState;
  const State state = Orchestrator::getState();
  const uint32_t nowMs = millis();
  switch (request.command) {
    case HmiProtocol::Command::BeginYieldEdit:
    case HmiProtocol::Command::KeepYieldEdit:
    case HmiProtocol::Command::ApplyYieldEdit:
    case HmiProtocol::Command::CancelYieldEdit:
      return QuickYield::command(static_cast<uint8_t>(request.command)-10,request.argument)
          ? HmiProtocol::CommandResult::Accepted : HmiProtocol::CommandResult::RejectedState;
    case HmiProtocol::Command::ResetStoppedPsm:
      if (request.argument != 0)
        return HmiProtocol::CommandResult::InvalidArgument;
      if (state != IDLE || deferredStop || RecoveryPortal::updateActive() ||
          (remoteStartWindowEndMs != 0 &&
           static_cast<int32_t>(remoteStartWindowEndMs - nowMs) > 0) ||
          !PumpPSM::requestStoppedReset())
        return HmiProtocol::CommandResult::RejectedState;
      stoppedResetReplyPending = true;
      stoppedResetRequestedMs = nowMs;
      return HmiProtocol::CommandResult::Accepted; // Reply deferred below.

    case HmiProtocol::Command::StartBrew:
      if ((state != IDLE && state != SHOT_END) ||
          ValveSense::isMachineValveActive() ||
          (remoteStartWindowEndMs != 0 &&
           static_cast<int32_t>(remoteStartWindowEndMs - nowMs) > 0))
        return HmiProtocol::CommandResult::RejectedState;
      if (!Heater::brewReady())
        return Heater::preheat() ? HmiProtocol::CommandResult::Accepted : HmiProtocol::CommandResult::RejectedState;
      Orchestrator::startShotFromRemote();
      remoteStartWindowEndMs = nowMs + 3000;
      return HmiProtocol::CommandResult::Accepted;

    case HmiProtocol::Command::StartFlush: {
      if ((state != IDLE && state != SHOT_END) ||
          ValveSense::isMachineValveActive() ||
          (remoteStartWindowEndMs != 0 &&
           static_cast<int32_t>(remoteStartWindowEndMs - nowMs) > 0))
        return HmiProtocol::CommandResult::RejectedState;
      const uint32_t durationMs = request.argument == 0 ? 5000 : request.argument;
      if (durationMs < 1000 || durationMs > 30000)
        return HmiProtocol::CommandResult::InvalidArgument;
      Orchestrator::startOrStopFlushFromRemote(durationMs);
      remoteStartWindowEndMs = nowMs + 3000;
      return HmiProtocol::CommandResult::Accepted;
    }

    case HmiProtocol::Command::StartPressureCalibration:
      if ((state != IDLE && state != SHOT_END) ||
          ValveSense::isMachineValveActive() ||
          (remoteStartWindowEndMs != 0 &&
           static_cast<int32_t>(remoteStartWindowEndMs - nowMs) > 0))
        return HmiProtocol::CommandResult::RejectedState;
      if (!Orchestrator::startPressureCalibrationFromRemote())
        return HmiProtocol::CommandResult::RejectedState;
      remoteStartWindowEndMs = nowMs + 3000;
      return HmiProtocol::CommandResult::Accepted;

    case HmiProtocol::Command::Stop:
      if (remoteStopGuardEndMs != 0 &&
          static_cast<int32_t>(remoteStopGuardEndMs - nowMs) > 0)
        return HmiProtocol::CommandResult::Accepted;
      if (remoteStartWindowEndMs != 0 &&
          static_cast<int32_t>(remoteStartWindowEndMs - nowMs) > 0 &&
          !ValveSense::isMachineValveActive()) {
        deferredStop = true;
        return HmiProtocol::CommandResult::Accepted;
      }
      if (state == FLUSH_RUN)
        Orchestrator::startOrStopFlushFromRemote();
      else if (state == CALIBRATION_RUN)
        Orchestrator::stopPressureCalibration("hmi-stop");
      else
        Orchestrator::stopShotFromRemote();
      remoteStopGuardEndMs = nowMs + 750;
      return HmiProtocol::CommandResult::Accepted;

    case HmiProtocol::Command::DismissResult:
      if (state == SHOT_END) {
        Orchestrator::forceIdle();
        return HmiProtocol::CommandResult::Accepted;
      }
      return state == IDLE ? HmiProtocol::CommandResult::Accepted
                           : HmiProtocol::CommandResult::RejectedState;

    case HmiProtocol::Command::SelectProfile:
      if (state != IDLE && state != SHOT_END)
        return HmiProtocol::CommandResult::RejectedState;
      if (request.argument >= static_cast<uint32_t>(profilesCount))
        return HmiProtocol::CommandResult::InvalidArgument;
      setCurrentProfileIndex(static_cast<int>(request.argument));
      return HmiProtocol::CommandResult::Accepted;

    default:
      return HmiProtocol::CommandResult::InvalidCommand;
  }
}

void serviceDeferredStop(uint32_t nowMs) {
  if (!deferredStop) return;
  const bool valveActive = ValveSense::isMachineValveActive();
  const bool startWindowExpired =
      remoteStartWindowEndMs == 0 ||
      static_cast<int32_t>(remoteStartWindowEndMs - nowMs) <= 0;
  if (!valveActive && !startWindowExpired) return;

  if (Orchestrator::getState() == FLUSH_RUN)
    Orchestrator::startOrStopFlushFromRemote();
  else if (Orchestrator::getState() == CALIBRATION_RUN)
    Orchestrator::stopPressureCalibration("hmi-deferred-stop");
  else
    Orchestrator::stopShotFromRemote();
  deferredStop = false;
  remoteStopGuardEndMs = nowMs + 750;
}

void sendCommandReply(const HmiProtocol::CommandReplyPayload& reply) {
  sendFrame(HmiProtocol::MessageType::CommandReply, &reply, sizeof(reply));
}

void finishStoppedResetReply(uint32_t nowMs) {
  if (!stoppedResetReplyPending) return;
  if (nowMs - stoppedResetRequestedMs >= 250U ||
      FirmwareReceiver::active() || RecoveryPortal::updateActive() ||
      Orchestrator::getState() != IDLE)
    PumpPSM::cancelStoppedReset();
  const auto result = PumpPSM::stoppedResetState();
  if (result == PumpPSM::StoppedResetState::Pending) return;
  lastCommandReply.result = result == PumpPSM::StoppedResetState::Applied
      ? HmiProtocol::CommandResult::Accepted
      : HmiProtocol::CommandResult::RejectedState;
  stoppedResetReplyPending = false;
  sendCommandReply(lastCommandReply);
}

}  // namespace

bool handle(const HmiProtocol::FrameHeader& header, const uint8_t* payload,
            uint32_t nowMs) {
  if (header.type != HmiProtocol::MessageType::CommandRequest ||
      header.payloadLength != sizeof(HmiProtocol::CommandRequestPayload))
    return false;
  if (hasLastCommand && header.sequence == lastCommandSequence) {
    // Retransmission cannot reset again or report success before execution.
    if (!stoppedResetReplyPending) sendCommandReply(lastCommandReply);
    return true;
  }
  if (stoppedResetReplyPending) {
    // New user commands take priority over a not-yet-executed diagnostic.
    PumpPSM::cancelStoppedReset();
    finishStoppedResetReply(nowMs);
  }
  HmiProtocol::CommandRequestPayload request{};
  memcpy(&request, payload, sizeof(request));
  lastCommandReply = {
      header.sequence, request.command, executeCommand(request),
      static_cast<uint8_t>(protocolState(Orchestrator::getState())), 0};
  lastCommandSequence = header.sequence;
  hasLastCommand = true;
  if (!stoppedResetReplyPending) sendCommandReply(lastCommandReply);
  if (request.command == HmiProtocol::Command::SelectProfile &&
      lastCommandReply.result == HmiProtocol::CommandResult::Accepted)
    ProfileHandler::sendList();
  return true;
}

void poll(uint32_t nowMs) {
  serviceDeferredStop(nowMs);
  finishStoppedResetReply(nowMs);
}

}  // namespace CommandHandler
}  // namespace HmiLinkInternal
