// File: src/app/calibration_store.cpp
// Tách từ calibration_store.h gốc — SPIFFS load/save implementations
// Refactor: 2026-04-20

#include "calibration_store.h"
#include "src/control/profile_store.h"
#include <FS.h>
#include <Preferences.h>
#include "src/model/thermal_model.h"
#include "src/model/pump_flow_calibration.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "interfaces/system_state.h"

bool ensureFS() {
  static bool inited = false;
  if (!inited) inited = SPIFFS.begin(true);
  return inited;
}

static void normalizeStepBumper(AdvancedStep &st) {
  if (st.bumperType != BUMPER_PRESS && st.bumperType != BUMPER_FLOW) {
    st.bumperType = BUMPER_NONE;
    st.bumperVal = -1.0f;
  } else if (st.bumperVal <= 0.0f || !isfinite(st.bumperVal)) {
    st.bumperVal = -1.0f;
  }
}

// Independent NVS namespace; no pressure/flow calibration is rewritten.
void loadThermalCal() {
  Preferences prefs;
  if (!prefs.begin("thermal-cal", true)) return;
  const float offset = prefs.getFloat("offset-c", ThermalCalibration::DefaultOffsetC);
  prefs.end();
  thermal.setOffsetC(offset); // Invalid stored values leave the default intact.
}

bool saveThermalCal(float offsetC) {
  if (!ThermalCalibration::valid(offsetC)) return false;
  Preferences prefs;
  if (!prefs.begin("thermal-cal", false)) return false;
  const bool saved = prefs.putFloat("offset-c", offsetC) == sizeof(float);
  prefs.end();
  if (saved) thermal.setOffsetC(offsetC);
  return saved;
}

// ====== Load/Save Pressure ======
void loadPressureCal() {
  if (!ensureFS()) return;
  File f = SPIFFS.open("/pressure_cal.json","r");
  if (!f) return;
  StaticJsonDocument<256> d;
  auto err = deserializeJson(d, f);
  f.close();
  if (err) return;

  gPress.offset = d["offset"] | gPress.offset;
  gPress.scale  = d["scale"]  | gPress.scale;
  gPress.divK   = d["divK"]   | gPress.divK;
}

void savePressureCal() {
  if (!ensureFS()) return;
  StaticJsonDocument<256> d;
  d["offset"] = gPress.offset;
  d["scale"]  = gPress.scale;
  d["divK"]   = gPress.divK;

  File f = SPIFFS.open("/pressure_cal.json","w");
  if (!f) return;
  serializeJson(d, f);
  f.close();
}

// ====== Load/Save Flow ======
void loadFlowCal() {
  if (!ensureFS()) return;
  File f = SPIFFS.open("/flow_cal.json","r");
  if (!f) return;

  StaticJsonDocument<768> d;
  auto err = deserializeJson(d, f);
  f.close();
  if (err) return;

  bool migrated = false;
  if (d.containsKey("q1_ml_s") && d.containsKey("q9_ml_s")) {
    const float q1 = d["q1_ml_s"];
    const float q9 = d["q9_ml_s"];
    if (isfinite(q1) && isfinite(q9) && q1 >= 0.5f && q1 <= 20.0f &&
        q9 >= 0.25f && q9 < q1) {
      gFlow.q1MlPerS = q1;
      gFlow.q9MlPerS = q9;
    }
  } else if (d.containsKey("ml_per_click")) {
    // One-time migration from the previous normalized default curve. This
    // preserves the exact pump model that was active before the Q1/Q9 editor.
    const float mlPerClick = d["ml_per_click"];
    if (isfinite(mlPerClick) && mlPerClick > 0.01f) {
      gFlow.q1MlPerS = PumpFlowCalibration::legacyQ1(mlPerClick);
      gFlow.q9MlPerS = PumpFlowCalibration::legacyQ9(mlPerClick);
      migrated = true;
    }
  }

  g_flowCalNeedUpdate = true;
  if (migrated) saveFlowCal();
}

void saveFlowCal() {
  if (!ensureFS()) return;
  StaticJsonDocument<256> d;
  d["model"] = "brewvector-q1-q9";
  d["q1_ml_s"] = gFlow.q1MlPerS;
  d["q9_ml_s"] = gFlow.q9MlPerS;

  File f = SPIFFS.open("/flow_cal.json","w");
  if (!f) return;
  serializeJson(d, f);
  f.close();
  g_flowCalNeedUpdate = true;
}

