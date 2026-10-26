#pragma once

#include <Arduino.h>

#include "hmi_protocol.h"

struct ProfileDetailData {
  HmiProtocol::ProfileBeginPayload profile{};
  HmiProtocol::ProfileChunkPayload
      steps[HmiProtocol::MAX_PROFILE_STEPS] = {};
  bool valid = false;
};

enum class ProfileGlobalField : uint8_t {
  Preinfusion,
  BloomTime,
  MainPressure,
  TargetWeight,
  MaxShotTime,
  FlowLimit,
};

enum class ProfileStepField : uint8_t {
  Target,
  Ramp,
  MaxTime,
  BumperValue,
};

enum class ProfileExitField : uint8_t {
  PressureAbove,
  PressureBelow,
  FlowAbove,
  FlowBelow,
  Weight,
  Water,
};

class ProfileDraft {
 public:
  void load(const ProfileDetailData& source);
  void createDefault(const char* name = "NEW PROFILE");
  void duplicate();
  void setName(const char* name);
  void markSynced(uint8_t runtimeIndex);
  void generateQuickSteps();

  const ProfileDetailData& data() const { return data_; }
  ProfileDetailData& data() { return data_; }
  bool valid() const { return data_.valid; }
  bool dirty() const { return dirty_; }
  const char* validate() const;

  void adjustGlobal(ProfileGlobalField field, float delta);
  void cycleStepMode(uint8_t index);
  void adjustStep(uint8_t index, ProfileStepField field, float delta);
  void cycleBumper(uint8_t index);
  void adjustExit(uint8_t index, ProfileExitField field, float delta);
  bool addStep();
  bool deleteStep(uint8_t index);
  bool moveStep(uint8_t index, int8_t direction);

 private:
  ProfileDetailData data_{};
  bool dirty_ = false;

  void touch();
  static float clampValue(float value, float minimum, float maximum);
};
