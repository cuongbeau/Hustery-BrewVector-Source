// File: src/control/profile_engine.cpp
// Tách từ profiles.h + control_logic.h — Profile data + Executor
// Refactor: 2026-04-20
// Active executor: vendored BrewProcess with local machine/scale policies.

#include "profile_engine.h"
#include <BrewProcessRuntime.h>
#include <memory>
#include "src/control/shot_aggregator.h"
#include "src/app/orchestrator.h"
#include "src/app/quick_yield.h"
#include "interfaces/system_state.h"

// ====== Profile definitions ======
Profile profiles[MAX_PROFILES_RUNTIME];
int     profilesCount = 0;
bool    isPhysicalScaleConnected = false;

static int currentProfile = 0;

// ====== Helpers ======
static void copyName(char dst[24], const String& s) {
  size_t n = s.length();
  if (n > 23) n = 23;
  memcpy(dst, s.c_str(), n);
  dst[n] = '\0';
}

static void initProfileArrays(Profile &pf) {
  pf.type      = 1;
  pf.stepCount = 0;
  pf.totalTargetWeight = 0.0f;
  pf.maxShotTimeS      = 0.0f;
  pf.isManual = false;
  pf.isOff = false;
  for (uint8_t k = 0; k < MAX_POINTS_PER_PROFILE; k++) {
    pf.steps[k].target         = 0.0f;
    pf.steps[k].rampRate       = 0.0f;
    pf.steps[k].timeoutMs      = 1000.0f;
    pf.steps[k].bumperType     = BUMPER_NONE;
    pf.steps[k].bumperVal      = -1.0f;
    pf.steps[k].exitType       = EXIT_TRIGGER_NONE;
    pf.steps[k].exitVal        = -1.0f;
    pf.steps[k].exits          = StepExitConditions{};
  }
}

static float startValueForMode(StepMode mode, float currentP, float currentFlow,
                               float currentDuty) {
  float value = currentP;
  switch (mode) {
    case MODE_FLOW:
      value = currentFlow;
      break;
    case MODE_DUTY:
      value = currentDuty;
      break;
    case MODE_PRESSURE:
    default:
      value = currentP;
      break;
  }
  if (!isfinite(value)) return 0.0f;
  return value;
}

namespace ProfilesRuntime {
  void rebuild() {
    QuickYield::reset();
    static bool firstLoad = true;
    profilesCount = 0;

    // 0. OFF
    Profile &pfOff = profiles[profilesCount++];
    initProfileArrays(pfOff);
    copyName(pfOff.name, "OFF");
    pfOff.isOff = true;

    // 1. MANUAL
    Profile &pfMan = profiles[profilesCount++];
    initProfileArrays(pfMan);
    copyName(pfMan.name, "MANUAL");
    pfMan.isManual = true;

    // 2..N. User Profiles from JSON
    for (uint8_t i = 0; i < gProfilesCount; i++) {
        if (profilesCount >= MAX_PROFILES_RUNTIME) break;
        
        const ProfileJson &src = gProfiles[i];
        Profile &dst = profiles[profilesCount++];
        initProfileArrays(dst);
        copyName(dst.name, src.name);
        dst.totalTargetWeight = src.totalTargetWeight;
        dst.maxShotTimeS = src.maxShotTimeS;
        dst.type = src.type;
        
        strncpy(dst.note, src.note.c_str(), 299);
        dst.note[299] = '\0';
        
        dst.stepCount = src.n;
        for (uint8_t k = 0; k < src.n; k++) {
            dst.steps[k] = src.steps[k];
        }
        if (firstLoad) {
          JsonDocument meta; deserializeJson(meta, src.canonicalJson);
          if (i == 0 || meta["selected"] == true) currentProfile = i + 2;
        }
    }
    firstLoad = false;
    if (currentProfile >= profilesCount) currentProfile = profilesCount - 1;
  }
}

// ====== Profile Executor ======
ProfileExecutorState pfExec = {false, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0};
static ProfileStepExit lastStepExit;

static void recordStepExit(uint8_t step, const AdvancedStep &st,
                           ProfileStepExitReason reason, float actual,
                           float threshold, uint32_t elapsedMs) {
  lastStepExit.valid = true;
  lastStepExit.step = step;
  lastStepExit.mode = st.mode;
  lastStepExit.reason = reason;
  lastStepExit.actual = actual;
  lastStepExit.threshold = threshold;
  lastStepExit.stepTimeS = elapsedMs / 1000.0f;
  lastStepExit.timeoutS = st.timeoutMs / 1000.0f;
  strncpy(lastStepExit.name, st.name, sizeof(lastStepExit.name) - 1);
  lastStepExit.name[sizeof(lastStepExit.name) - 1] = '\0';
}