// ====== Load/Save Profiles ======
void loadProfilesJson() {
  gProfilesCount=0;
  if(!ensureFS()) return;
  // Read the previous profile file once when upgrading. Keep it untouched until
  // the new file has been written successfully.
  if(!SPIFFS.exists("/profiles_brewvector.json") && SPIFFS.exists("/profiles_brewvector.bak"))
    SPIFFS.rename("/profiles_brewvector.bak","/profiles_brewvector.json");
  const bool hasCurrent=SPIFFS.exists("/profiles_brewvector.json");
  const char* source=hasCurrent ? "/profiles_brewvector.json" :
      SPIFFS.exists("/profiles_gm.json") ? "/profiles_gm.json" : "/profiles_gm.bak";
  File file=SPIFFS.open(source,"r");
  JsonDocument doc;
  if(file) { auto err=deserializeJson(doc,file);file.close();if(err)return; }
  else {
    deserializeJson(doc,R"JSON({"profiles":[{"id":"flat-9","label":"Flat 9 bar","type":"pro","temperature":0,"phases":[{"name":"Preinfusion","phase":"preinfusion","valve":1,"duration":5,"pump":{"target":"pressure","pressure":3,"flow":0}},{"name":"Brew","phase":"brew","valve":1,"duration":30,"pump":{"target":"pressure","pressure":9,"flow":0},"transition":{"type":"linear","target":"time","duration":3,"adaptive":true}}]}]})JSON");
  }
  for(JsonObject p:doc["profiles"].as<JsonArray>()) {
    if(gProfilesCount>=MAX_PROFILES_JSON)break;
    String json;serializeJson(p,json);
    if(ProfileStore::loadProjection(json,gProfiles[gProfilesCount]))++gProfilesCount;
  }
  if(!hasCurrent) saveProfilesJson();
}

bool saveProfilesJson() {
  if(!ensureFS())return false;
  JsonDocument doc;auto arr=doc["profiles"].to<JsonArray>();
  for(unsigned i=0;i<gProfilesCount;++i){JsonDocument p;if(deserializeJson(p,gProfiles[i].canonicalJson))return false;arr.add(p.as<JsonObject>());}
  File f=SPIFFS.open("/profiles_brewvector.tmp","w");if(!f)return false;
  const size_t expected=measureJson(doc),written=serializeJson(doc,f);f.close();
  if(written!=expected)return false;
  if(SPIFFS.exists("/profiles_brewvector.bak") && !SPIFFS.remove("/profiles_brewvector.bak"))return false;
  const bool hadOld=SPIFFS.exists("/profiles_brewvector.json");
  if(hadOld && !SPIFFS.rename("/profiles_brewvector.json","/profiles_brewvector.bak"))return false;
  if(!SPIFFS.rename("/profiles_brewvector.tmp","/profiles_brewvector.json")){
    if(hadOld)SPIFFS.rename("/profiles_brewvector.bak","/profiles_brewvector.json");return false;
  }
  return true;
}

// ====== Load/Save LastShot ======
void loadLastShot() {
  if (!ensureFS()) return;
  File f = SPIFFS.open("/last_shot.json", "r");
  if (!f) return;
  StaticJsonDocument<256> d;
  auto err = deserializeJson(d, f);
  f.close();
  if (err) return;
  lastShot.weight = d["w"] | 0.0f;
  lastShot.time = d["t"] | 0.0f;
  lastShot.peakPressure = d["pk"] | 0.0f;
  lastShot.profileName = String((const char*)(d["p"] | "N/A"));
  lastShot.conductance = d["c"] | 0.35f;
  lastShot.adaptiveStall = d["s"] | 12.0f;
}

void saveLastShot() {
  if (!ensureFS()) return;
  StaticJsonDocument<256> d;
  d["w"] = lastShot.weight;
  d["t"] = lastShot.time;
  d["pk"] = lastShot.peakPressure;
  d["p"] = lastShot.profileName;
  d["c"] = lastShot.conductance;
  d["s"] = lastShot.adaptiveStall;
  File f = SPIFFS.open("/last_shot.json", "w");
  if (!f) return;
  serializeJson(d, f);
  f.close();
}
