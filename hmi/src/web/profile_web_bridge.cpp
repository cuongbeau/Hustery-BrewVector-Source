#include "web/profile_web_bridge.h"

#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

#include "communication/main_link.h"
#include "web/owner_call.h"
#include "web/web_transaction.h"
#include "hmi_protocol.h"
#include "profile_codec.h"

namespace {
constexpr uint32_t RESPONSE_TIMEOUT_MS = 1400;
constexpr uint8_t BUILTIN_PROFILE_COUNT = 2;
constexpr uint8_t MAX_USER_PROFILES = 12;

const char* transferError(HmiProtocol::ProfileTransferStatus status) {
  using S = HmiProtocol::ProfileTransferStatus;
  switch (status) {
    case S::RejectedState: return "Main must be idle";
    case S::InvalidIndex: return "Invalid profile index";
    case S::InvalidData: return "Profile data rejected by Main";
    case S::Incomplete: return "Incomplete UART transfer";
    case S::ChecksumMismatch: return "Profile checksum mismatch";
    case S::StorageError: return "Main profile storage error";
    default: return "Profile transfer failed";
  }
}

bool waitForTransfer(MainLink& link, uint32_t generation,
                     HmiProtocol::ProfileTransferAction action,
                     String& error) {
  const uint16_t requestId=ownerValue([&](){return link.profileRequestId();});
  const uint32_t started = millis();
  while (millis() - started < RESPONSE_TIMEOUT_MS) {

    if (ownerValue([&](){return link.profileTransferGeneration();}) != generation) {
      const auto& result = ownerValue([&](){return link.profileTransferResult();});
      if(result.action!=action || (action!=HmiProtocol::ProfileTransferAction::Download && result.transferId!=requestId)){delay(2);continue;}
      if (result.action == action &&
          result.status == HmiProtocol::ProfileTransferStatus::Accepted)
        return true;
      error = transferError(result.status);
      return false;
    }
    delay(2);
  }
  error = "Main profile response timeout";
  return false;
}

bool refreshList(MainLink& link, String& error) {
  if (!ownerValue([&](){return link.connected();})) {
    error = "Main offline";
    return false;
  }
  if (!ownerValue([&](){return link.compatible();})) { error="Update both Main and HMI to the matching profile firmware"; return false; }
  const uint32_t generation=ownerValue([&](){return link.profileListGeneration();});
  if (!ownerValue([&](){return link.requestProfiles();})) {
    error = "Cannot request profile list";
    return false;
  }
  const uint32_t started = millis();
  while (millis() - started < RESPONSE_TIMEOUT_MS) {
    if(ownerValue([&](){return link.profileListGeneration();})!=generation)return true;
    delay(2);
  }
  error="Main profile list timeout";return false;
}

float numberOr(JsonVariantConst value, float fallback) {
  return value.is<float>() || value.is<int>() ? value.as<float>() : fallback;
}

bool parseDraft(JsonObjectConst source, uint8_t runtimeIndex, ProfileDetailData& draft, String& error) {
  draft=ProfileDetailData{};
  if(!ProfileCodec::parse(source,draft.profile,draft.steps,error))return false;
  draft.profile.profileIndex=runtimeIndex;draft.profile.action=HmiProtocol::ProfileTransferAction::Upload;draft.valid=true;
  return true;
}
}  // namespace