namespace {
std::unique_ptr<BrewProfileRuntime::BrewProcess> brew;
uint32_t lastProgressMs=0;
float yieldFallbackSeconds[8]{};
ProfileStepExitReason convertReason(BrewProfileRuntime::PhaseExitReason r,const BrewProfileRuntime::Phase& phase,
                                    float pressure,float flow,float weight,float water,float& actual,float& threshold){
  using R=BrewProfileRuntime::PhaseExitReason;using T=BrewProfileRuntime::TargetType;
  if(r==R::DURATION){actual=threshold=phase.duration;return PROFILE_EXIT_TIMEOUT;}
  if(r==R::SAFETY){actual=threshold=60;return PROFILE_EXIT_GLOBAL_TIME;}
  for(const auto& t:phase.targets){
    bool match=(r==R::TARGET_PRESSURE && t.type==T::TARGET_TYPE_PRESSURE)||(r==R::TARGET_FLOW && t.type==T::TARGET_TYPE_FLOW)||
      (r==R::TARGET_VOLUMETRIC && t.type==T::TARGET_TYPE_VOLUMETRIC)||(r==R::TARGET_PUMPED && t.type==T::TARGET_TYPE_PUMPED);
    if(!match)continue;
    actual=t.type==T::TARGET_TYPE_PRESSURE?pressure:t.type==T::TARGET_TYPE_FLOW?flow:t.type==T::TARGET_TYPE_VOLUMETRIC?weight:water;
    if(!t.isReached(actual))continue;threshold=t.value;
    const bool below=t.operator_==BrewProfileRuntime::TargetOperator::LTE;
    if(r==R::TARGET_PRESSURE)return below?PROFILE_EXIT_PRESSURE_BELOW:PROFILE_EXIT_PRESSURE_ABOVE;
    if(r==R::TARGET_FLOW)return below?PROFILE_EXIT_FLOW_BELOW:PROFILE_EXIT_FLOW_ABOVE;
    return r==R::TARGET_VOLUMETRIC?PROFILE_EXIT_WEIGHT:PROFILE_EXIT_WATER;
  }
  return PROFILE_EXIT_NONE;
}
}
void profileExecutorStart(int idx,float currentP,float currentFlow,float currentDuty){
  (void)currentDuty;brew.reset();pfExec=ProfileExecutorState{};lastStepExit=ProfileStepExit{};
  if(idx<2 || idx>=profilesCount || idx-2>=gProfilesCount)return;
  JsonDocument doc;if(deserializeJson(doc,gProfiles[idx-2].canonicalJson))return;
  const float quick=QuickYield::active();
  if(isfinite(quick)) {
    JsonArray phases=doc["phases"].as<JsonArray>();
    if(phases.size()) {
      JsonArray targets=phases[phases.size()-1]["targets"].as<JsonArray>();
      if(targets.isNull())targets=phases[phases.size()-1]["targets"].to<JsonArray>();
      for(int n=static_cast<int>(targets.size())-1;n>=0;--n)
        if(strcmp(targets[n]["type"] | "", "volumetric")==0)targets.remove(n);
      // Applied quick yield replaces the final weight exit with a whole-shot
      // stop in ShotAggregator. Earlier weight transitions remain in the profile.
    }
  }
  BrewProfileRuntime::Profile active{};auto obj=doc.as<JsonObject>();
  if(!BrewProfileRuntime::parseProfile(obj,active) || active.phases.empty())return;
  unsigned i=0;for(JsonObject p:doc["phases"].as<JsonArray>())yieldFallbackSeconds[i++]=p["transition"]["fallback_seconds"] | p["duration"].as<float>();
  const bool scale=isfinite(ShotAggregator::yieldControlWeight());
  brew.reset(new BrewProfileRuntime::BrewProcess(active,scale ? BrewProfileRuntime::ProcessTarget::VOLUMETRIC:BrewProfileRuntime::ProcessTarget::TIME,0,currentP,currentFlow));
  if(!scale)brew->useTimeFallback(yieldFallbackSeconds[0]);
  pfExec.running=true;pfExec.profileIndex=idx;pfExec.profileStartMs=brew->processStarted;pfExec.stepStartMs=brew->currentPhaseStarted;
  lastProgressMs=millis()-100;
}
void profileExecutorStop(){if(brew)brew->processPhase=BrewProfileRuntime::ProcessPhase::FINISHED;pfExec.running=false;brew.reset();}
ProfileStepExit profileExecutorLastExit(){return lastStepExit;}
float profileExecutorTemperature(){return brew && brew->isActive() ? brew->getTemperature() : 0.0f;}
void profileExecutorMarkManualStop(){
  if(!pfExec.running)return;
  const auto& st=profiles[pfExec.profileIndex].steps[pfExec.currentStep];
  recordStepExit(pfExec.currentStep,st,PROFILE_EXIT_MANUAL_STOP,0,0,millis()-pfExec.stepStartMs);
}
bool profileExecutorTick(float currentP,float currentWeight,float currentFlow,float currentWaterPumped,float currentDuty,
                         float& outTarget,StepMode& outMode,float& outPLim,float& outFLim){
  (void)currentDuty;outTarget=0;outPLim=outFLim=-1;
  if(!pfExec.running || !brew)return false;
  if(millis()-brew->processStarted>=60000){
    recordStepExit(pfExec.currentStep,profiles[pfExec.profileIndex].steps[pfExec.currentStep],PROFILE_EXIT_GLOBAL_TIME,60,60,millis()-pfExec.stepStartMs);
    pfExec.running=false;return false;
  }
  brew->updatePressure(currentP);brew->updateFlow(currentFlow);brew->updateWaterPumped(currentWaterPumped);
  if(!isfinite(currentWeight))brew->useTimeFallback(yieldFallbackSeconds[brew->phaseIndex]);
  if(millis()-lastProgressMs>=100){
    lastProgressMs=millis();
    if(isfinite(currentWeight) && brew->target==BrewProfileRuntime::ProcessTarget::VOLUMETRIC)brew->updateVolume(currentWeight);
    unsigned oldPhase=brew->phaseIndex;auto oldStarted=brew->currentPhaseStarted;
    float exitActual=0,exitThreshold=0;
    const auto exitReason=convertReason(brew->currentPhaseExitReason(),brew->currentPhase,currentP,currentFlow,currentWeight,
                                        currentWaterPumped-brew->phaseStartedPumped,exitActual,exitThreshold);
    brew->progress();
    if(oldPhase!=brew->phaseIndex || !brew->isActive()){
      recordStepExit(oldPhase,profiles[pfExec.profileIndex].steps[oldPhase],exitReason,exitActual,exitThreshold,millis()-oldStarted);
      if(!isfinite(currentWeight) && brew->isActive())brew->useTimeFallback(yieldFallbackSeconds[brew->phaseIndex]);
    }
  }
  pfExec.currentStep=brew->phaseIndex;pfExec.stepStartMs=brew->currentPhaseStarted;
  pfExec.running=brew->isActive();if(!pfExec.running)return false;
  if(!brew->isAdvancedPump()){outMode=MODE_DUTY;outTarget=brew->getPumpValue();}
  else if(brew->getPumpTarget()==BrewProfileRuntime::PumpTarget::PUMP_TARGET_FLOW){outMode=MODE_FLOW;outTarget=brew->getPumpFlow();outPLim=brew->getPumpPressure();}
  else{outMode=MODE_PRESSURE;outTarget=brew->getPumpPressure();outFLim=brew->getPumpFlow();}
  return true;
}

