#pragma once

#include <stddef.h>
#include <stdint.h>

namespace HmiProtocol {

constexpr uint16_t MAGIC = 0xA55A;
constexpr uint8_t VERSION = 1;
// Wire framing stays backward-compatible so either controller can still carry
// a firmware update while application payloads evolve independently.
constexpr uint16_t SCHEMA_REVISION = 17;

enum class MessageType : uint8_t {
  Heartbeat = 0x01,
  MachineState = 0x02,
  Telemetry = 0x03,
  ScaleSample = 0x04,
  Event = 0x05,
  // A compact, fixed-size controller snapshot. Main emits this only while a
  // shot is active; the HMI owns all TF-card I/O and CSV formatting.
  ControlTrace = 0x06,
  // Reliable end marker for the active trace. It carries the source of the
  // stop so HMI history can distinguish profile completion from a physical
  // switch edge or a remote request.
  ShotEnd = 0x07,
  ScaleTareRequest = 0x08,  // Main -> HMI, expiring shot-preparation request.
  CommandRequest = 0x10,
  CommandReply = 0x11,
  ProfileBegin = 0x20,
  ProfileChunk = 0x21,
  ProfileCommit = 0x22,
  ProfileListRequest = 0x23,
  ProfileInfo = 0x24,
  ProfileDetailRequest = 0x25,
  ProfileTransferResult = 0x26,
  ProfileDeleteRequest = 0x27,
  SettingsRequest = 0x30,
  SettingsReply = 0x31,
  CalibrationRequest = 0x32,
  CalibrationReply = 0x33,
  PressureCaptureRequest = 0x34,
  PressureCaptureReply = 0x35,
  AdcProbeRequest = 0x36,
  AdcProbeReply = 0x37,
  HeaterRequest = 0x38,
  HeaterStatus = 0x39,
  HeaterSensorStatus = 0x3A,  // Optional diagnostics; schema 15 unchanged.
  FirmwareBegin = 0x40,
  FirmwareChunk = 0x41,
  FirmwareEnd = 0x42,
  FirmwareAbort = 0x43,
  FirmwareReply = 0x44,
};

enum class MachineState : uint8_t {
  Boot,
  Heating,
  Ready,
  Brew,
  Flush,
  Ending,
  Done,
  HotWater,
  Settings,
  Calibration,
  Fault,
};

enum TelemetryFlags : uint16_t {
  ScaleConnected = 1U << 0,
  ScaleStable = 1U << 1,
  // Main's view of the reverse UART path. These bits let the HMI diagnose a
  // one-way link even while Main -> HMI telemetry is still arriving.
  HmiPeerConnected = 1U << 2,
  HmiPeerCompatible = 1U << 3,
};

// Scale transport is owned by the HMI, while all decisions that use weight
// remain on Main. Value 3 remains reserved for protocol compatibility.
enum class ScaleSource : uint8_t {
  None = 0,
  BleCustom = 1,
  BleCommercial = 2,
  Reserved = 3,
  Simulator = 4,
};

enum ScaleSampleFlags : uint8_t {
  ScaleSampleConnected = 1U << 0,
  ScaleSampleStable = 1U << 1,
  ScaleSampleBatteryValid = 1U << 2,
  ScaleSampleSimulated = 1U << 3,
};

enum class Command : uint8_t {
  StartBrew = 1,
  Stop = 2,
  StartFlush = 3,
  SelectProfile = 4,
  TareScale = 5,
  AcknowledgeFault = 6,
  DismissResult = 7,
  StartPressureCalibration = 8,
  ResetStoppedPsm = 9,  // Manual diagnostic; ACK only after owner completes.
  BeginYieldEdit = 10,
  KeepYieldEdit = 11,
  ApplyYieldEdit = 12,
  CancelYieldEdit = 13,
};

enum class CommandResult : uint8_t {
  Accepted = 0,
  RejectedState = 1,
  InvalidCommand = 2,
  InvalidArgument = 3,
};

enum class EndpointRole : uint8_t {
  MainController = 1,
  Hmi = 2,
};

// Optional features are advertised independently from SCHEMA_REVISION. This
// lets a newer HMI remain compatible with an older Main while clearly knowing
// whether detailed shot traces will be emitted.
enum HeartbeatFlags : uint8_t {
  SupportsControlTrace = 1U << 0,
  SupportsPressureCapture = 1U << 1,
  SupportsAdcProbe = 1U << 2,
  SupportsHeater = 1U << 3,
};

enum class ShotStopReason : uint8_t {
  Unknown = 0,
  Profile = 1,
  BrewSwitch = 2,
  Hmi = 3,
  Manual = 4,
  AutoYield = 5,
  HotWaterSwitch = 6,
};

enum ShotEndFlags : uint8_t {
  ShotEndValveActive = 1U << 0,
  ShotEndScaleOnline = 1U << 1,
};

#pragma pack(push, 1)
struct FrameHeader {
  uint16_t magic;
  uint8_t version;
  MessageType type;
  uint16_t payloadLength;
  uint16_t sequence;
};

struct HeartbeatPayload {
  uint32_t uptimeMs;
  EndpointRole role;
  uint8_t flags;
  uint16_t reserved;
};

struct TelemetryPayload {
  uint32_t timestampMs;
  float pressureBar;
  float flowGps;
  float weightG;
  float blockTempC;
  float brewTempC;
  float elapsedS;
  float targetValue;
  float pumpPercent;
  uint8_t machineState;
  uint8_t controlMode;
  uint8_t stepIndex;
  uint8_t stepCount;
  uint16_t flags;
  ScaleSource scaleSource;
  uint8_t scaleBatteryPercent;
  char profileName[24];
  char phaseName[16];
  float quickYieldG;
  uint32_t yieldEditToken;
  uint8_t quickYieldFlags; // bit0 editable, bit1 editing, bit2 next-shot override
  uint8_t selectedProfileIndex;
};

// One analysis sample at 25 Hz.  Keep this binary and fixed-size on the wire:
// logging must never make Main allocate memory, format text, or wait for TF.
// Percent fields use the 0.0..100.0 range for convenient CSV export.
struct ControlTracePayload {
  uint32_t timestampMs;
  uint32_t traceSequence;
  uint32_t shotSessionId;
  float elapsedS;
  float pressureRawBar;
  float pressureFilteredBar;
  float pressureRateBarPerS;
  float pumpFlowMlPerS;
  float puckFlowMlPerS;
  float outputWeightG;
  float scaleWeightG;
  float modelYieldG;
  float waterPumpedG;
  float blockTempC;
  float brewTempC;
  float targetCommand;
  float controllerTargetBar;
  float pumpPercent;
  float pumpCps;
  float basePercent;
  float pTermPercent;
  float iTermPercent;
  float dampingPercent;
  float minPercent;
  float maxPercent;
  float pressureLimitBar;
  float flowLimitMlPerS;
  float flowQ1MlPerS;
  float flowQ9MlPerS;
  float flowModelReserved;
  uint32_t mainTraceDropCount;
  uint16_t controlFlags;
  uint16_t scaleAgeMs;
  uint8_t machineState;
  uint8_t controlMode;
  uint8_t stepIndex;
  uint8_t scaleFlags;
  uint8_t predictiveGateOpen;
  uint8_t reserved[3];

