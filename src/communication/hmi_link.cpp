#include "src/communication/hmi_link.h"
#include "src/hardware/heater_control.h"
#include "protocol/heater_legacy_wire.h"
#include "src/hardware/sensor_reader.h"
#include "src/recovery/recovery_portal.h"
#include "src/hardware/ads1115_interface.h"

#include <Arduino.h>
#include <string.h>

#include "protocol/hmi_protocol.h"
#include "interfaces/system_state.h"
#include "src/app/orchestrator.h"
#include "src/app/quick_yield.h"
#include "src/control/profile_engine.h"
#include "src/communication/hmi_link_internal.h"
#include "src/control/pressure_flow_control.h"
#include "src/control/shot_aggregator.h"
#include "src/hardware/pump_control.h"
#include "src/hardware/scale_service.h"

namespace {
constexpr int MAIN_UART_RX_PIN = 39;
constexpr int MAIN_UART_TX_PIN = 1;
constexpr uint32_t UART_BAUD = 230400;
constexpr uint32_t HEARTBEAT_PERIOD_MS = 500;
constexpr uint32_t CONTROL_TRACE_PERIOD_MS = 40;
constexpr uint32_t LINK_TIMEOUT_MS = 3500;
constexpr uint32_t PARTIAL_FRAME_TIMEOUT_MS = 120;
constexpr size_t MAX_PAYLOAD = 512;
constexpr size_t MAX_FRAME = sizeof(HmiProtocol::FrameHeader) + MAX_PAYLOAD + 2;
constexpr size_t RX_CAPACITY = 2048;
constexpr size_t TX_CAPACITY = 512;

HardwareSerial hmiSerial(1);
uint8_t rxBuffer[RX_CAPACITY] = {};
size_t rxCount = 0;
uint16_t txSequence = 0;
uint32_t lastHeartbeatTxMs = 0;
uint32_t lastTelemetryTxMs = 0;
uint32_t lastControlTraceTxMs = 0;
uint32_t controlTraceSequence = 0;
uint32_t controlTraceDrops = 0;
uint32_t controlTraceSessionId = 0;
bool controlTraceWasActive = false;
HmiProtocol::ShotEndPayload pendingShotEnd{};
bool shotEndPending = false;
uint32_t shotEndQueuedAtMs = 0;
uint32_t lastValidFrameMs = 0;
uint32_t lastRxByteMs = 0;
uint16_t hmiSchemaRevision = 0;
uint8_t hmiFeatureFlags = 0;
uint32_t validFramesRx = 0;
uint32_t crcErrorsRx = 0;
uint32_t framingErrorsRx = 0;
uint32_t staleFrameResets = 0;
uint32_t linkReconnects = 0;
uint32_t lastUartErrorMs = 0;
}  // namespace

