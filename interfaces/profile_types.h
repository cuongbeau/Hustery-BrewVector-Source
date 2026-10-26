// File: interfaces/profile_types.h
// Tách từ calibration_store.h + profiles.h — chứa tất cả profile/step types
// Refactor: 2026-04-20

#ifndef PROFILE_TYPES_H
#define PROFILE_TYPES_H

#include <Arduino.h>

// ====== Giới hạn hợp lý để dùng mảng tĩnh ======
#define MAX_PROFILES_JSON   12      // không tính OFF/MANUAL
#define MAX_STAGES_JSON     8
#define MAX_PROFILES_RUNTIME   16 
#define MAX_POINTS_PER_PROFILE 10

// ====== Enums ======
enum BumperType : uint8_t {
  BUMPER_NONE = 0,
  BUMPER_PRESS = 1,
  BUMPER_FLOW = 2
};

enum ExitTriggerType : uint8_t {
  EXIT_TRIGGER_NONE = 0,
  EXIT_TRIGGER_WEIGHT = 1,
  EXIT_TRIGGER_PRESS_ABOVE = 2,
  EXIT_TRIGGER_PRESS_BELOW = 3,
  EXIT_TRIGGER_FLOW_ABOVE = 4,
  EXIT_TRIGGER_FLOW_BELOW = 5,
  EXIT_TRIGGER_WATER_PUMPED = 6
};

enum StepMode : uint8_t {
  MODE_PRESSURE = 0,
  MODE_FLOW     = 1,
  MODE_DUTY     = 2
};

struct StepExitConditions {
  float pressureAbove = -1.0f;
  float pressureBelow = -1.0f;
  float flowAbove = -1.0f;
  float flowBelow = -1.0f;
  float weight = -1.0f;
  float waterPumped = -1.0f;
};

// ====== Step definition (Advanced) ======
struct AdvancedStep {
  char  name[16]       = "";      // Tên bước (ví dụ: "Soak")
  StepMode mode        = MODE_PRESSURE;
  float target         = 0.0f;    // Mức áp (bar), Flow (g/s) hoặc Duty (%)
  float rampRate       = 0.0f;    // Tốc độ chuyển đổi (/s)
  float timeoutMs      = 1000.0f; // Kéo dài tối đa bao lâu (ms)
  
  // Rào chắn bảo vệ (Bumper) - Chỉ cho phép 1 loại
  BumperType bumperType = BUMPER_NONE;
  float      bumperVal  = -1.0f;   

  // Điều kiện dừng (Exit) - Chỉ cho phép 1 loại
  ExitTriggerType exitType = EXIT_TRIGGER_NONE;
  float           exitVal  = -1.0f;
  StepExitConditions exits;
};

// ====== Profile JSON (lưu trữ từ SPIFFS) ======
struct ProfileJson {
  String canonicalJson; // Upstream schema; legacy fields below are UI projection only.
  String  name;
  uint8_t n = 0;
  float   totalTargetWeight = 0.0f; // Tham số Shot tổng cục bộ
  float   maxShotTimeS      = 0.0f; // Thời gian an toàn toàn Shot
  uint8_t type              = 1;    // 0: Advanced, 1: Basic
  float   basicPreP         = 3.0f;
  float   basicMainP        = 9.0f;
  float   basicBloomT       = 5.0f;
  float   basicFlowLimit    = 0.0f; // 0.0 = disabled
  String  note;
  AdvancedStep steps[MAX_STAGES_JSON];
};

// ====== Runtime Profile (expanded for execution) ======
struct Profile {
  char         name[24];
  uint8_t      type;      // 0: Advanced, 1: Basic
  uint8_t      stepCount;
  float        totalTargetWeight;
  float        maxShotTimeS;
  char         note[300];
  AdvancedStep steps[MAX_POINTS_PER_PROFILE];
  bool         isManual;
  bool         isOff;
};

// ====== Profile Executor State ======
struct ProfileExecutorState {
  bool     running;
  int      profileIndex;
  uint8_t  currentStep;
  
  uint32_t profileStartMs;
  uint32_t stepStartMs;
  float    startValue;      // Pressure or Flow at start of step
  float    highestObserved; // For PressureDrop triggers
  float    stepStartPressure;
  float    stepStartFlow;
  float    stepStartWeight;
  float    stepStartWaterPumped;
  
  uint32_t triggerStartMs;  // Hysteresis timer
};

// ====== Target Snapshot ======
struct TargetSnap {
  float val;
  StepMode mode;
};

// ====== Legacy phase types (compatibility) ======
enum class PHASE_TYPE {
  FLOW,
  PRESSURE
};

struct PhaseStopConditions {
  uint32_t timeMs = 0;
  float pressureAbove = -1.0f;
  float pressureBelow = -1.0f;
  float flowAbove = -1.0f;
  float flowBelow = -1.0f;
  float weight = -1.0f;
  float waterPumped = -1.0f;
};

struct Phase {
  PHASE_TYPE type = PHASE_TYPE::PRESSURE;
  float      targetStart = 0.0f;
  float      targetEnd = 0.0f;
  uint32_t   rampTimeMs = 0;
  float      restriction = 0.0f;
  PhaseStopConditions stop;
};

// ====== Extern declarations ======
extern ProfileJson gProfiles[MAX_PROFILES_JSON];
extern uint8_t     gProfilesCount;
extern Profile     profiles[MAX_PROFILES_RUNTIME];
extern int         profilesCount;
extern bool        isPhysicalScaleConnected;

#endif // PROFILE_TYPES_H
