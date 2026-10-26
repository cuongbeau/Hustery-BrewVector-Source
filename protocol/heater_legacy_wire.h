#pragma once
#include "heater_wire.h"
#include <cstring>
#include <stddef.h>
namespace HeaterLegacyWire {
constexpr uint8_t VERSION = 2;
#pragma pack(push,1)
struct Status {
  uint8_t version = VERSION;
  HeaterWire::Mode mode;
  HeaterWire::Fault fault;
  HeaterWire::Result result;
  uint32_t ticket, uptime, sampleAge, idleRemaining;
  uint8_t config[31];
  float raw, filtered, power, p, i, d;
  uint8_t valid, relay, idleExpired, ready;
};
struct Request {
  uint8_t version;
  HeaterWire::Op op;
  uint32_t ticket;
  uint8_t config[31];
  float testPower;
  uint16_t testSeconds;
};
struct StatusV3 {
  uint8_t version = 3;
  HeaterWire::Mode mode;
  HeaterWire::Fault fault;
  HeaterWire::Result result;
  uint32_t ticket, uptime, sampleAge, idleRemaining;
  uint8_t config[44];
  float raw, filtered, power, p, i, d;
  uint8_t valid, relay, idleExpired, ready;
};
struct RequestV3 {
  uint8_t version;
  HeaterWire::Op op;
  uint32_t ticket;
  uint8_t config[44];
  float testPower;
  uint16_t testSeconds;
};
#pragma pack(pop)
static_assert(sizeof(Status)==79 && sizeof(Request)==43,"frozen v2 layout");
static_assert(sizeof(StatusV3)==92 && sizeof(RequestV3)==56,"frozen v3 layout");
static_assert(offsetof(HeaterWire::Config,waterMaxPower)==44,"v3 config prefix");
static_assert(offsetof(HeaterWire::Config,heaterW)==31,"v2 config prefix");
constexpr bool accepts(uint8_t version,HeaterWire::Op op) {
  return version==VERSION && op==HeaterWire::Op::Off;
}
template<class T> inline T convertStatus(const HeaterWire::Status& current) {
  T old{};
  old.mode=current.mode;old.fault=current.fault;old.result=current.result;
  old.ticket=current.ticket;old.uptime=current.uptime;old.sampleAge=current.sampleAge;
  old.idleRemaining=current.idleRemaining;
  memcpy(old.config,&current.config,sizeof(old.config));
  old.raw=current.raw;old.filtered=current.filtered;old.power=current.power;
  old.p=current.p;old.i=current.i;old.d=current.d;old.valid=current.valid;
  old.relay=current.relay;old.idleExpired=current.idleExpired;old.ready=current.ready;
  return old;
}
inline Status status(const HeaterWire::Status& current) { return convertStatus<Status>(current); }
inline StatusV3 statusV3(const HeaterWire::Status& current) { return convertStatus<StatusV3>(current); }
inline bool offRequest(const void* data,size_t size,HeaterWire::Request& request) {
  if(size==sizeof(RequestV3)) {
    RequestV3 old{};memcpy(&old,data,size);
    if(old.version!=3 || old.op!=HeaterWire::Op::Off)return false;
    request=HeaterWire::Request{};request.op=HeaterWire::Op::Off;request.ticket=old.ticket;
    return true;
  }
  if(size!=sizeof(Request))return false;
  Request old{};memcpy(&old,data,size);
  if(!accepts(old.version,old.op))return false;
  request=HeaterWire::Request{};request.op=HeaterWire::Op::Off;request.ticket=old.ticket;
  return true;
}
}
