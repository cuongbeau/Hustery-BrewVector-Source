#include "model/profile_draft.h"

#include <math.h>
#include <string.h>

namespace {
void resetExit(HmiProtocol::ProfileChunkPayload& step) {
  step.pressureAbove = -1.0f;
  step.pressureBelow = -1.0f;
  step.flowAbove = -1.0f;
  step.flowBelow = -1.0f;
  step.weight = -1.0f;
  step.waterPumped = -1.0f;
}

void initializeStep(HmiProtocol::ProfileChunkPayload& step, uint8_t index,
                    const char* name, float target, float ramp,
                    float timeoutMs) {
  step = HmiProtocol::ProfileChunkPayload{};
  step.stepIndex = index;
  step.mode = 0;
  step.bumperType = 0;
  step.bumperValue = -1.0f;
  snprintf(step.name, sizeof(step.name), "%s", name);
  step.target = target;
  step.rampRate = ramp;
  step.timeoutMs = timeoutMs;
  resetExit(step);
}

float* exitValue(HmiProtocol::ProfileChunkPayload& step,
                 ProfileExitField field) {
  switch (field) {
    case ProfileExitField::PressureAbove: return &step.pressureAbove;
    case ProfileExitField::PressureBelow: return &step.pressureBelow;
    case ProfileExitField::FlowAbove: return &step.flowAbove;
    case ProfileExitField::FlowBelow: return &step.flowBelow;
    case ProfileExitField::Weight: return &step.weight;
    case ProfileExitField::Water: return &step.waterPumped;
  }
  return nullptr;
}
}  // namespace

float ProfileDraft::clampValue(float value, float minimum, float maximum) {
  return value < minimum ? minimum : value > maximum ? maximum : value;
}

void ProfileDraft::touch() {
  dirty_ = true;
  data_.valid = true;
  data_.profile.checksum = 0;
}

void ProfileDraft::load(const ProfileDetailData& source) {
  data_ = source;
  dirty_ = false;
}

void ProfileDraft::createDefault(const char* name) {
  data_ = ProfileDetailData{};
  data_.valid = true;
  data_.profile.action = HmiProtocol::ProfileTransferAction::Upload;
  data_.profile.profileIndex = 0xFF;
  data_.profile.type = 1;
  data_.profile.targetWeightG = 36.0f;
  data_.profile.maxShotTimeS = 45.0f;
  data_.profile.basicPreinfusionBar = 4.0f;
  data_.profile.basicMainBar = 9.0f;
  data_.profile.basicBloomS = 5.0f;
  data_.profile.basicFlowLimitGps = 0.0f;
  snprintf(data_.profile.name, sizeof(data_.profile.name), "%s", name);
  generateQuickSteps();
  dirty_ = true;
}

void ProfileDraft::duplicate() {
  if (!data_.valid) return;
  data_.profile.profileIndex = 0xFF;
  char copied[sizeof(data_.profile.name)] = {};
  snprintf(copied, sizeof(copied), "%.18s COPY", data_.profile.name);
  snprintf(data_.profile.name, sizeof(data_.profile.name), "%s", copied);
  touch();
}

void ProfileDraft::setName(const char* name) {
  if (!data_.valid || !name || name[0] == '\0') return;
  snprintf(data_.profile.name, sizeof(data_.profile.name), "%s", name);
  touch();
}

void ProfileDraft::markSynced(uint8_t runtimeIndex) {
  data_.profile.profileIndex = runtimeIndex;
  dirty_ = false;
}

void ProfileDraft::generateQuickSteps() {
  if (!data_.valid) return;
  auto& profile = data_.profile;
  profile.type = 1;
  profile.stepCount = 5;
  const float flowLimit = profile.basicFlowLimitGps;
  const float remaining = max(1.0f, profile.maxShotTimeS -
                                        profile.basicBloomS - 8.5f);
  initializeStep(data_.steps[0], 0, "PRE-INFUSE",
                 profile.basicPreinfusionBar, 999.0f, 1500.0f);
  initializeStep(data_.steps[1], 1, "BLOOM",
                 profile.basicPreinfusionBar * 0.6f, 999.0f,
                 profile.basicBloomS * 1000.0f);
  initializeStep(data_.steps[2], 2, "RAMP", profile.basicMainBar, 2.5f,
                 4000.0f);
  data_.steps[2].pressureAbove = profile.basicMainBar;
  initializeStep(data_.steps[3], 3, "EXTRACT", profile.basicMainBar, 0.0f,
                 3000.0f);
  initializeStep(data_.steps[4], 4, "FINISH", profile.basicMainBar, 0.0f,
                 remaining * 1000.0f);
  data_.steps[4].weight = profile.targetWeightG;
  if (flowLimit > 0.0f) {
    for (uint8_t i = 2; i < profile.stepCount; ++i) {
      data_.steps[i].bumperType = 2;
      data_.steps[i].bumperValue = flowLimit;
    }
  }
  touch();
}

void ProfileDraft::adjustGlobal(ProfileGlobalField field, float delta) {
  if (!data_.valid) return;
  auto& p = data_.profile;
  switch (field) {
    case ProfileGlobalField::Preinfusion:
      p.basicPreinfusionBar = clampValue(p.basicPreinfusionBar + delta, 0.5f, 8.0f);
      break;
    case ProfileGlobalField::BloomTime:
      p.basicBloomS = clampValue(p.basicBloomS + delta, 0.0f, 30.0f);
      break;
    case ProfileGlobalField::MainPressure:
      p.basicMainBar = clampValue(p.basicMainBar + delta, 1.0f, 12.0f);
      break;
    case ProfileGlobalField::TargetWeight:
      p.targetWeightG = clampValue(p.targetWeightG + delta, 1.0f, 200.0f);
      break;
    case ProfileGlobalField::MaxShotTime:
      p.maxShotTimeS = clampValue(p.maxShotTimeS + delta, 10.0f, 180.0f);
      break;
    case ProfileGlobalField::FlowLimit:
      p.basicFlowLimitGps = clampValue(p.basicFlowLimitGps + delta, 0.0f, 15.0f);
      if (p.basicFlowLimitGps < 0.1f) p.basicFlowLimitGps = 0.0f;
      break;
  }
  generateQuickSteps();
}

