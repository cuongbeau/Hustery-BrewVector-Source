#pragma once
#include "interfaces/profile_types.h"
#include "protocol/profile_codec.h"

namespace ProfileStore {
inline bool decode(const String& json,HmiProtocol::ProfileBeginPayload& b,HmiProtocol::ProfileChunkPayload* steps,String& error) {
  JsonDocument doc;
  if(deserializeJson(doc,json)){error="Invalid JSON";return false;}
  return ProfileCodec::parse(doc.as<JsonObjectConst>(),b,steps,error);
}
inline bool project(const HmiProtocol::ProfileBeginPayload& b,const HmiProtocol::ProfileChunkPayload* steps,ProfileJson& dst) {
  JsonDocument doc;ProfileCodec::write(doc.to<JsonObject>(),b,steps);
  dst=ProfileJson{};serializeJson(doc,dst.canonicalJson);
  dst.name=b.name;dst.note=b.note;dst.n=b.stepCount;dst.type=b.type;dst.maxShotTimeS=60;dst.totalTargetWeight=b.targetWeightG;
  for(unsigned i=0;i<b.stepCount;++i){auto& d=dst.steps[i];const auto& s=steps[i];
    snprintf(d.name,sizeof(d.name),"%s",s.name);d.mode=static_cast<StepMode>(s.mode);d.target=s.target;d.timeoutMs=s.timeoutMs;
    d.bumperType=static_cast<BumperType>(s.bumperType);d.bumperVal=s.bumperValue;
  }
  return true;
}
inline bool loadProjection(const String& json,ProfileJson& dst) {
  HmiProtocol::ProfileBeginPayload b{};HmiProtocol::ProfileChunkPayload steps[8]{};String error;
  return decode(json,b,steps,error) && project(b,steps,dst);
}
}
