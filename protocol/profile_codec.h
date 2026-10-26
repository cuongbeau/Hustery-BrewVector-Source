#pragma once
#include "hmi_protocol.h"
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

// Shared lossless mapping for the supported device limits (8 phases/8 targets).
// Never clamp malformed configuration into a different, runnable profile.
namespace ProfileCodec {
using namespace HmiProtocol;
inline bool number(JsonVariantConst v, float lo, float hi) {
  return v.is<float>() && isfinite(v.as<float>()) && v.as<float>() >= lo && v.as<float>() <= hi;
}
inline int choice(const char* value, const char* const* names, int count) {
  for (int i=0;i<count;++i) if (value && strcmp(value,names[i])==0) return i;
  return -1;
}
static const char* const transitions[] = {"instant","linear","ease-in","ease-out","ease-in-out"};
static const char* const axes[] = {"time","volumetric","pumped"};
static const char* const targets[] = {"volumetric","pressure","flow","pumped"};
inline bool parse(JsonObjectConst obj, ProfileBeginPayload& b, ProfileChunkPayload* steps, String& error) {
  const char* label = obj["label"] | "";
  const char* id = obj["id"] | "";
  const char* type = obj["type"] | "";
  JsonArrayConst phases = obj["phases"].as<JsonArrayConst>();
  if (!label[0] || strlen(label)>=sizeof(b.name) || !id[0] || strlen(id)>=sizeof(b.id) ||
      (strcmp(type,"pro") && strcmp(type,"standard")) || phases.size()==0 || phases.size()>8) {
    error="Require id, label (max 23 bytes), type standard/pro and 1..8 phases"; return false;
  }
  b = ProfileBeginPayload{};
  b.formatVersion=1; b.type=strcmp(type,"standard")==0 ? 1:0;
  b.stepCount=phases.size(); b.maxShotTimeS=60;
  snprintf(b.name,sizeof(b.name),"%s",label); snprintf(b.id,sizeof(b.id),"%s",id);
  const char* desc=obj["description"] | "";
  if (strlen(desc)>=sizeof(b.note)) {error="Description too long (max 191 bytes)";return false;}
  snprintf(b.note,sizeof(b.note),"%s",desc);
  if (!obj["temperature"].isNull() && !number(obj["temperature"],0,160)) {error="Invalid temperature";return false;}
  b.temperature=obj["temperature"] | 0.0f;
  b.profileFlags=(obj["favorite"] == true ? 1:0)|(obj["selected"] == true ? 2:0)|(obj["utility"] == true ? 4:0);
  unsigned i=0;
  for (JsonObjectConst p:phases) {
    auto& s=steps[i];s=ProfileChunkPayload{};s.stepIndex=i++;
    const char* name=p["name"] | "";const char* kind=p["phase"] | "brew";
    if (!name[0] || strlen(name)>=sizeof(s.name) || (strcmp(kind,"brew") && strcmp(kind,"preinfusion")) ||
        !number(p["duration"],0.1f,60)) {error="Invalid phase name/type or duration (0.1..60 s)";return false;}
    if (!(p["valve"] == 1 || p["valve"] == true)) {error="This hardware requires valve=1; independent phase valve is unavailable";return false;}
    s.valve=1;s.phaseType=strcmp(kind,"brew")==0 ? 1:0;
    snprintf(s.name,sizeof(s.name),"%s",name);s.timeoutMs=p["duration"].as<float>()*1000;
    s.temperature=p["temperature"] | 0.0f;
    if (!p["temperature"].isNull() && !number(p["temperature"],0,160)) {error="Invalid phase temperature";return false;}
    s.bumperType=0;s.bumperValue=-1;
    if (p["pump"].is<JsonObjectConst>()) {
      auto pump=p["pump"].as<JsonObjectConst>();const char* target=pump["target"] | "";
      if ((strcmp(target,"pressure") && strcmp(target,"flow")) || !number(pump["pressure"],-1,12) || !number(pump["flow"],-1,15)) {error="Invalid pump target/pressure/flow";return false;}
      float pressure=pump["pressure"],flow=pump["flow"];
      if ((pressure<0 && pressure!=-1) || (flow<0 && flow!=-1)) {error="Only -1 sentinel is supported";return false;}
      s.mode=strcmp(target,"pressure")==0 ? 0:1;
      s.target=s.mode==0 ? pressure:flow;s.bumperType=s.mode==0 ? 2:1;s.bumperValue=s.mode==0 ? flow:pressure;
    } else {
      if (!p["pump"].is<int>() || !number(p["pump"],0,100)) {error="Simple pump must be integer 0..100";return false;}
      s.mode=2;s.target=p["pump"].as<float>();
    }
    if (!p["transition"].isNull() && !p["transition"].is<JsonObjectConst>()) {error="Transition must be an object";return false;}
    auto tr=p["transition"].as<JsonObjectConst>();
    if (!tr["adaptive"].isNull() && !tr["adaptive"].is<bool>()) {error="Adaptive must be boolean";return false;}
    if (!tr["fallback_seconds"].isNull() && !number(tr["fallback_seconds"],0.1f,60)) {error="Invalid fallback seconds";return false;}
    int tt=choice(tr["type"] | "instant",transitions,5),ta=choice(tr["target"] | "time",axes,3);
    if (tt<0 || ta<0 || (!tr["duration"].isNull() && !number(tr["duration"],0,500))) {error="Invalid transition";return false;}
    s.transitionType=tt;s.transitionTarget=ta;s.transitionDuration=tr["duration"] | 0.0f;s.adaptive=tr["adaptive"] | false;
    s.yieldFallbackS=tr["fallback_seconds"] | (s.timeoutMs/1000.0f);
    if (!isfinite(s.yieldFallbackS) || s.yieldFallbackS<=0 || s.yieldFallbackS>s.timeoutMs/1000.0f) {error="Invalid yield fallback seconds";return false;}
    if (ta==0 && s.transitionDuration>60) {error="Time transition exceeds 60 seconds";return false;}
    auto ts=p["targets"].as<JsonArrayConst>();
    if ((!p["targets"].isNull() && ts.isNull()) || ts.size()>8) {error="At most 8 targets per phase";return false;}
    for (JsonObjectConst t:ts) {
      int kind=choice(t["type"] | "",targets,4);const char* op=t["operator"] | "gte";
      if (kind<0 || (strcmp(op,"gte") && strcmp(op,"lte")) || !number(t["value"],0,500)) {error="Invalid phase target";return false;}
      auto& d=s.targets[s.targetCount++];d.type=kind;d.op=strcmp(op,"gte")==0;d.value=t["value"];
      if (kind==0) b.targetWeightG=d.value;
    }
  }
  return true;
}
inline void write(JsonObject obj,const ProfileBeginPayload& b,const ProfileChunkPayload* steps) {
  // These are const array references, NOT string literals. ArduinoJson 7.4
  // can link const char (&)[N] as static storage (and use N-1 as length).
  // Own the strings: HMI reuses its UART detail buffer for the next profile.
  obj["id"]=String(b.id);obj["label"]=String(b.name);obj["type"]=b.type==1 ? "standard":"pro";obj["description"]=String(b.note);
  obj["temperature"]=b.temperature;obj["favorite"]=(b.profileFlags&1)!=0;obj["selected"]=(b.profileFlags&2)!=0;obj["utility"]=(b.profileFlags&4)!=0;
  auto phases=obj["phases"].to<JsonArray>();
  for(unsigned i=0;i<b.stepCount;++i) {
    const auto& s=steps[i];auto p=phases.add<JsonObject>();p["name"]=String(s.name);p["phase"]=s.phaseType ? "brew":"preinfusion";
    p["valve"]=s.valve;p["duration"]=s.timeoutMs/1000.0f;p["temperature"]=s.temperature;
    if(s.mode==2) p["pump"]=static_cast<int>(s.target);
    else {auto pump=p["pump"].to<JsonObject>();pump["target"]=s.mode==0 ? "pressure":"flow";pump["pressure"]=s.mode==0?s.target:s.bumperValue;pump["flow"]=s.mode==1?s.target:s.bumperValue;}
    auto tr=p["transition"].to<JsonObject>();tr["type"]=transitions[s.transitionType];tr["target"]=axes[s.transitionTarget];tr["duration"]=s.transitionDuration;tr["adaptive"]=s.adaptive!=0;tr["fallback_seconds"]=s.yieldFallbackS;
    auto ts=p["targets"].to<JsonArray>();for(unsigned k=0;k<s.targetCount;++k){auto t=ts.add<JsonObject>();t["type"]=targets[s.targets[k].type];t["operator"]=s.targets[k].op ? "gte":"lte";t["value"]=s.targets[k].value;}
  }
}
} // namespace ProfileCodec