namespace HmiLinkInternal {void discardBytes(size_t count) {
  if (count >= rxCount) {
    rxCount = 0;
    return;
  }
  memmove(rxBuffer, rxBuffer + count, rxCount - count);
  rxCount -= count;
}

void markValidFrame(uint32_t nowMs) {
  if (lastValidFrameMs == 0 || nowMs - lastValidFrameMs >= LINK_TIMEOUT_MS)
    ++linkReconnects;
  lastValidFrameMs = nowMs;
  ++validFramesRx;
}

uint16_t sendFrame(HmiProtocol::MessageType type, const void* payload,
                   uint16_t payloadLength) {
  if (payloadLength > MAX_PAYLOAD) return 0;
  uint8_t frame[MAX_FRAME];
  ++txSequence;
  if (txSequence == 0) ++txSequence;
  HmiProtocol::FrameHeader header{HmiProtocol::MAGIC, HmiProtocol::VERSION,
                                  type, payloadLength, txSequence};
  memcpy(frame, &header, sizeof(header));
  if (payloadLength > 0)
    memcpy(frame + sizeof(header), payload, payloadLength);
  const size_t bodyLength = sizeof(header) + payloadLength;
  const uint16_t crc = HmiProtocol::crc16Ccitt(frame, bodyLength);
  frame[bodyLength] = static_cast<uint8_t>(crc & 0xFF);
  frame[bodyLength + 1] = static_cast<uint8_t>(crc >> 8);
  hmiSerial.write(frame, bodyLength + 2);
  return header.sequence;
}

bool sendFrameIfWritable(HmiProtocol::MessageType type, const void* payload,
                         uint16_t payloadLength) {
  if (payloadLength > MAX_PAYLOAD) return false;
  const size_t frameLength = sizeof(HmiProtocol::FrameHeader) + payloadLength + 2;
  // Never wait behind UART output. Dropping a diagnostic sample is preferable
  // to delaying the next control/update pass.
  if (hmiSerial.availableForWrite() < static_cast<int>(frameLength)) return false;
  uint8_t frame[MAX_FRAME];
  ++txSequence;
  if (txSequence == 0) ++txSequence;
  HmiProtocol::FrameHeader header{HmiProtocol::MAGIC, HmiProtocol::VERSION,
                                  type, payloadLength, txSequence};
  memcpy(frame, &header, sizeof(header));
  memcpy(frame + sizeof(header), payload, payloadLength);
  const size_t bodyLength = sizeof(header) + payloadLength;
  const uint16_t crc = HmiProtocol::crc16Ccitt(frame, bodyLength);
  frame[bodyLength] = static_cast<uint8_t>(crc & 0xFF);
  frame[bodyLength + 1] = static_cast<uint8_t>(crc >> 8);
  hmiSerial.write(frame, frameLength);
  return true;
}

void sendHeartbeat(uint32_t nowMs) {
  HmiProtocol::HeartbeatPayload payload{nowMs,
      HmiProtocol::EndpointRole::MainController,
      HmiProtocol::HeartbeatFlags::SupportsControlTrace |
          HmiProtocol::HeartbeatFlags::SupportsPressureCapture |
          HmiProtocol::HeartbeatFlags::SupportsAdcProbe |
          HmiProtocol::HeartbeatFlags::SupportsHeater,
      HmiProtocol::SCHEMA_REVISION};
  sendFrame(HmiProtocol::MessageType::Heartbeat, &payload, sizeof(payload));
}

HmiProtocol::MachineState protocolState(State state) {
  switch (state) {
    case IDLE: return HmiProtocol::MachineState::Ready;
    case PROFILE_RUN: return HmiProtocol::MachineState::Brew;
    case FLUSH_RUN: return HmiProtocol::MachineState::Flush;
    case SHOT_END: return HmiProtocol::MachineState::Done;
    case HOTWATER_RUN: return HmiProtocol::MachineState::HotWater;
    case SETTINGS_MENU: return HmiProtocol::MachineState::Settings;
    case CALIBRATION_RUN: return HmiProtocol::MachineState::Calibration;
    default: return HmiProtocol::MachineState::Fault;
  }
}

void sendTelemetry(uint32_t nowMs) {
  const ControlTelemetrySnapshot snapshot = Orchestrator::telemetrySnapshot();

  HmiProtocol::TelemetryPayload payload{};
  payload.timestampMs = nowMs;
  payload.pressureBar = snapshot.pressureBar;
  payload.flowGps = snapshot.flowGps;
  payload.weightG = snapshot.weightG;
  payload.blockTempC = shot.blockTempC;
  payload.brewTempC = shot.brewTempEstC;
  payload.elapsedS = snapshot.elapsedS;
  payload.targetValue = snapshot.targetValue;
  payload.pumpPercent = snapshot.pumpPercent;
  payload.quickYieldG = QuickYield::displayed();
  payload.yieldEditToken = QuickYield::token();
  payload.quickYieldFlags = QuickYield::flags();
  payload.selectedProfileIndex = getCurrentProfileIndex();
  payload.machineState = static_cast<uint8_t>(protocolState(Orchestrator::getState()));
  payload.controlMode = snapshot.controlMode;
  payload.stepIndex = snapshot.currentStep > 0 ? snapshot.currentStep - 1 : 0;
  payload.stepCount = snapshot.totalSteps > 0 ? snapshot.totalSteps : 1;
  if (snapshot.scaleConnected)
    payload.flags |= HmiProtocol::TelemetryFlags::ScaleConnected;
  if (snapshot.scaleConnected && ScaleService::isStable())
    payload.flags |= HmiProtocol::TelemetryFlags::ScaleStable;
  payload.scaleSource = ScaleService::source();
  payload.scaleBatteryPercent = ScaleService::batteryPercent();
  const bool peerConnected =
      lastValidFrameMs != 0 && nowMs - lastValidFrameMs < LINK_TIMEOUT_MS;
  if (peerConnected)
    payload.flags |= HmiProtocol::TelemetryFlags::HmiPeerConnected;
  if (peerConnected &&
      hmiSchemaRevision == HmiProtocol::SCHEMA_REVISION)
    payload.flags |= HmiProtocol::TelemetryFlags::HmiPeerCompatible;
  snprintf(payload.profileName, sizeof(payload.profileName), "%s",
           snapshot.profileName && snapshot.profileName[0]
               ? snapshot.profileName : "OFF");
  snprintf(payload.phaseName, sizeof(payload.phaseName), "%s",
           snapshot.phaseName && snapshot.phaseName[0]
               ? snapshot.phaseName : snapshot.stateName);

  if (Orchestrator::getState() == FLUSH_RUN) {
    payload.targetValue = snapshot.stepMaxMs > 0
        ? snapshot.stepMaxMs * 0.001f : 5.0f;
  }
  sendFrame(HmiProtocol::MessageType::Telemetry, &payload, sizeof(payload));
}

bool traceableState(State state) {
  return state == PROFILE_RUN || state == HOTWATER_RUN;
}

void sendControlTrace(uint32_t nowMs) {
  const State state = Orchestrator::getState();
  const bool active = traceableState(state);
  if (active && !controlTraceWasActive) {
    controlTraceSequence = 0;
    controlTraceDrops = 0;
    // Unique enough for grouping one live UART stream. Avoid zero because it
    // is reserved as an invalid/legacy session marker on HMI.
    controlTraceSessionId = nowMs != 0 ? nowMs : 1;
    lastControlTraceTxMs = 0;
  }
  controlTraceWasActive = active;
  if (!active || hmiSchemaRevision != HmiProtocol::SCHEMA_REVISION ||
      (hmiFeatureFlags & HmiProtocol::HeartbeatFlags::SupportsControlTrace) == 0)
    return;
  if (nowMs - lastControlTraceTxMs < CONTROL_TRACE_PERIOD_MS) return;
  lastControlTraceTxMs = nowMs;

  HmiProtocol::ControlTracePayload payload{};
  const ControlTelemetrySnapshot snapshot = Orchestrator::telemetrySnapshot();
  payload.timestampMs = nowMs;
  payload.traceSequence = ++controlTraceSequence;
  payload.shotSessionId = controlTraceSessionId;
  payload.elapsedS = snapshot.elapsedS;
  const ControlDebugSnapshot debug = getControlDebugSnapshot();
  // Same sensor-filtered channel as live HMI; never label owner raw as filtered.
  const auto pressureSample = ADS1115_Interface::getPressureSnapshot();
  payload.pressureRawBar = pressureSample.rawBar;
  payload.pressureFilteredBar = pressureSample.sensorFilteredBar;
  payload.pressureRateBarPerS = pressureSample.sensorRateBarPerS;
  const auto pumpModel = PumpPSM::getControllerTelemetry();
  // Legacy wire names: pump model and puck model, consistently in all modes.
  payload.pumpFlowMlPerS = pumpModel.pumpFlowMlPerS;
  payload.puckFlowMlPerS = pumpModel.puckFlowMlPerS;
  payload.outputWeightG = shot.outGrams;
  payload.scaleWeightG = ScaleService::isOnline() ? ScaleService::getWeight() : 0.0f;
  payload.modelYieldG = ShotAggregator::modelYieldAfterGate();
  payload.waterPumpedG = shot.waterPumped;
  payload.blockTempC = shot.blockTempC;
  payload.brewTempC = shot.brewTempEstC;
  payload.targetCommand = debug.commandTarget;
  payload.controllerTargetBar = debug.controllerTargetBar;
  payload.pumpPercent = shot.pumpPower;
  payload.pumpCps = PumpPSM::getCPS();
  payload.basePercent = debug.basePct;
  payload.pTermPercent = debug.pTermPct;
  payload.iTermPercent = debug.iTermPct;
  payload.dampingPercent = debug.dampingPct;
  payload.minPercent = debug.minPct;
  payload.maxPercent = debug.maxPct;
  payload.pressureLimitBar = debug.pressureLimitBar;
  payload.flowLimitMlPerS = debug.flowLimitMlPerS;
  payload.flowQ1MlPerS = gFlow.q1MlPerS;
  payload.flowQ9MlPerS = gFlow.q9MlPerS;
  payload.flowModelReserved = 0.0f;
  payload.mainTraceDropCount = controlTraceDrops;
  payload.controlFlags = debug.flags;
  payload.scaleAgeMs = static_cast<uint16_t>(
      min<uint32_t>(ScaleService::sampleAgeMs(), UINT16_MAX));
  payload.machineState = static_cast<uint8_t>(protocolState(state));
  payload.controlMode = debug.mode;
  payload.stepIndex = debug.stepIndex;
  if (ScaleService::isOnline()) payload.scaleFlags |= 1U << 0;
  if (ScaleService::isStable()) payload.scaleFlags |= 1U << 1;
  // Schema 13 trace: online and permission to stop by yield are distinct.
  if (ScaleService::allowedForYield()) payload.scaleFlags |= 1U << 2;
  if (ScaleService::preparingShot()) payload.scaleFlags |= 1U << 3;
  payload.predictiveGateOpen = ShotAggregator::predictiveGateOpen() ? 1 : 0;

  const PumpPSM::DiagnosticTelemetry diagnostics =
      PumpPSM::getDiagnosticTelemetry();
  payload.diagSequence = diagnostics.sequence;
  payload.diagWindowMs = diagnostics.windowMs;
  payload.psmValueMin = diagnostics.valueMin;
  payload.psmValueMax = diagnostics.valueMax;
  payload.psmValueChanges = diagnostics.valueChanges;
  payload.zcRawEdges = diagnostics.rawEdges;
  payload.zcAcceptedEdges = diagnostics.acceptedEdges;
  payload.zcRejectedEdges = diagnostics.rejectedEdges;
  payload.zcRawDtMinUs = diagnostics.rawDtMinUs;
  payload.zcAcceptedDtMinUs = diagnostics.acceptedDtMinUs;
  payload.zcAcceptedDtAverageUs = diagnostics.acceptedDtAverageUs;
  payload.zcAcceptedDtMaxUs = diagnostics.acceptedDtMaxUs;
  payload.zcShortIntervals = diagnostics.acceptedShortIntervals;
  payload.zcLongIntervals = diagnostics.acceptedLongIntervals;
  payload.psmDecisions = diagnostics.decisions;
  payload.gateOnDecisions = diagnostics.gateOnDecisions;
  payload.gateTransitions = diagnostics.gateTransitions;
  payload.maxOnRun = diagnostics.maxOnRun;
  payload.maxOffRun = diagnostics.maxOffRun;
  payload.gateHighMinUs = diagnostics.gateHighMinUs;
  payload.gateHighMaxUs = diagnostics.gateHighMaxUs;
  payload.gateLowMinUs = diagnostics.gateLowMinUs;
  payload.gateLowMaxUs = diagnostics.gateLowMaxUs;
  payload.psmPatternBits = diagnostics.patternBits;
  payload.psmPatternLow = diagnostics.patternLow;
  payload.psmPatternHigh = diagnostics.patternHigh;
  payload.pumpTaskGapMinUs = diagnostics.taskGapMinUs;
  payload.pumpTaskGapMaxUs = diagnostics.taskGapMaxUs;
  payload.pumpTaskLateCount = diagnostics.taskLateCount;

  if (!sendFrameIfWritable(HmiProtocol::MessageType::ControlTrace, &payload,
                           sizeof(payload)))
    ++controlTraceDrops;
}

bool servicePendingShotEnd(uint32_t nowMs) {
  if (!shotEndPending) return true;
  if (nowMs - shotEndQueuedAtMs >= 5000U) {
    shotEndPending = false;
    return true;
  }
  if (hmiSchemaRevision != HmiProtocol::SCHEMA_REVISION ||
      (hmiFeatureFlags & HmiProtocol::HeartbeatFlags::SupportsControlTrace) == 0) {
    shotEndPending = false;
    return true;
  }
  if (!sendFrameIfWritable(HmiProtocol::MessageType::ShotEnd,
                           &pendingShotEnd, sizeof(pendingShotEnd)))
    return false;
  shotEndPending = false;
  return true;
}


void handleFrame(const HmiProtocol::FrameHeader& header,
                 const uint8_t* payload, uint32_t nowMs) {
  if (header.type == HmiProtocol::MessageType::Heartbeat &&
      header.payloadLength == sizeof(HmiProtocol::HeartbeatPayload)) {
    HmiProtocol::HeartbeatPayload heartbeat{};
    memcpy(&heartbeat, payload, sizeof(heartbeat));
    if (heartbeat.role == HmiProtocol::EndpointRole::Hmi) {
      hmiSchemaRevision = heartbeat.reserved;
      hmiFeatureFlags = heartbeat.flags;
    }
    return;
  }
  if (header.type == HmiProtocol::MessageType::ScaleSample &&
      header.payloadLength == sizeof(HmiProtocol::ScaleSamplePayload)) {
    HmiProtocol::ScaleSamplePayload sample{};
    memcpy(&sample, payload, sizeof(sample));
    if (hmiSchemaRevision == HmiProtocol::SCHEMA_REVISION)
      ScaleService::acceptHmiSample(sample, nowMs);
    return;
  }
  if (AdcProbeHandler::handle(header, payload, nowMs)) return;
  if (header.type == HmiProtocol::MessageType::HeaterRequest) {
    if (!HmiLink::compatible()) return;
    HeaterWire::Request r{};
    if (header.payloadLength == sizeof(r)) {
      memcpy(&r,payload,sizeof(r));if(r.version!=HeaterWire::VERSION)return;
    } else if (!HeaterLegacyWire::offRequest(payload,header.payloadLength,r)) return;
    Heater::request(r, Orchestrator::getState() == IDLE && !FirmwareReceiver::active() && !RecoveryPortal::updateActive());
    auto s = Heater::status(); sendFrame(HmiProtocol::MessageType::HeaterStatus, &s, sizeof(s));
    const auto legacy = HeaterLegacyWire::status(s);
    sendFrame(HmiProtocol::MessageType::HeaterStatus, &legacy, sizeof(legacy));
    const auto legacy3 = HeaterLegacyWire::statusV3(s);
    sendFrame(HmiProtocol::MessageType::HeaterStatus, &legacy3, sizeof(legacy3));
    return;
  }
  if (PressureCaptureHandler::handle(header, payload, nowMs)) return;
  if (CalibrationHandler::handle(header, payload, nowMs)) return;
  if (ProfileHandler::handle(header, payload, nowMs)) return;
  if (FirmwareReceiver::handle(header, payload, nowMs)) return;
  CommandHandler::handle(header, payload, nowMs);
}
void parseFrames(uint32_t nowMs) {
  while (rxCount >= sizeof(HmiProtocol::FrameHeader)) {
    HmiProtocol::FrameHeader header;
    memcpy(&header, rxBuffer, sizeof(header));
    if (header.magic != HmiProtocol::MAGIC) {
      ++framingErrorsRx;
      lastUartErrorMs = nowMs;
      discardBytes(1);
      continue;
    }
    if (header.version != HmiProtocol::VERSION ||
        header.payloadLength > MAX_PAYLOAD) {
      ++framingErrorsRx;
      lastUartErrorMs = nowMs;
      discardBytes(1);
      continue;
    }
    const size_t total = sizeof(header) + header.payloadLength + 2;
    if (rxCount < total) return;
    const uint16_t receivedCrc = static_cast<uint16_t>(rxBuffer[total - 2]) |
                                 static_cast<uint16_t>(rxBuffer[total - 1]) << 8;
    const uint16_t calculatedCrc =
        HmiProtocol::crc16Ccitt(rxBuffer, total - 2);
    if (receivedCrc != calculatedCrc) {
      ++crcErrorsRx;
      lastUartErrorMs = nowMs;
      // Do not trust payloadLength from a damaged header. Advance one byte so
      // the next valid MAGIC inside the buffered stream can be found.
      discardBytes(1);
      continue;
    }
    markValidFrame(nowMs);
    handleFrame(header, rxBuffer + sizeof(header), nowMs);
    discardBytes(total);
  }
}
}  // namespace HmiLinkInternal

