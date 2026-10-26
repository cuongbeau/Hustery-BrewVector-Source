#include "src/communication/hmi_link_internal.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "src/app/calibration_store.h"
#include "src/app/orchestrator.h"
#include "src/control/profile_engine.h"
#include "src/control/profile_store.h"

namespace HmiLinkInternal {
namespace ProfileHandler {
namespace {
uint16_t profileTransferId = 0;
HmiProtocol::ProfileBeginPayload incomingProfile{};
HmiProtocol::ProfileChunkPayload
    incomingSteps[HmiProtocol::MAX_PROFILE_STEPS] = {};
uint8_t incomingStepMask = 0;
bool profileUploadActive = false;

void sendProfileList();
void sendProfileTransferResult(
    uint16_t transferId, HmiProtocol::ProfileTransferAction action,
    HmiProtocol::ProfileTransferStatus status, uint8_t profileIndex,
    uint8_t receivedSteps, uint16_t checksum) {
  HmiProtocol::ProfileCommitPayload result{transferId, action, status,
                                           profileIndex, receivedSteps,
                                           checksum};
  sendFrame(HmiProtocol::MessageType::ProfileTransferResult, &result,
            sizeof(result));
}

bool validFiniteRange(float value, float minimum, float maximum) {
  return isfinite(value) && value >= minimum && value <= maximum;
}

bool validateTransferredProfile(const HmiProtocol::ProfileBeginPayload& begin,
                                const HmiProtocol::ProfileChunkPayload* steps) {
  if(begin.formatVersion!=1 || begin.type>1 || begin.stepCount==0 || begin.stepCount>8 ||
     !memchr(begin.id,0,sizeof(begin.id)) || !memchr(begin.name,0,sizeof(begin.name)) || !memchr(begin.note,0,sizeof(begin.note)))return false;
  for(unsigned i=0;i<begin.stepCount;++i){const auto& s=steps[i];
    if(s.stepIndex!=i || s.mode>2 || s.transitionType>4 || s.transitionTarget>2 || s.phaseType>1 || s.targetCount>8 || !memchr(s.name,0,sizeof(s.name)))return false;
    for(unsigned k=0;k<s.targetCount;++k)if(s.targets[k].type>3 || s.targets[k].op>1)return false;
  }
  JsonDocument doc;ProfileCodec::write(doc.to<JsonObject>(),begin,steps);
  HmiProtocol::ProfileBeginPayload checked{};HmiProtocol::ProfileChunkPayload checkedSteps[8]{};String error;
  return ProfileCodec::parse(doc.as<JsonObjectConst>(),checked,checkedSteps,error);
}

void sendProfileDetail(uint8_t runtimeIndex) {
  if (runtimeIndex < 2 || runtimeIndex >= static_cast<uint8_t>(profilesCount) ||
      runtimeIndex - 2 >= gProfilesCount) {
    sendProfileTransferResult(0, HmiProtocol::ProfileTransferAction::Download,
                              HmiProtocol::ProfileTransferStatus::InvalidIndex,
                              runtimeIndex, 0, 0);
    return;
  }
  HmiProtocol::ProfileBeginPayload begin{};
  HmiProtocol::ProfileChunkPayload steps[8]{};String error;
  if(!ProfileStore::decode(gProfiles[runtimeIndex-2].canonicalJson,begin,steps,error))return;
  if(++profileTransferId==0)++profileTransferId;
  begin.transferId=profileTransferId;begin.action=HmiProtocol::ProfileTransferAction::Download;begin.profileIndex=runtimeIndex;
  for(unsigned i=0;i<begin.stepCount;++i)steps[i].transferId=begin.transferId;
  begin.checksum = HmiProtocol::profileChecksum(begin, steps, begin.stepCount);
  sendFrame(HmiProtocol::MessageType::ProfileBegin, &begin, sizeof(begin));
  for (uint8_t i = 0; i < begin.stepCount; ++i)
    sendFrame(HmiProtocol::MessageType::ProfileChunk, &steps[i], sizeof(steps[i]));
  sendProfileTransferResult(begin.transferId,
                            HmiProtocol::ProfileTransferAction::Download,
                            HmiProtocol::ProfileTransferStatus::Accepted,
                            runtimeIndex, begin.stepCount, begin.checksum);
}

void beginProfileUpload(const HmiProtocol::ProfileBeginPayload& begin) {
  const State state = Orchestrator::getState();
  if (state != IDLE && state != SHOT_END) {
    sendProfileTransferResult(begin.transferId, begin.action,
                              HmiProtocol::ProfileTransferStatus::RejectedState,
                              begin.profileIndex, 0, begin.checksum);
    return;
  }
  const bool creating = begin.profileIndex == 0xFF;
  if ((!creating && (begin.profileIndex < 2 ||
                     begin.profileIndex >= static_cast<uint8_t>(profilesCount))) ||
      (creating && gProfilesCount >= MAX_PROFILES_JSON) ||
      begin.stepCount == 0 ||
      begin.stepCount > HmiProtocol::MAX_PROFILE_STEPS) {
    sendProfileTransferResult(begin.transferId, begin.action,
                              HmiProtocol::ProfileTransferStatus::InvalidIndex,
                              begin.profileIndex, 0, begin.checksum);
    return;
  }
  incomingProfile = begin;
  incomingProfile.name[sizeof(incomingProfile.name) - 1] = '\0';
  incomingProfile.note[sizeof(incomingProfile.note) - 1] = '\0';
  memset(incomingSteps, 0, sizeof(incomingSteps));
  incomingStepMask = 0;
  profileUploadActive = true;
}

void receiveProfileStep(const HmiProtocol::ProfileChunkPayload& step) {
  if (!profileUploadActive || step.transferId != incomingProfile.transferId ||
      step.stepIndex >= incomingProfile.stepCount) return;
  incomingSteps[step.stepIndex] = step;
  incomingSteps[step.stepIndex].name[
      sizeof(incomingSteps[step.stepIndex].name) - 1] = '\0';
  incomingStepMask |= static_cast<uint8_t>(1U << step.stepIndex);
}

void commitProfileUpload(const HmiProtocol::ProfileCommitPayload& commit) {
  if (!profileUploadActive || commit.transferId != incomingProfile.transferId) {
    sendProfileTransferResult(commit.transferId, commit.action,
                              HmiProtocol::ProfileTransferStatus::Incomplete,
                              commit.profileIndex, 0, commit.checksum);
    return;
  }
  profileUploadActive = false;
  if(Orchestrator::getState()!=IDLE && Orchestrator::getState()!=SHOT_END){
    sendProfileTransferResult(commit.transferId,commit.action,HmiProtocol::ProfileTransferStatus::RejectedState,commit.profileIndex,0,0);return;
  }
  const uint8_t expectedMask =
      static_cast<uint8_t>((1U << incomingProfile.stepCount) - 1U);
  if ((incomingStepMask & expectedMask) != expectedMask) {
    sendProfileTransferResult(commit.transferId, commit.action,
                              HmiProtocol::ProfileTransferStatus::Incomplete,
                              incomingProfile.profileIndex, 0, commit.checksum);
    return;
  }
  const uint16_t checksum = HmiProtocol::profileChecksum(
      incomingProfile, incomingSteps, incomingProfile.stepCount);
  if (checksum != incomingProfile.checksum || checksum != commit.checksum) {
    sendProfileTransferResult(commit.transferId, commit.action,
                              HmiProtocol::ProfileTransferStatus::ChecksumMismatch,
                              incomingProfile.profileIndex,
                              incomingProfile.stepCount, checksum);
    return;
  }
  if (!validateTransferredProfile(incomingProfile, incomingSteps)) {
    sendProfileTransferResult(commit.transferId, commit.action,
                              HmiProtocol::ProfileTransferStatus::InvalidData,
                              incomingProfile.profileIndex,
                              incomingProfile.stepCount, checksum);
    return;
  }

  const bool creating = incomingProfile.profileIndex == 0xFF;
  const uint8_t jsonIndex = creating ? gProfilesCount
                                     : incomingProfile.profileIndex - 2;
  if(jsonIndex>=MAX_PROFILES_JSON || (!creating && jsonIndex>=gProfilesCount)){
    sendProfileTransferResult(commit.transferId,commit.action,HmiProtocol::ProfileTransferStatus::InvalidIndex,commit.profileIndex,0,0);return;
  }
  for(unsigned i=0;i<gProfilesCount;++i)if(i!=jsonIndex){
    JsonDocument other;deserializeJson(other,gProfiles[i].canonicalJson);
    if(other["id"] == incomingProfile.id){sendProfileTransferResult(commit.transferId,commit.action,HmiProtocol::ProfileTransferStatus::InvalidData,commit.profileIndex,0,0);return;}
  }
  ProfileJson& destination = gProfiles[jsonIndex];
  const ProfileJson previous=destination;
  ProfileStore::project(incomingProfile,incomingSteps,destination);
  if (creating) ++gProfilesCount;
  const int previousProfile = getCurrentProfileIndex();
  if(!saveProfilesJson()){
    destination=previous;if(creating)--gProfilesCount;
    sendProfileTransferResult(commit.transferId,commit.action,HmiProtocol::ProfileTransferStatus::StorageError,commit.profileIndex,0,0);return;
  }
  ProfilesRuntime::rebuild();
  setCurrentProfileIndex(constrain(previousProfile, 0, profilesCount - 1));
  const uint8_t runtimeIndex = creating ? static_cast<uint8_t>(jsonIndex + 2)
                                        : incomingProfile.profileIndex;
  sendProfileTransferResult(commit.transferId, commit.action,
                            HmiProtocol::ProfileTransferStatus::Accepted,
                            runtimeIndex, incomingProfile.stepCount, checksum);
  sendProfileList();
}

void deleteProfile(const HmiProtocol::ProfileDeleteRequestPayload& request) {
  const State state = Orchestrator::getState();
  if (state != IDLE && state != SHOT_END) {
    sendProfileTransferResult(request.transferId,
                              HmiProtocol::ProfileTransferAction::Delete,
                              HmiProtocol::ProfileTransferStatus::RejectedState,
                              request.profileIndex, 0, 0);
    return;
  }
  if (request.profileIndex < 2 || request.profileIndex - 2 >= gProfilesCount) {
    sendProfileTransferResult(request.transferId,
                              HmiProtocol::ProfileTransferAction::Delete,
                              HmiProtocol::ProfileTransferStatus::InvalidIndex,
                              request.profileIndex, 0, 0);
    return;
  }
  const uint8_t jsonIndex = request.profileIndex - 2;
  const ProfileJson removed=gProfiles[jsonIndex];
  for (uint8_t i = jsonIndex; i + 1 < gProfilesCount; ++i)
    gProfiles[i] = gProfiles[i + 1];
  gProfiles[gProfilesCount - 1] = ProfileJson{};
  --gProfilesCount;
  const int previousProfile = getCurrentProfileIndex();
  if(!saveProfilesJson()){
    for(unsigned i=gProfilesCount;i>jsonIndex;--i)gProfiles[i]=gProfiles[i-1];
    gProfiles[jsonIndex]=removed;++gProfilesCount;
    sendProfileTransferResult(request.transferId,HmiProtocol::ProfileTransferAction::Delete,HmiProtocol::ProfileTransferStatus::StorageError,request.profileIndex,0,0);return;
  }
  ProfilesRuntime::rebuild();
  setCurrentProfileIndex(constrain(previousProfile, 0, profilesCount - 1));
  sendProfileTransferResult(request.transferId,
                            HmiProtocol::ProfileTransferAction::Delete,
                            HmiProtocol::ProfileTransferStatus::Accepted,
                            request.profileIndex, 0, 0);
  sendProfileList();
}

void sendProfileList() {
  const uint8_t count = static_cast<uint8_t>(
      constrain(profilesCount, 0, HmiProtocol::MAX_PROFILE_ITEMS));
  const uint8_t current = static_cast<uint8_t>(
      constrain(getCurrentProfileIndex(), 0, count > 0 ? count - 1 : 0));
  for (uint8_t i = 0; i < count; ++i) {
    const Profile& profile = profiles[i];
    HmiProtocol::ProfileInfoPayload payload{};
    payload.index = i;
    payload.count = count;
    payload.currentIndex = current;
    payload.stepCount = profile.stepCount;
    payload.type = profile.type;
    payload.flags = (profile.isManual ? 1U : 0U) | (profile.isOff ? 2U : 0U);
    payload.targetWeightG = profile.totalTargetWeight;
    payload.maxShotTimeS = profile.maxShotTimeS;
    snprintf(payload.name, sizeof(payload.name), "%s", profile.name);
    sendFrame(HmiProtocol::MessageType::ProfileInfo, &payload,
              sizeof(payload));
  }
}

}  // namespace

bool handle(const HmiProtocol::FrameHeader& header, const uint8_t* payload,
            uint32_t nowMs) {
  (void)nowMs;
  if (header.type == HmiProtocol::MessageType::ProfileListRequest &&
      header.payloadLength == 0) {
    sendProfileList();
    return true;
  }
  if (header.type == HmiProtocol::MessageType::ProfileDetailRequest &&
      header.payloadLength == sizeof(HmiProtocol::ProfileDetailRequestPayload)) {
    HmiProtocol::ProfileDetailRequestPayload request{};
    memcpy(&request, payload, sizeof(request));
    sendProfileDetail(request.profileIndex);
    return true;
  }
  if (header.type == HmiProtocol::MessageType::ProfileBegin &&
      header.payloadLength == sizeof(HmiProtocol::ProfileBeginPayload)) {
    HmiProtocol::ProfileBeginPayload begin{};
    memcpy(&begin, payload, sizeof(begin));
    if (begin.action == HmiProtocol::ProfileTransferAction::Upload)
      beginProfileUpload(begin);
    return true;
  }
  if (header.type == HmiProtocol::MessageType::ProfileChunk &&
      header.payloadLength == sizeof(HmiProtocol::ProfileChunkPayload)) {
    HmiProtocol::ProfileChunkPayload step{};
    memcpy(&step, payload, sizeof(step));
    receiveProfileStep(step);
    return true;
  }
  if (header.type == HmiProtocol::MessageType::ProfileCommit &&
      header.payloadLength == sizeof(HmiProtocol::ProfileCommitPayload)) {
    HmiProtocol::ProfileCommitPayload commit{};
    memcpy(&commit, payload, sizeof(commit));
    if (commit.action == HmiProtocol::ProfileTransferAction::Upload)
      commitProfileUpload(commit);
    return true;
  }
  if (header.type == HmiProtocol::MessageType::ProfileDeleteRequest &&
      header.payloadLength == sizeof(HmiProtocol::ProfileDeleteRequestPayload)) {
    HmiProtocol::ProfileDeleteRequestPayload request{};
    memcpy(&request, payload, sizeof(request));
    deleteProfile(request);
    return true;
  }
  return false;
}

void sendList() { sendProfileList(); }

}  // namespace ProfileHandler
}  // namespace HmiLinkInternal