namespace ProfileWebBridge {

bool readSummaries(MainLink& link, String& response, String& error) {
  WebTransaction transaction(link);
  if(!transaction){error="Main transaction busy";return false;}
  if (!refreshList(link, error)) return false;
  JsonDocument doc;
  doc["ok"] = true;
  doc["version"] = millis();
  JsonArray profiles = doc["profiles"].to<JsonArray>();
  const uint8_t count = ownerValue([&](){return link.profileCount();});
  for (uint8_t index = BUILTIN_PROFILE_COUNT; index < count; ++index) {
    const auto info = ownerValue([&](){return link.profiles()[index];});
    JsonObject item = profiles.add<JsonObject>();
    item["index"] = index - BUILTIN_PROFILE_COUNT;
    item["runtimeIndex"] = index;
    item["label"] = String(info.name);
    item["type"] = info.type == 1 ? "standard" : "pro";
    item["phaseCount"] = info.stepCount;
    item["selected"] = info.currentIndex == index;
    item["targetWeight"] = info.targetWeightG;
    item["maxShotTime"] = info.maxShotTimeS;
  }
  serializeJson(doc, response);
  return true;
}

bool readOne(MainLink& link, uint8_t customIndex, String& response,
             String& error) {
  WebTransaction transaction(link);
  if(!transaction){error="Main transaction busy";return false;}
  if (!ownerValue([&](){return link.connected();})) {
    error = "Main offline";
    return false;
  }
  if (!ownerValue([&](){return link.compatible();})) {
    error = "Update both Main and HMI to the matching profile firmware";
    return false;
  }
  const uint8_t runtimeIndex = customIndex + BUILTIN_PROFILE_COUNT;
  if (runtimeIndex < BUILTIN_PROFILE_COUNT ||
      runtimeIndex >= ownerValue([&](){return link.profileCount();})) {
    error = "Profile index is out of date; reload the library";
    return false;
  }
  const uint32_t generation =
      ownerValue([&](){return link.profileTransferGeneration();});
  if (!ownerValue([&](){return link.requestProfileDetail(runtimeIndex);}) ||
      !waitForTransfer(link, generation,
                       HmiProtocol::ProfileTransferAction::Download, error))
    return false;
  const auto detail = ownerValue([&](){return link.profileDetail();});
  if (!detail.valid || detail.profile.profileIndex != runtimeIndex) {
    error = "Invalid profile detail from Main";
    return false;
  }
  JsonDocument doc;
  doc["ok"] = true;
  doc["index"] = customIndex;
  ProfileCodec::write(doc["profile"].to<JsonObject>(), detail.profile,
                 detail.steps);
  serializeJson(doc, response);
  return true;
}

bool saveOne(MainLink& link, const String& request, String& response,
             String& error) {
  WebTransaction transaction(link);
  if(!transaction){error="Main transaction busy";return false;}
  JsonDocument doc;
  if (deserializeJson(doc, request)) {
    error = "Invalid profile JSON";
    return false;
  }
  if (!doc["index"].is<int>() || !doc["profile"].is<JsonObject>()) {
    error = "Profile index and data are required";
    return false;
  }
  const int requestedIndex = doc["index"].as<int>();
  if (requestedIndex < 0 || requestedIndex >= MAX_USER_PROFILES) {
    error = "Invalid profile index";
    return false;
  }
  if (!refreshList(link, error)) return false;
  const uint8_t oldCustomCount =
      ownerValue([&](){return link.profileCount();}) > BUILTIN_PROFILE_COUNT
          ? ownerValue([&](){return link.profileCount();}) - BUILTIN_PROFILE_COUNT
          : 0;
  if (requestedIndex > oldCustomCount) {
    error = "Profile index is out of date; reload the library";
    return false;
  }
  if (requestedIndex == oldCustomCount &&
      oldCustomCount >= MAX_USER_PROFILES) {
    error = "Maximum 12 user profiles";
    return false;
  }

  ProfileDetailData draft{};
  const uint8_t runtimeIndex = requestedIndex < oldCustomCount
                                   ? requestedIndex + BUILTIN_PROFILE_COUNT
                                   : 0xFF;
  if (!parseDraft(doc["profile"].as<JsonObjectConst>(), runtimeIndex, draft,
                  error))
    return false;
  const uint32_t generation = ownerValue([&](){return link.profileTransferGeneration();});
  if (!ownerValue([&](){return link.uploadProfile(draft);}) ||
      !waitForTransfer(link, generation,
                       HmiProtocol::ProfileTransferAction::Upload, error))
    return false;

  JsonDocument output;
  output["ok"] = true;
  output["index"] = requestedIndex;
  output["created"] = requestedIndex == oldCustomCount;
  serializeJson(output, response);
  return true;
}

bool deleteOne(MainLink& link, const String& request, String& response,
               String& error) {
  WebTransaction transaction(link);
  if(!transaction){error="Main transaction busy";return false;}
  JsonDocument doc;
  if (deserializeJson(doc, request) || !doc["index"].is<int>()) {
    error = "Profile index is required";
    return false;
  }
  const int requestedIndex = doc["index"].as<int>();
  if (!refreshList(link, error)) return false;
  const uint8_t oldCustomCount =
      ownerValue([&](){return link.profileCount();}) > BUILTIN_PROFILE_COUNT
          ? ownerValue([&](){return link.profileCount();}) - BUILTIN_PROFILE_COUNT
          : 0;
  if (requestedIndex < 0 || requestedIndex >= oldCustomCount) {
    error = "Profile index is out of date; reload the library";
    return false;
  }
  const uint8_t runtimeIndex = requestedIndex + BUILTIN_PROFILE_COUNT;
  const uint32_t generation = ownerValue([&](){return link.profileTransferGeneration();});
  if (!ownerValue([&](){return link.deleteProfile(runtimeIndex);}) ||
      !waitForTransfer(link, generation,
                       HmiProtocol::ProfileTransferAction::Delete, error))
    return false;
  response = "{\"ok\":true}";
  return true;
}

bool replaceAll(MainLink& link, const String& request, String& response,
                String& error) {
  WebTransaction transaction(link);
  if(!transaction){error="Main transaction busy";return false;}
  JsonDocument doc;
  if (deserializeJson(doc, request)) {
    error = "Invalid profile JSON";
    return false;
  }
  JsonArrayConst profiles = doc["profiles"].as<JsonArrayConst>();
  if (profiles.isNull() || profiles.size() > MAX_USER_PROFILES) {
    error = "Profile list must contain at most 12 items";
    return false;
  }
  if (!refreshList(link, error)) return false;
  const uint8_t oldCustomCount =
      ownerValue([&](){return link.profileCount();}) > BUILTIN_PROFILE_COUNT
          ? ownerValue([&](){return link.profileCount();}) - BUILTIN_PROFILE_COUNT
          : 0;

  // Validate every item before changing persistent Main storage.
  ProfileDetailData draft{};
  uint8_t index = 0;
  for (JsonObjectConst source : profiles) {
    const uint8_t runtimeIndex = index < oldCustomCount
                                     ? index + BUILTIN_PROFILE_COUNT
                                     : 0xFF;
    if (!parseDraft(source, runtimeIndex, draft, error)) return false;
    ++index;
  }

  index = 0;
  for (JsonObjectConst source : profiles) {
    const uint8_t runtimeIndex = index < oldCustomCount
                                     ? index + BUILTIN_PROFILE_COUNT
                                     : 0xFF;
    if (!parseDraft(source, runtimeIndex, draft, error)) return false;
    const uint32_t generation = ownerValue([&](){return link.profileTransferGeneration();});
    if (!ownerValue([&](){return link.uploadProfile(draft);}) ||
        !waitForTransfer(link, generation,
                         HmiProtocol::ProfileTransferAction::Upload, error))
      return false;
    ++index;
  }
  for (uint8_t remaining = oldCustomCount; remaining > profiles.size();
       --remaining) {
    const uint8_t runtimeIndex =
        BUILTIN_PROFILE_COUNT + remaining - 1;
    const uint32_t generation = ownerValue([&](){return link.profileTransferGeneration();});
    if (!ownerValue([&](){return link.deleteProfile(runtimeIndex);}) ||
        !waitForTransfer(link, generation,
                         HmiProtocol::ProfileTransferAction::Delete, error))
      return false;
  }
  response = "{\"ok\":true}";
  return true;
}

}  // namespace ProfileWebBridge