using namespace HmiLinkInternal;
namespace HmiLink {

void begin() {
  hmiSerial.setRxBufferSize(4096);
  // A ControlTrace frame is larger than the ESP32 UART hardware FIFO. Without
  // a software TX buffer, availableForWrite() can never report enough room and
  // every non-blocking trace sample is silently dropped.
  hmiSerial.setTxBufferSize(TX_CAPACITY);
  hmiSerial.begin(UART_BAUD, SERIAL_8N1, MAIN_UART_RX_PIN, MAIN_UART_TX_PIN);
  Serial.printf("[HMI] UART1 RX=%d TX=%d @ %lu\n", MAIN_UART_RX_PIN,
                MAIN_UART_TX_PIN, static_cast<unsigned long>(UART_BAUD));
}

bool requestScaleTare(const HmiProtocol::ScaleTareRequestPayload& request) {
  return compatible() && HmiLinkInternal::sendFrameIfWritable(
      HmiProtocol::MessageType::ScaleTareRequest, &request, sizeof(request));
}

void poll() {
  const uint32_t nowMs = millis();
  if (rxCount > 0 && lastRxByteMs != 0 &&
      nowMs - lastRxByteMs >= PARTIAL_FRAME_TIMEOUT_MS) {
    rxCount = 0;
    ++staleFrameResets;
    lastUartErrorMs = nowMs;
  }
  while (hmiSerial.available() > 0) {
    const int value = hmiSerial.read();
    if (value < 0) break;
    if (rxCount == RX_CAPACITY) discardBytes(1);
    rxBuffer[rxCount++] = static_cast<uint8_t>(value);
    lastRxByteMs = nowMs;
  }
  parseFrames(nowMs);
  const uint32_t serviceNowMs = millis();
  FirmwareReceiver::poll(serviceNowMs);
  CommandHandler::poll(nowMs);
  static uint32_t lastHeaterMs = 0;
  if (nowMs - lastHeaterMs >= 500) {
    lastHeaterMs = nowMs;
    auto heater = Heater::status();
    sendFrameIfWritable(HmiProtocol::MessageType::HeaterStatus, &heater, sizeof(heater));
    const auto legacy = HeaterLegacyWire::status(heater);
    sendFrameIfWritable(HmiProtocol::MessageType::HeaterStatus, &legacy, sizeof(legacy));
    const auto legacy3 = HeaterLegacyWire::statusV3(heater);
    sendFrameIfWritable(HmiProtocol::MessageType::HeaterStatus, &legacy3, sizeof(legacy3));
    const auto sensor = SensorReader::temperatureDiagnostics();
    sendFrameIfWritable(HmiProtocol::MessageType::HeaterSensorStatus, &sensor, sizeof(sensor));
  }
  // Deliver the end marker before the first DONE telemetry frame. This lets
  // HMI append stop metadata before it closes the active trace.
  if (!servicePendingShotEnd(nowMs)) return;
  if (nowMs - lastHeartbeatTxMs >= HEARTBEAT_PERIOD_MS) {
    lastHeartbeatTxMs = nowMs;
    sendHeartbeat(nowMs);
  }
  const State state = Orchestrator::getState();
  const uint32_t telemetryPeriod =
      (state == PROFILE_RUN || state == FLUSH_RUN || state == HOTWATER_RUN)
          ? 50U
          : 250U;
  if (nowMs - lastTelemetryTxMs >= telemetryPeriod) {
    lastTelemetryTxMs = nowMs;
    sendTelemetry(nowMs);
  }
  sendControlTrace(nowMs);
}

bool connected() {
  return lastValidFrameMs != 0 && millis() - lastValidFrameMs < LINK_TIMEOUT_MS;
}

bool compatible() {
  return connected() && hmiSchemaRevision == HmiProtocol::SCHEMA_REVISION;
}

uint16_t remoteSchemaRevision() { return hmiSchemaRevision; }

uint32_t lastFrameAgeMs() {
  return lastValidFrameMs == 0 ? UINT32_MAX : millis() - lastValidFrameMs;
}

uint32_t validFrameCount() { return validFramesRx; }
uint32_t crcErrorCount() { return crcErrorsRx; }
uint32_t framingErrorCount() { return framingErrorsRx; }
uint32_t staleFrameResetCount() { return staleFrameResets; }
uint32_t reconnectCount() { return linkReconnects; }
uint32_t lastErrorAgeMs() {
  return lastUartErrorMs == 0 ? UINT32_MAX : millis() - lastUartErrorMs;
}

bool firmwareUpdateActive() { return FirmwareReceiver::active(); }
uint32_t controlTraceDropCount() { return controlTraceDrops; }

void queueShotEnd(HmiProtocol::ShotStopReason stopReason,
                  uint8_t profileExitReason, uint8_t stepIndex,
                  bool valveActive) {
  const ControlTelemetrySnapshot snapshot = Orchestrator::telemetrySnapshot();
  pendingShotEnd = HmiProtocol::ShotEndPayload{};
  pendingShotEnd.timestampMs = millis();
  pendingShotEnd.shotSessionId = controlTraceSessionId;
  pendingShotEnd.elapsedS = snapshot.elapsedS;
  pendingShotEnd.outputWeightG = shot.outGrams;
  pendingShotEnd.modelYieldG = ShotAggregator::modelYieldAfterGate();
  pendingShotEnd.stopReason = stopReason;
  pendingShotEnd.profileExitReason = profileExitReason;
  pendingShotEnd.stepIndex = stepIndex;
  if (valveActive)
    pendingShotEnd.flags |= HmiProtocol::ShotEndFlags::ShotEndValveActive;
  if (ScaleService::isOnline()) {
    pendingShotEnd.flags |= HmiProtocol::ShotEndFlags::ShotEndScaleOnline;
    pendingShotEnd.scaleWeightG = ScaleService::getWeight();
  }
  shotEndQueuedAtMs = pendingShotEnd.timestampMs;
  shotEndPending = true;
}

}  // namespace HmiLink