void ProfileDraft::cycleStepMode(uint8_t index) {
  if (!data_.valid || index >= data_.profile.stepCount) return;
  auto& step = data_.steps[index];
  step.mode = (step.mode + 1U) % 3U;
  const float maximum = step.mode == 0 ? 12.0f : step.mode == 1 ? 15.0f : 100.0f;
  step.target = clampValue(step.target, 0.0f, maximum);
  data_.profile.type = 0;
  touch();
}

void ProfileDraft::adjustStep(uint8_t index, ProfileStepField field,
                              float delta) {
  if (!data_.valid || index >= data_.profile.stepCount) return;
  auto& step = data_.steps[index];
  switch (field) {
    case ProfileStepField::Target: {
      const float maximum = step.mode == 0 ? 12.0f : step.mode == 1 ? 15.0f : 100.0f;
      step.target = clampValue(step.target + delta, 0.0f, maximum);
      break;
    }
    case ProfileStepField::Ramp:
      step.rampRate = clampValue(step.rampRate + delta, 0.0f, 999.0f);
      break;
    case ProfileStepField::MaxTime:
      step.timeoutMs = clampValue(step.timeoutMs + delta * 1000.0f,
                                  100.0f, 180000.0f);
      break;
    case ProfileStepField::BumperValue:
      step.bumperValue = clampValue(step.bumperValue + delta, 0.1f, 15.0f);
      break;
  }
  data_.profile.type = 0;
  touch();
}

void ProfileDraft::cycleBumper(uint8_t index) {
  if (!data_.valid || index >= data_.profile.stepCount) return;
  auto& step = data_.steps[index];
  step.bumperType = (step.bumperType + 1U) % 3U;
  step.bumperValue = step.bumperType == 0 ? -1.0f :
      (step.bumperValue > 0.0f ? step.bumperValue :
       step.bumperType == 1 ? 10.0f : 4.0f);
  data_.profile.type = 0;
  touch();
}

void ProfileDraft::adjustExit(uint8_t index, ProfileExitField field,
                              float delta) {
  if (!data_.valid || index >= data_.profile.stepCount) return;
  float* value = exitValue(data_.steps[index], field);
  if (!value) return;
  if (*value <= 0.0f && delta > 0.0f) *value = delta;
  else *value += delta;
  if (*value <= 0.0f) *value = -1.0f;
  else *value = clampValue(*value, 0.1f, 500.0f);
  data_.profile.type = 0;
  touch();
}

bool ProfileDraft::addStep() {
  if (!data_.valid || data_.profile.stepCount >= HmiProtocol::MAX_PROFILE_STEPS)
    return false;
  const uint8_t index = data_.profile.stepCount++;
  initializeStep(data_.steps[index], index, "NEW STEP", 9.0f, 0.0f, 5000.0f);
  data_.profile.type = 0;
  touch();
  return true;
}

bool ProfileDraft::deleteStep(uint8_t index) {
  if (!data_.valid || data_.profile.stepCount <= 1 ||
      index >= data_.profile.stepCount)
    return false;
  for (uint8_t i = index; i + 1 < data_.profile.stepCount; ++i) {
    data_.steps[i] = data_.steps[i + 1];
    data_.steps[i].stepIndex = i;
  }
  --data_.profile.stepCount;
  data_.profile.type = 0;
  touch();
  return true;
}

bool ProfileDraft::moveStep(uint8_t index, int8_t direction) {
  const int destination = static_cast<int>(index) + direction;
  if (!data_.valid || index >= data_.profile.stepCount || destination < 0 ||
      destination >= data_.profile.stepCount)
    return false;
  auto temporary = data_.steps[index];
  data_.steps[index] = data_.steps[destination];
  data_.steps[destination] = temporary;
  data_.steps[index].stepIndex = index;
  data_.steps[destination].stepIndex = destination;
  data_.profile.type = 0;
  touch();
  return true;
}

const char* ProfileDraft::validate() const {
  if (!data_.valid) return "PROFILE NOT LOADED";
  const auto& profile = data_.profile;
  if (profile.name[0] == '\0') return "NAME IS REQUIRED";
  if (profile.stepCount == 0 ||
      profile.stepCount > HmiProtocol::MAX_PROFILE_STEPS)
    return "INVALID STEP COUNT";
  if (!isfinite(profile.maxShotTimeS) || profile.maxShotTimeS < 1.0f ||
      profile.maxShotTimeS > 180.0f)
    return "INVALID SHOT TIME";
  for (uint8_t i = 0; i < profile.stepCount; ++i) {
    const auto& step = data_.steps[i];
    const float maximum = step.mode == 0 ? 12.0f : step.mode == 1 ? 15.0f : 100.0f;
    if (step.mode > 2 || !isfinite(step.target) || step.target < 0.0f ||
        step.target > maximum)
      return "STEP TARGET OUT OF RANGE";
    if (!isfinite(step.timeoutMs) || step.timeoutMs < 100.0f ||
        step.timeoutMs > 180000.0f)
      return "STEP TIME OUT OF RANGE";
  }
  return nullptr;
}