  // One-second PSM diagnostic window, repeated in the 25 Hz trace until the
  // next window is complete. Consumers should de-duplicate by diagSequence.
  uint32_t diagSequence;
  uint32_t diagWindowMs;
  uint32_t psmValueMin;
  uint32_t psmValueMax;
  uint32_t psmValueChanges;
  uint32_t zcRawEdges;
  uint32_t zcAcceptedEdges;
  uint32_t zcRejectedEdges;
  uint32_t zcRawDtMinUs;
  uint32_t zcAcceptedDtMinUs;
  uint32_t zcAcceptedDtAverageUs;
  uint32_t zcAcceptedDtMaxUs;
  uint32_t zcShortIntervals;
  uint32_t zcLongIntervals;
  uint32_t psmDecisions;
  uint32_t gateOnDecisions;
  uint32_t gateTransitions;
  uint32_t maxOnRun;
  uint32_t maxOffRun;
  uint32_t gateHighMinUs;
  uint32_t gateHighMaxUs;
  uint32_t gateLowMinUs;
  uint32_t gateLowMaxUs;
  uint32_t psmPatternBits;
  uint32_t psmPatternLow;
  uint32_t psmPatternHigh;
  uint32_t pumpTaskGapMinUs;
  uint32_t pumpTaskGapMaxUs;
  uint32_t pumpTaskLateCount;
};

struct ShotEndPayload {
  uint32_t timestampMs;
  uint32_t shotSessionId;
  float elapsedS;
  float outputWeightG;
  float scaleWeightG;
  float modelYieldG;
  ShotStopReason stopReason;
  // Numeric value of Main's ProfileStepExitReason. Kept as a byte here so
  // the wire protocol does not depend on Main-only profile headers.
  uint8_t profileExitReason;
  uint8_t stepIndex;
  uint8_t flags;
};

// HMI sends the most recent scale observation, including its age. Repeating a
// stale observation cannot keep a scale online because Main reconstructs the
// original sample time from sampleAgeMs.
enum class ScaleTareState : uint8_t {
  None, Preparing, Ready, Failed, Cancelled
};

struct ScaleTareRequestPayload {
  uint32_t requestId;
  uint32_t hmiBootId;
  uint32_t connectionGeneration;
  uint32_t expiresAtHmiMs;
  uint8_t cancel;
};

struct ScaleSamplePayload {
  uint32_t sourceTimestampMs;
  uint32_t sampleSequence;
  float weightG;
  uint16_t sampleAgeMs;
  ScaleSource source;
  uint8_t flags;
  uint8_t batteryPercent;
  uint8_t reserved[3];
  uint32_t hmiBootId;
  uint32_t connectionGeneration;
  uint32_t tareGeneration;
  uint32_t transmittedAtMs;
  uint32_t tareRequestId;
  ScaleTareState tareState;
};

// The frame sequence is the request ID. Commands are events, never persistent
// output states; the Main controller remains the only owner of pump/relay state.
struct CommandRequestPayload {
  Command command;
  uint8_t reserved[3];
  uint32_t argument;
};

struct CommandReplyPayload {
  uint16_t requestSequence;
  Command command;
  CommandResult result;
  uint8_t machineState;
  uint8_t reserved;
};

constexpr uint8_t MAX_PROFILE_ITEMS = 16;
constexpr uint8_t MAX_PROFILE_STEPS = 8;

enum class ProfileTransferAction : uint8_t {
  Download = 1,
  Upload = 2,
  Delete = 3,
};

enum class ProfileTransferStatus : uint8_t {
  Pending = 0,
  Accepted = 1,
  RejectedState = 2,
  InvalidIndex = 3,
  InvalidData = 4,
  Incomplete = 5,
  ChecksumMismatch = 6,
  StorageError = 7,
};

struct ProfileInfoPayload {
  uint8_t index;
  uint8_t count;
  uint8_t currentIndex;
  uint8_t stepCount;
  uint8_t type;
  uint8_t flags;
  float targetWeightG;
  float maxShotTimeS;
  char name[24];
};

struct ProfileDetailRequestPayload {
  uint8_t profileIndex;
  uint8_t reserved[3];
};

// A profile transfer is atomic. ProfileBegin describes the draft, one
// ProfileChunk carries each step, and ProfileCommit accepts or rejects the
// complete checksum. A profileIndex of 0xFF creates a new user profile.
struct ProfileTargetPayload {
  uint8_t type; // volume, pressure, flow, pumped
  uint8_t op; // lte, gte
  float value;
};

struct ProfileBeginPayload {
  uint16_t transferId;
  ProfileTransferAction action;
  uint8_t profileIndex;
  uint8_t stepCount;
  uint8_t type;
  uint16_t checksum;
  float targetWeightG;
  float maxShotTimeS;
  float basicPreinfusionBar;
  float basicMainBar;
  float basicBloomS;
  float basicFlowLimitGps;
  char name[24];
  char note[192];
  char id[40];
  float temperature;
  uint8_t formatVersion;
  uint8_t profileFlags; // favorite=1, selected=2, utility=4
};

struct ProfileChunkPayload {
  uint16_t transferId;
  uint8_t stepIndex;
  uint8_t mode;
  uint8_t bumperType;
  uint8_t reserved;
  char name[16];
  float target;
  float rampRate;
  float timeoutMs;
  float bumperValue;
  float pressureAbove;
  float pressureBelow;
  float flowAbove;
  float flowBelow;
  float weight;
  float waterPumped;
  uint8_t phaseType;
  uint8_t valve;
  uint8_t transitionType;
  uint8_t transitionTarget;
  uint8_t adaptive;
  uint8_t targetCount;
  float temperature;
  float transitionDuration;
  float yieldFallbackS;
  ProfileTargetPayload targets[8];
};

struct ProfileCommitPayload {
  uint16_t transferId;
  ProfileTransferAction action;
  ProfileTransferStatus status;
  uint8_t profileIndex;
  uint8_t receivedSteps;
  uint16_t checksum;
};

struct ProfileDeleteRequestPayload {
  uint16_t transferId;
  uint8_t profileIndex;
  uint8_t reserved;
};

enum class CalibrationOperation : uint8_t {
  ReadAll = 1,
  ZeroPressure = 2,
  SetPressureAt = 3,
  SetFlow = 4,
  SetThermalOffset = 5,
};

enum class CalibrationStatus : uint8_t {
  Accepted = 0,
  RejectedState = 1,
  InvalidValue = 2,
  SensorError = 3,
  StorageError = 4,
  Conflict = 5,
};

struct CalibrationRequestPayload {
  uint16_t requestId;
  CalibrationOperation operation;
  uint8_t reserved;
  float value1;
  float value2;
  float value3;
};

struct CalibrationReplyPayload {
  uint16_t requestId;
  CalibrationOperation operation;
  CalibrationStatus status;
  uint8_t machineState;
  uint8_t reserved[3];
  float pressureOffset;
  float pressureScale;
  float pressureAdcVolts;
  float pressureBar;
  float flowQ1MlPerS;
  float flowQ9MlPerS;
  float flowModelReserved;
  float zeroCrossHz;
  float pumpCps;
  float pumpFlowPerClickMl;
  float freeFlowPerClickMl;
  float actualPumpFlowMlPerS;
  float thermalOffsetC;
};

constexpr uint16_t MAX_FIRMWARE_CHUNK = 480;

enum class FirmwareTarget : uint8_t {
  MainController = 1,
};

enum class FirmwareStage : uint8_t {
  Begin = 1,
  Chunk = 2,
  End = 3,
  Abort = 4,
};

enum class FirmwareStatus : uint8_t {
  Accepted = 0,
  RejectedState = 1,
  Busy = 2,
  InvalidTarget = 3,
  InvalidSize = 4,
  InvalidOffset = 5,
  ChecksumMismatch = 6,
  WriteError = 7,
  FinalizeError = 8,
  InvalidTransfer = 9,
  Aborted = 10,
};

struct FirmwareBeginPayload {
  uint32_t transferId;
  uint32_t imageSize;
  FirmwareTarget target;
  uint8_t reserved[3];
  uint8_t sha256[32];
};

// Only the first dataLength bytes of data are sent on the wire.
struct FirmwareChunkPayload {
  uint32_t transferId;
  uint32_t offset;
  uint16_t dataLength;
  uint16_t dataCrc;
  uint8_t data[MAX_FIRMWARE_CHUNK];
};

struct FirmwareEndPayload {
  uint32_t transferId;
  uint32_t imageSize;
  uint8_t sha256[32];
};

struct FirmwareAbortPayload {
  uint32_t transferId;
};

struct FirmwareReplyPayload {
  uint32_t transferId;
  FirmwareStage stage;
  FirmwareStatus status;
  uint16_t reserved;
  uint32_t nextOffset;
  uint32_t imageSize;
};
#pragma pack(pop)

inline const char* scaleSourceName(ScaleSource source) {
  switch (source) {
    case ScaleSource::BleCustom: return "BLE CUSTOM";
    case ScaleSource::BleCommercial: return "BLE COMMERCIAL";
    case ScaleSource::Reserved: return "RESERVED";
    case ScaleSource::Simulator: return "SIMULATOR";
    default: return "OFFLINE";
  }
}

inline uint16_t crc16Ccitt(const uint8_t* data, size_t length,
                          uint16_t initial = 0xFFFF) {
  uint16_t crc = initial;
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                           : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

inline uint16_t profileChecksum(const ProfileBeginPayload& source,
                                const ProfileChunkPayload* steps,
                                uint8_t stepCount) {
  ProfileBeginPayload begin = source;
  begin.checksum = 0;
  uint16_t crc = crc16Ccitt(reinterpret_cast<const uint8_t*>(&begin),
                            sizeof(begin));
  for (uint8_t i = 0; i < stepCount; ++i) {
    crc = crc16Ccitt(reinterpret_cast<const uint8_t*>(&steps[i]),
                     sizeof(ProfileChunkPayload), crc);
  }
  return crc;
}

}  // namespace HmiProtocol
