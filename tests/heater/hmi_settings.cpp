#include "hmi/src/ui/heater_settings.h"
using namespace HeaterSettings;
constexpr bool sameText(const char* a,const char* b){while(*a&&*b){if(*a++!=*b++)return false;}return *a==*b;}
constexpr bool editorScenario(){
 Editor e;HeaterWire::Status s;s.config.ceiling=125;
 e.sync(s,false,0);if(e.loaded)return false;
 e.sync(s,true,1);if(!e.loaded||!e.valid()||!sameText(e.message,"Config loaded from Main"))return false;
 e.change(2,-1);if(!e.dirty||e.draft.ki==s.config.ki)return false;
 auto expected=e.draft;e.sync(s,true,2);if(!equal(e.draft,expected))return false;
 e.ticket=123;e.sentAt=2;e.op=HeaterWire::Op::Configure;
 s.ticket=122;e.sync(s,true,3);if(!e.ticket)return false;
 s.ticket=123;s.result=HeaterWire::Result::Busy;e.sync(s,true,4);
 if(e.ticket||!e.dirty)return false;
 e.ticket=124;s.ticket=124;s.result=HeaterWire::Result::Ok;s.config=expected;e.sync(s,true,5);
 if(e.ticket||e.dirty||!equal(e.draft,s.config))return false;
 e.change(0,1);s.config.ffScale=.7f;e.sync(s,true,6);if(!e.conflict||e.valid())return false;
 e.reload(s);if(e.conflict||e.dirty)return false;
 e.ticket=125;e.sentAt=10;e.sync(s,false,4011);if(e.ticket)return false;
 e.draft.ceiling=e.draft.target+4;if(e.valid())return false;
 e.reload(s);for(int n=0;n<50;++n)e.change(11,1);
 return e.draft.ffScale==1 && e.draft.ffEnabled==s.config.ffEnabled;
}
static_assert(editorScenario(),"edit isolation, ACK rejection/success, conflict, timeout and bounds");