TargetSnap getTargetSnapshot(int pIdx, float tsec) {
  TargetSnap snap = {0.0f, MODE_PRESSURE};
  if (pIdx < 0 || pIdx >= profilesCount) return snap;
  const Profile &pf = profiles[pIdx];
  if (pf.stepCount == 0) return snap;

  float currentT = 0.0f;
  float currentP = 0.0f;

  for (uint8_t i = 0; i < pf.stepCount; i++) {
    const AdvancedStep &st = pf.steps[i];
    float duration = st.timeoutMs / 1000.0f;

    if (tsec <= currentT + duration) {
      snap.mode = st.mode;
      float dt = tsec - currentT;
      if (st.rampRate > 0.01f) {
        float diff = st.target - currentP;
        float dir = diff > 0.0f ? 1.0f : -1.0f;
        float p = currentP + (dir * st.rampRate) * dt;
        if (dir > 0 && p > st.target) p = st.target;
        if (dir < 0 && p < st.target) p = st.target;
        snap.val = p;
        return snap;
      }
      snap.val = st.target;
      return snap;
    }
    currentT += duration;
    currentP = st.target;
  }
  snap.val = pf.steps[pf.stepCount-1].target;
  snap.mode = pf.steps[pf.stepCount-1].mode;
  return snap;
}

float profileTargetBar(int pIdx, float tsec) {
  return getTargetSnapshot(pIdx, tsec).val;
}

// ====== Profile helpers ======
int getCurrentProfileIndex() { return currentProfile; }

void setCurrentProfileIndex(int idx) {
  if ((Orchestrator::getState() == IDLE || Orchestrator::getState() == SHOT_END) && idx >= 0 && idx < profilesCount) {
    if(currentProfile!=idx) QuickYield::reset();
    currentProfile = idx;
    shot.profileName = profiles[currentProfile].name;
  }
}

bool isManualProfile() { return (currentProfile == 1); }

bool isFullOutProfile() {
  return (!isManualProfile() && profiles[currentProfile].stepCount == 0);
}

bool isPressureProfile() {
  return (!isManualProfile() && profiles[currentProfile].stepCount > 0);
}

bool showTargetLine() {
  // Note: state check removed — orchestrator will handle this
  return (profiles[currentProfile].stepCount > 0);
}
