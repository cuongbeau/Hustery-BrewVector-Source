#pragma once
#include <cstring>
#include <stddef.h>
#include "heater_wire.h"
#include <cmath>

#if __cplusplus >= 201402L
#define HS_CONSTEXPR constexpr
#else
#define HS_CONSTEXPR inline
#endif
namespace HeaterSettings {
struct Field { const char* label; size_t offset; float step, low, high; uint8_t bytes, decimals; };
#define HF(name,label,step,low,high,dec) {label,offsetof(HeaterWire::Config,name),step,low,high,sizeof(HeaterWire::Config::name),dec}
constexpr Field fields[] = {
 HF(target,"Body target C",1,30,130,0), HF(kp,"Kp",0.1f,0,100,2),
 HF(ki,"Ki",0.005f,0,10,3), HF(kd,"Kd",0.5f,0,100,1),
 HF(ceiling,"Overtemp C",1,35,150,0), HF(maxPower,"Idle cap %",5,1,100,0),
 HF(waterMaxPower,"Water cap %",5,1,100,0), HF(stableSeconds,"Brew wait s",1,0,300,0),
 HF(ffEnabled,"Water FF",1,0,1,0), HF(heaterW,"Heater W",50,100,4000,0),
 HF(inletC,"Inlet C",1,0,60,0), HF(ffScale,"FF scale",0.05f,0,1,2),
 HF(autoSleep,"Auto sleep",1,0,1,0), HF(idleMinutes,"Idle min",1,1,240,0),
 HF(sleepDelaySeconds,"Extra delay s",5,0,3600,0)
};
#undef HF
HS_CONSTEXPR float value(const HeaterWire::Config& c,unsigned n){switch(n){case 0: return c.target;case 1: return c.kp;case 2: return c.ki;case 3: return c.kd;case 4: return c.ceiling;case 5: return c.maxPower;case 6: return c.waterMaxPower;case 7: return c.stableSeconds;case 8: return c.ffEnabled;case 9: return c.heaterW;case 10: return c.inletC;case 11: return c.ffScale;case 12: return c.autoSleep;case 13: return c.idleMinutes;case 14: return c.sleepDelaySeconds;default:return 0;}}
HS_CONSTEXPR void adjust(HeaterWire::Config& c,unsigned n,int direction){const auto& f=fields[n];float v=value(c,n)+direction*f.step;v=v<f.low?f.low:v>f.high?f.high:v;switch(n){case 0: c.target=v; break;case 1: c.kp=v; break;case 2: c.ki=v; break;case 3: c.kd=v; break;case 4: c.ceiling=v; break;case 5: c.maxPower=v; break;case 6: c.waterMaxPower=v; break;case 7: c.stableSeconds=v; break;case 8: c.ffEnabled=v; break;case 9: c.heaterW=v; break;case 10: c.inletC=v; break;case 11: c.ffScale=v; break;case 12: c.autoSleep=v; break;case 13: c.idleMinutes=v; break;case 14: c.sleepDelaySeconds=v; break;}}
HS_CONSTEXPR bool equal(const HeaterWire::Config& a,const HeaterWire::Config& b){for(unsigned n=0;n<15;++n)if(value(a,n)!=value(b,n))return false;return true;}
struct Editor {
 HeaterWire::Config draft{};
 bool loaded=false,dirty=false,conflict=false;
 uint32_t ticket=0,sentAt=0;
 HeaterWire::Op op=HeaterWire::Op::Off;
 const char* message="Waiting for Main";
 HeaterWire::Config baseline{};
 HS_CONSTEXPR void sync(const HeaterWire::Status& s,bool fresh,uint32_t now) {
   if(ticket && fresh && s.ticket==ticket) {
     ticket=0;
     if(s.result==HeaterWire::Result::Ok) {
       message=op==HeaterWire::Op::Configure?"Saved by Main":"Main confirmed";
       if(op==HeaterWire::Op::Configure){draft=baseline=s.config;loaded=true;dirty=conflict=false;}
     } else message=s.result==HeaterWire::Result::Busy?"Rejected: stop heater / brew":
         s.result==HeaterWire::Result::Invalid?"Rejected: invalid settings":"Rejected: safety / storage";
   }
   if(ticket && now-sentAt>4000){ticket=0;message="No ACK: check Main; no retry";}
   if(!fresh)return;
   if(!loaded){draft=baseline=s.config;loaded=true;conflict=false;message="Config loaded from Main";}
   else if(!dirty && !ticket){draft=baseline=s.config;conflict=false;}
   else if(dirty && !equal(baseline,s.config))conflict=true;
 }
 HS_CONSTEXPR bool valid() const {
   if(!loaded || conflict || draft.ceiling<draft.target+5)return false;
   for(unsigned n=0;n<15;++n){float v=value(draft,n);if(!std::isfinite(v)||v<fields[n].low||v>fields[n].high)return false;}
   return true;
 }
 HS_CONSTEXPR void change(unsigned n,int direction){if(!loaded||ticket||n>=15)return;adjust(draft,n,direction);dirty=true;message="Edited - not saved";}
 HS_CONSTEXPR void reload(const HeaterWire::Status& s){draft=baseline=s.config;loaded=true;dirty=conflict=false;message="Reloaded from Main";}
};
}

#undef HS_CONSTEXPR
