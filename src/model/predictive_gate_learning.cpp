#include "predictive_gate_learning.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

#include "interfaces/profile_types.h"

namespace {

constexpr char STORE_PATH[] = "/predictive_gate.json";
constexpr float DEFAULT_GATE_WATER_G = 20.0f;
constexpr float MIN_VALID_WATER_G = 8.0f;
constexpr float MAX_VALID_WATER_G = 45.0f;
constexpr uint32_t MIN_VALID_TIME_MS = 1500;
constexpr uint32_t MAX_VALID_TIME_MS = 30000;
constexpr uint32_t MAX_BASELINE_CAPTURE_MS = 2000;
constexpr uint32_t MAX_SCALE_AGE_MS = 300;
constexpr float FIRST_RISE_G = 0.08f;
constexpr float CONFIRMED_RISE_G = 0.18f;
constexpr uint32_t CONFIRM_MS = 300;
constexpr float LEARNING_ALPHA = 0.20f;
constexpr float MAX_WATER_UPDATE_G = 2.0f;
constexpr float MAX_TIME_UPDATE_MS = 1500.0f;

struct GateRecord {
  String profile;
  float waterG = DEFAULT_GATE_WATER_G;
  float timeMs = 0.0f;
  uint16_t count = 0;
};

GateRecord records[MAX_PROFILES_JSON];
uint8_t recordCount = 0;
bool loaded = false;
bool storeDirty = false;

bool learningActive = false;
String activeProfile;
bool baselineReady = false;
float baselineWeightG = 0.0f;
uint32_t candidateAtMs = 0;
float candidateWaterG = 0.0f;
bool pendingObservation = false;
float observedWaterG = 0.0f;
uint32_t observedTimeMs = 0;

bool saneRecord(float waterG, float timeMs, uint16_t count) {
  return count > 0 && isfinite(waterG) && isfinite(timeMs) &&
         waterG >= MIN_VALID_WATER_G && waterG <= MAX_VALID_WATER_G &&
         timeMs >= MIN_VALID_TIME_MS && timeMs <= MAX_VALID_TIME_MS;
}

int findRecord(const char* profileName) {
  if (!profileName || !profileName[0]) return -1;
  for (uint8_t i = 0; i < recordCount; ++i) {
    if (records[i].profile.equals(profileName)) return i;
  }
  return -1;
}

bool saveRecords() {
  if (!SPIFFS.begin(true)) return false;

  StaticJsonDocument<2048> doc;
  doc["version"] = 1;
  JsonArray items = doc.createNestedArray("profiles");
  for (uint8_t i = 0; i < recordCount; ++i) {
    const GateRecord& record = records[i];
    if (!saneRecord(record.waterG, record.timeMs, record.count)) continue;
    JsonObject item = items.createNestedObject();
    item["name"] = record.profile;
    item["water_g"] = record.waterG;
    item["time_ms"] = record.timeMs;
    item["count"] = record.count;
  }

  File file = SPIFFS.open(STORE_PATH, "w");
  if (!file) return false;
  serializeJson(doc, file);
  file.close();
  return true;
}

void loadRecords() {
  if (loaded) return;
  loaded = true;
  recordCount = 0;
  if (!SPIFFS.begin(true)) return;

  File file = SPIFFS.open(STORE_PATH, "r");
  if (!file) return;
  StaticJsonDocument<2048> doc;
  const DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error || (doc["version"] | 0) != 1) return;

  JsonArray items = doc["profiles"].as<JsonArray>();
  for (JsonObject item : items) {
    if (recordCount >= MAX_PROFILES_JSON) break;
    const char* name = item["name"] | "";
    const float waterG = item["water_g"] | NAN;
    const float timeMs = item["time_ms"] | NAN;
    const uint16_t count = item["count"] | 0;
    if (!name[0] || !saneRecord(waterG, timeMs, count)) continue;

    GateRecord& record = records[recordCount++];
    record.profile = name;
    record.waterG = waterG;
    record.timeMs = timeMs;
    record.count = count;
  }
}

}  // namespace

