#include "quick_yield.h"
#include "quick_yield_session.h"
#include <ArduinoJson.h>
#include <math.h>
#include "src/app/orchestrator.h"
#include "src/control/profile_engine.h"
#include "src/hardware/scale_service.h"
#include "src/communication/hmi_link.h"
#include "interfaces/system_state.h"
namespace QuickYield {
namespace {
QuickYieldSession session;
float baseline=0;
int selected=-1;
bool supported=false;
void refresh() {
  selected=getCurrentProfileIndex(); baseline=0;
  supported=selected>=0 && selected<profilesCount && !profiles[selected].isOff;
  if(!supported || selected<2)return;
  JsonDocument doc;
  if(deserializeJson(doc,gProfiles[selected-2].canonicalJson)){supported=false;return;}
  JsonArray phases=doc["phases"].as<JsonArray>();
  if(phases.size()==0){supported=false;return;}
  unsigned weightTargets=0;
  for(JsonObject t:phases[phases.size()-1]["targets"].as<JsonArray>()) {
    if(strcmp(t["type"] | "", "volumetric"))continue;
    if(++weightTargets>1)supported=false; // Ambiguous compound exits stay in the Web editor.
    if(strcmp(t["operator"] | "gte", "gte")){supported=false;continue;}
    baseline=t["value"] | 0.0f;
  }
}
bool sessionAllowed() { return Orchestrator::getState()==IDLE &&
  HmiLink::compatible() && supported; }
bool beginAllowed() { return sessionAllowed() && ScaleService::isOnline(); }
}
void reset(){session.reset();selected=-1;}
void poll(){
  if(selected!=getCurrentProfileIndex()){reset();refresh();}
  // A physical tare may pause scale samples long enough to look offline.
  // Keep the editor and an applied next-shot override through that transient;
  // beginShot still requires a live scale before it can activate the limit.
  session.poll(sessionAllowed(),millis());
}
bool command(uint8_t op,uint32_t id){
  poll();
  if(!sessionAllowed() || !id || (op==0 && !ScaleService::isOnline()))return false;
  return session.command(op,id,baseline,millis());
}
bool editing(){return session.editor!=0;}
void rotate(int steps){session.rotate(steps);}
float displayed(){return session.editor?session.draft:isfinite(session.pending)?session.pending:baseline;}
uint32_t token(){return session.editor;}
uint8_t flags(){return (beginAllowed()?1:0)|(session.editor?2:0)|(isfinite(session.pending)?4:0);}
void beginShot(bool brew){poll();session.consume(brew && beginAllowed());}
void endShot(){session.running=NAN;}
float active(){return session.running;}
}