namespace PredictiveGateLearning {

void begin() { loadRecords(); }

Estimate estimateFor(const char* profileName) {
  loadRecords();
  Estimate estimate;
  const int index = findRecord(profileName);
  if (index < 0) return estimate;

  const GateRecord& record = records[index];
  estimate.waterG = record.waterG;
  estimate.timeMs = static_cast<uint32_t>(record.timeMs + 0.5f);
  estimate.sampleCount = record.count;
  estimate.learned = true;
  return estimate;
}

void startShot(const char* profileName, bool scaleOnline, float baselineWeight) {
  loadRecords();
  learningActive = profileName && profileName[0];
  activeProfile = learningActive ? profileName : "";
  baselineReady = scaleOnline && isfinite(baselineWeight);
  baselineWeightG = baselineReady ? baselineWeight : 0.0f;
  candidateAtMs = 0;
  candidateWaterG = 0.0f;
  pendingObservation = false;
  observedWaterG = 0.0f;
  observedTimeMs = 0;
}

void observeScale(bool scaleOnline, uint32_t sampleAgeMs, float weightG,
                  uint32_t elapsedMs, float waterPumpedG) {
  if (!learningActive || pendingObservation || !scaleOnline ||
      sampleAgeMs > MAX_SCALE_AGE_MS || !isfinite(weightG) ||
      !isfinite(waterPumpedG)) {
    return;
  }

  if (!baselineReady) {
    if (elapsedMs <= MAX_BASELINE_CAPTURE_MS) {
      baselineWeightG = weightG;
      baselineReady = true;
    }
    return;
  }

  const float riseG = weightG - baselineWeightG;
  if (riseG < FIRST_RISE_G) {
    candidateAtMs = 0;
    candidateWaterG = 0.0f;
    return;
  }

  if (candidateAtMs == 0) {
    candidateAtMs = elapsedMs > 0U ? elapsedMs : 1U;
    candidateWaterG = waterPumpedG;
    return;
  }

  if (riseG >= CONFIRMED_RISE_G && elapsedMs - candidateAtMs >= CONFIRM_MS) {
    if (candidateWaterG >= MIN_VALID_WATER_G &&
        candidateWaterG <= MAX_VALID_WATER_G &&
        candidateAtMs >= MIN_VALID_TIME_MS && candidateAtMs <= MAX_VALID_TIME_MS) {
      observedWaterG = candidateWaterG;
      observedTimeMs = candidateAtMs;
      pendingObservation = true;
      Serial.printf("[PredictiveGate] First drip observed: profile=%s water=%.2fg time=%.2fs\n",
                    activeProfile.c_str(), observedWaterG,
                    observedTimeMs / 1000.0f);
    }
  }
}

void finishShot(bool acceptObservation) {
  if (!learningActive || !acceptObservation || !pendingObservation) {
    learningActive = false;
    return;
  }

  int index = findRecord(activeProfile.c_str());
  if (index < 0) {
    if (recordCount >= MAX_PROFILES_JSON) {
      learningActive = false;
      return;
    }
    index = recordCount++;
    records[index].profile = activeProfile;
    records[index].waterG = observedWaterG;
    records[index].timeMs = static_cast<float>(observedTimeMs);
    records[index].count = 1;
  } else {
    GateRecord& record = records[index];
    const float waterDelta = constrain(
        (observedWaterG - record.waterG) * LEARNING_ALPHA,
        -MAX_WATER_UPDATE_G, MAX_WATER_UPDATE_G);
    const float timeDelta = constrain(
        (static_cast<float>(observedTimeMs) - record.timeMs) * LEARNING_ALPHA,
        -MAX_TIME_UPDATE_MS, MAX_TIME_UPDATE_MS);
    record.waterG += waterDelta;
    record.timeMs += timeDelta;
    if (record.count < UINT16_MAX) ++record.count;
  }

  // Do not write flash in the shot-stop path. Orchestrator persists adaptive
  // state later in IDLE, after the pump and valve handling are complete.
  storeDirty = true;
  const GateRecord& record = records[index];
  Serial.printf("[PredictiveGate] Learned: profile=%s water=%.2fg time=%.2fs n=%u\n",
                record.profile.c_str(), record.waterG, record.timeMs / 1000.0f,
                record.count);
  learningActive = false;
}

void persistIfNeeded() {
  if (!storeDirty) return;
  if (saveRecords()) storeDirty = false;
}

}  // namespace PredictiveGateLearning
