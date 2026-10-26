#include "src/control/heater_core.h"
using namespace HeaterControl;
constexpr Core prepared(uint32_t t=100) {
  Core c; c.mainAt=t; c.sample(80,t);
  Request r; r.op=Op::Configure; r.config.ceiling=125;
  c.command(r,t,true); return c;
}
constexpr bool startup() { Core c; c.tick(100,false); Request r; r.op=Op::Enable; return !c.s.relay && c.command(r,100,true)==Result::Unsafe; }
constexpr bool stale() { auto c=prepared(); Request r; r.op=Op::Enable;c.command(r,100,true);c.mainAt=5101;c.tick(5101,false);return c.s.fault==Fault::Stale&&!c.s.relay; }
constexpr bool stalled() { auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);c.sample(85,1000);c.tick(1101,false);return c.s.fault==Fault::MainStalled&&!c.s.relay; }
constexpr bool sensor() {
  auto c=prepared(); Request r;r.op=Op::Enable;c.command(r,100,true);
  for(unsigned n=1;n<=18;++n){
    const unsigned t=100+n*250;c.mainAt=t;c.sample(NAN,t);c.tick(t,false);
    if(n<18 && c.s.fault!=Fault::None)return false;
  }
  if(c.sensorWindow.errors!=18 || c.s.fault!=Fault::Sensor || c.s.relay)return false;
  r.op=Op::Off;c.command(r,4600,false);
  if(c.sensorWindow.errors!=18 || c.s.mode!=Mode::Fault)return false;
  c.mainAt=4850;c.sample(80,4850);r.op=Op::ClearFault;
  if(c.command(r,4850,true)!=Result::Unsafe)return false;
  c.mainAt=5100;c.sample(80,5100);
  if(c.command(r,5100,true)!=Result::Unsafe)return false;
  c.mainAt=5350;c.sample(80,5350);
  if(c.s.fault!=Fault::Sensor || c.s.mode!=Mode::Fault || c.sensorWindow.errors!=17)return false;
  return c.command(r,5350,true)==Result::Ok && c.s.mode==Mode::Off && !c.s.relay;
}
// Feed real-cadence good samples while exercising long idle timers.
constexpr void advance(Core& c, uint32_t to, bool water=false) {
  while(c.sampleAt < to) {
    const auto t=c.sampleAt+250 < to ? c.sampleAt+250 : to;
    c.mainAt=t;c.sample(80,t);c.tick(t,water);
  }
}
constexpr bool overtemp() { auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);c.sample(126,300);return c.s.fault==Fault::Overtemp&&c.s.power==0; }
constexpr bool testLimit() { auto c=prepared();Request r;r.op=Op::Test;r.testSeconds=1;c.command(r,100,true);c.tick(100,false);if(!c.s.relay)return false;c.mainAt=1100;c.sample(80,1100);c.tick(1100,false);return c.s.mode==Mode::Off&&!c.s.relay; }
constexpr bool idle() { auto c=prepared();c.s.config.idleMinutes=1;Request r;r.op=Op::Enable;c.command(r,100,true);advance(c,60100);return c.s.mode==Mode::Off&&c.s.idleExpired; }
constexpr bool waterIdle() { auto c=prepared();c.s.config.idleMinutes=1;Request r;r.op=Op::Enable;c.command(r,100,true);advance(c,60100,true);return c.s.mode==Mode::Pid&&!c.s.idleExpired; }
constexpr bool windup() { auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);for(unsigned t=300;t<10000;t+=200){c.mainAt=t;c.sample(50,t);c.tick(t,false);}return c.s.i==0&&c.s.power==30; }
constexpr bool wrap() { auto c=prepared(UINT32_MAX-100);Request r;r.op=Op::Test;r.testSeconds=1;c.command(r,UINT32_MAX-100,true);c.mainAt=200;c.sample(80,200);c.tick(200,false);return c.s.mode==Mode::Test; }
constexpr bool configGuards() { Config c;c.ceiling=125;if(!validConfig(c))return false;c.kp=NAN;return !validConfig(c); }
constexpr bool offAlways() { auto c=prepared();Request r;r.op=Op::Test;c.command(r,100,true);r.op=Op::Off;return c.command(r,200,false)==Result::Ok&&!c.s.relay&&c.s.mode==Mode::Off; }
constexpr bool clear() { auto c=prepared();c.trip(Fault::Overtemp);Request r;r.op=Op::ClearFault;return c.command(r,100,true)==Result::Ok&&c.s.mode==Mode::Off&&!c.s.relay; }
constexpr bool testWaterStop() { auto c=prepared();Request r;r.op=Op::Test;c.command(r,100,true);c.tick(100,true);return c.s.mode==Mode::Off&&!c.s.relay; }
constexpr bool unconfiguredHot() { Core c;c.sample(151,100);return c.s.fault==Fault::Overtemp&&!c.s.relay; }
static_assert(startup(),"uncommissioned startup OFF");
static_assert(stale(),"sample stale shutdown");
static_assert(stalled(),"main-loop watchdog shutdown");
static_assert(sensor(),"sensor fault latches after recovery");
static_assert(overtemp(),"raw overtemp bypasses filter");
static_assert(testLimit(),"test deadline");
static_assert(idle(),"idle expires");
static_assert(waterIdle(),"water resets inactivity");
static_assert(windup(),"no integral accumulation during saturation");
static_assert(wrap(),"millis wrap");
static_assert(configGuards(),"nonfinite parameters");
static_assert(offAlways(),"off bypasses state guard");
static_assert(clear(),"clear fault never enables heater");
static_assert(testWaterStop(),"water cancels manual heater test");
static_assert(unconfiguredHot(),"absolute overtemp warning before commissioning");

constexpr bool sleepDisabled() {
  auto c=prepared();c.s.config.autoSleep=0;c.s.config.idleMinutes=1;
  Request r;r.op=Op::Enable;c.command(r,100,true);
  advance(c,120100);
  return c.s.mode==Mode::Pid && c.s.idleRemaining==0;
}
constexpr bool sleepDelay() {
  auto c=prepared();c.s.config.idleMinutes=1;c.s.config.sleepDelaySeconds=10;
  Request r;r.op=Op::Enable;c.command(r,100,true);
  advance(c,60100);
  if(c.s.mode!=Mode::Pid || c.s.idleRemaining!=10000)return false;
  advance(c,70100);
  return c.s.mode==Mode::Off && c.s.idleExpired;
}
constexpr bool readiness() {
  Core c;c.mainAt=100;c.sample(115,100);Request r;r.op=Op::Configure;
  r.config.ceiling=125;r.config.stableSeconds=1;c.command(r,100,true);
  r.op=Op::Enable;c.command(r,100,true);
  for(unsigned t=300;t<=1100;t+=200){c.mainAt=t;c.sample(115,t);c.tick(t,false);if(c.s.ready)return false;}
  c.mainAt=1300;c.sample(115,1300);c.tick(1300,false);
  if(!c.s.ready || c.activityAt!=1300)return false;
  c.mainAt=1500;c.sample(99,1500);c.tick(1500,false);
  return !c.s.ready && c.activityAt==1300;
}
constexpr bool preheatOnly() {
  auto c=prepared();Request r;r.op=Op::Enable;
  return c.command(r,100,true)==Result::Ok && c.s.mode==Mode::Pid && !c.s.ready;
}
constexpr bool badTimers() {
  Config c;c.ceiling=125;c.autoSleep=2;if(validConfig(c))return false;
  c.autoSleep=1;c.stableSeconds=301;if(validConfig(c))return false;
  c.stableSeconds=0;c.sleepDelaySeconds=3601;return !validConfig(c);
}
static_assert(sleepDisabled(),"disable timeout retains PID");
static_assert(sleepDelay(),"idle plus configurable delay");
static_assert(readiness(),"continuous stable interval and one-shot idle reset");
constexpr bool relaxedBrewMinimum() {
  Core c;c.mainAt=100;c.sample(100,100);Request r;r.op=Op::Configure;
  r.config.ceiling=125;r.config.stableSeconds=1;c.command(r,100,true);
  r.op=Op::Enable;c.command(r,100,true);
  for(unsigned t=300;t<=1100;t+=200){c.mainAt=t;c.sample(t%400?118:113,t);c.tick(t,false);if(c.s.ready)return false;}
  c.mainAt=1300;c.sample(118,1300);c.tick(1300,false);
  if(!c.s.ready)return false; // no reset for crossing the old 117C upper band
  c.mainAt=1500;c.sample(125,1500);c.tick(1500,false);
  return !c.s.ready && c.s.fault==Fault::Overtemp && c.s.power==0;
}
static_assert(relaxedBrewMinimum(),"100C body readiness tolerates noise but never bypasses overtemp");
static_assert(preheatOnly(),"enable enters warmup not ready");
static_assert(badTimers(),"validate timer fields");
constexpr bool clearBeforeCommissioning() {
  Core c;c.mainAt=100;c.sample(NAN,100);
  Request r;r.op=Op::ClearFault;
  if(c.command(r,100,true)!=Result::Unsafe)return false;
  c.mainAt=300;c.sample(22,300);
  if(c.command(r,300,true)!=Result::Ok || c.s.mode!=Mode::Off || c.s.relay)return false;
  r.op=Op::Enable;
  return c.command(r,300,true)==Result::Unsafe && c.s.config.ceiling==0;
}
constexpr bool cannotClearHotOrStale() {
  Core c;c.mainAt=100;c.sample(151,100);
  Request r;r.op=Op::ClearFault;
  if(c.command(r,100,true)!=Result::Unsafe)return false;
  c.sample(22,300);c.mainAt=1901;
  return c.command(r,1901,true)==Result::Unsafe;
}
static_assert(clearBeforeCommissioning(),"clear recovered sensor with ceiling0 stays OFF and cannot enable");
static_assert(cannotClearHotOrStale(),"clear never bypasses heat or freshness checks");

constexpr bool heldPid() {
  auto c=prepared();c.s.config.kp=0;c.s.config.ki=0.2f;
  Request r;r.op=Op::Enable;c.command(r,100,true);
  c.mainAt=350;c.sample(80,350);c.tick(350,false);
  const auto goodI=c.s.i;
  c.mainAt=600;c.sample(NAN,600);c.tick(600,false);
  if(c.s.fault!=Fault::None || c.s.valid || c.sampleAt!=350 || c.s.sampleAge!=250 ||
      c.s.filtered!=80 || c.s.raw!=80 || c.s.i!=goodI)return false;
  c.mainAt=1100;c.tick(1100,false);
  if(c.s.i<=goodI || c.s.power<=0)return false; // 1Hz PID uses held good temperature
  c.mainAt=1350;c.sample(82,1350);c.tick(1350,false);
  return c.s.valid && c.s.filtered>80 && c.s.filtered<82 && c.s.mode==Mode::Pid;
}
constexpr bool heldTimeout() {
  auto c=prepared();Request r;r.op=Op::Test;c.command(r,100,true);
  for(unsigned t=350;t<=1100;t+=250){c.mainAt=t;c.sample(NAN,t);c.tick(t,false);}
  if(c.s.fault!=Fault::None || c.s.sampleAge!=1000)return false;
  c.mainAt=1601;c.tick(1601,false);
  if(!c.sensorPaused || c.s.power!=0 || c.s.relay || c.s.fault!=Fault::None)return false;
  c.mainAt=5101;c.tick(5101,false);c.sample(80,5102);
  return c.s.fault==Fault::Stale && !c.s.relay && c.s.power==0;
}
constexpr bool lateGoodCannotHideTimeout() {
  auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);
  c.mainAt=5101;c.sample(80,5101);
  return c.s.valid && c.s.fault==Fault::Stale && !c.s.relay;
}
constexpr bool startupFaultWindow() {
  Core c;Request r;r.op=Op::Enable;
  for(unsigned n=1;n<=18;++n){
    c.mainAt=n*250;c.sample(NAN,n*250);c.tick(n*250,false);
    if(c.command(r,n*250,true)!=Result::Unsafe || c.s.relay)return false;
    if(n<18 && c.s.fault!=Fault::None)return false;
  }
  return c.s.fault==Fault::Sensor;
}
constexpr bool overtempAfterHeld() {
  auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);
  c.mainAt=350;c.sample(NAN,350);c.tick(350,false);
  c.mainAt=600;c.sample(126,600);
  return c.s.fault==Fault::Overtemp && c.s.power==0 && !c.s.relay;
}
constexpr bool windowDoesNotReset() {
  auto c=prepared();
  for(unsigned n=1;n<=17;++n){c.mainAt=100+n*250;c.sample(NAN,c.mainAt);}
  c.mainAt=4600;c.sample(80,4600);
  Request r;r.op=Op::Configure;r.config.ceiling=125;
  if(c.command(r,4600,true)!=Result::Ok)return false;
  r.op=Op::ClearFault;if(c.command(r,4600,true)!=Result::Ok)return false;
  r.op=Op::Enable;
  return c.sensorWindow.errors==17 && c.command(r,4600,true)==Result::Unsafe &&
      c.s.mode==Mode::Off && !c.s.relay;
}
static_assert(heldPid(),"bad read holds temperature; independent 1Hz PID still computes");
static_assert(heldTimeout(),"soft pause and hard freshness timeout without further sensor reads");
static_assert(lateGoodCannotHideTimeout(),"late valid read cannot conceal stale interval");
static_assert(startupFaultWindow(),"no valid reading cannot enable; eighteenth error latches at startup");
static_assert(overtempAfterHeld(),"overtemperature still bypasses filter after a tolerated error");
static_assert(windowDoesNotReset(),"commands cannot erase sensor window history");
constexpr bool intermittentRun() {
  auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);
  for(unsigned n=1;n<=200;++n) {
    const auto t=100+n*250;c.mainAt=t;
    c.sample(n%20>=1 && n%20<=3 ? NAN : 80,t);c.tick(t,false);
    if(c.s.fault!=Fault::None || c.s.power>c.s.config.maxPower)return false;
  }
  return c.s.mode==Mode::Pid && c.sensorWindow.count==20 && c.sensorWindow.errors==3;
}
constexpr bool sixtyPercentContinues() {
  auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);
  for(unsigned n=1;n<=20;++n) {
    const auto t=100+n*250;c.mainAt=t;const auto slot=n%5;
    c.sample(slot>=1 && slot<=3 ? NAN : 80,t);c.tick(t,false);
    if(c.s.fault!=Fault::None || c.sensorPaused)return false;
  }
  return c.s.mode==Mode::Pid && c.sensorWindow.count==20 &&
      c.sensorWindow.errors==12 && !c.sensorPaused;
}
constexpr bool densityPauseAndHysteresis() {
  auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);
  uint32_t t=100;
  for(unsigned n=0;n<20;++n){t+=250;c.mainAt=t;c.sample(80,t);c.tick(t,false);}
  for(unsigned n=1;n<=17;++n){
    t+=250;c.mainAt=t;c.sample(n%5?NAN:80,t);c.tick(t,false);
  }
  if(c.sensorWindow.errors!=14 || !c.sensorPaused || !c.sensorDensityPaused ||
      c.s.fault!=Fault::None || c.s.power!=0 || c.s.relay)return false;
  while(c.sensorWindow.errors>9){t+=250;c.mainAt=t;c.sample(80,t);c.tick(t,false);}
  if(!c.sensorPaused || !c.sensorDensityPaused || c.s.fault!=Fault::None)return false;
  while(c.sensorWindow.errors>8){t+=250;c.mainAt=t;c.sample(80,t);c.tick(t,false);}
  return !c.sensorPaused && !c.sensorDensityPaused && c.s.fault==Fault::None &&
      c.s.mode==Mode::Pid;
}
constexpr bool densityHardFault() {
  auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);
  uint32_t t=100;
  for(unsigned n=0;n<20;++n){t+=250;c.mainAt=t;c.sample(80,t);c.tick(t,false);}
  for(unsigned n=1;n<=20;++n){
    t+=250;c.mainAt=t;const bool good=n==7 || n==14;
    c.sample(good?80:NAN,t);c.tick(t,false);
    if(n<20 && c.s.fault!=Fault::None)return false;
  }
  return c.sensorWindow.errors==18 && c.s.fault==Fault::Sensor &&
      c.s.mode==Mode::Fault && !c.s.relay;
}
constexpr bool holdAcrossClockWrap() {
  const uint32_t start=UINT32_MAX-400;auto c=prepared(start);
  Request r;r.op=Op::Enable;c.command(r,start,true);
  c.mainAt=start+750;c.sample(NAN,start+750);c.tick(start+750,false);
  if(c.s.fault!=Fault::None || c.s.sampleAge!=750)return false;
  c.mainAt=start+1501;c.tick(start+1501,false);
  if(!c.sensorPaused || c.s.fault!=Fault::None)return false;
  c.mainAt=start+5001;c.tick(start+5001,false);
  return c.s.fault==Fault::Stale && !c.s.relay;
}
static_assert(intermittentRun(),"15 percent errors including triplets remain below density/freshness limits at exact cadence");
static_assert(sixtyPercentContinues(),"60 percent interleaved errors warn but keep heating active");
static_assert(densityPauseAndHysteresis(),"70 percent pauses SSR and recovery waits for 40 percent");
static_assert(densityHardFault(),"90 percent errors latch the sensor fault");
static_assert(holdAcrossClockWrap(),"held sample timeout remains correct across millis wrap");

constexpr void keepMain(Core& c, uint32_t to) {
  while(c.mainAt<to) {
    c.mainAt = c.mainAt+250<to ? c.mainAt+250 : to;
    c.tick(c.mainAt,false);
  }
}
constexpr bool pauseAndResume() {
  auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);
  c.s.config.ki=1;c.s.config.kd=5;c.s.i=2.5f;c.pid.integral=2.5f;
  c.mainAt=350;c.sample(80,350);c.tick(350,false);
  c.s.ready=1;c.stableTracking=true;c.firstReady=true;
  const auto integral=c.s.i;const auto activity=c.activityAt;
  keepMain(c,1850);
  if(c.sensorPaused)return false; // exactly 1500ms is still allowed
  keepMain(c,1851);
  if(!c.sensorPaused || c.s.relay || c.s.power!=0 || c.s.ready ||
     c.stableTracking || !c.firstReady || c.s.i!=integral || c.s.fault!=Fault::None)return false;
  keepMain(c,2100);c.sample(NAN,2100);
  if(c.s.i!=integral || !c.sensorPaused || c.s.power!=0)return false;
  keepMain(c,2350);c.sample(114,2350);
  if(c.sensorPaused || c.s.mode!=Mode::Pid || c.s.fault!=Fault::None ||
     c.s.i!=integral || c.s.d!=0 || c.s.power!=2+integral ||
     c.s.relay || c.activityAt!=activity)return false;
  c.tick(2350,false);
  for(unsigned t=2600;t<=3350;t+=250){c.mainAt=t;c.sample(114,t);c.tick(t,false);}
  return c.s.i==integral+1.0f; // one normal 1s PID step, no catch-up
}
constexpr bool jitterTriplet() {
  auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);
  for(unsigned n=1;n<=3;++n){c.mainAt=100+n*251;c.sample(NAN,c.mainAt);c.tick(c.mainAt,false);}
  c.mainAt=1104;c.sample(80,1104);c.tick(1104,false);
  return !c.sensorPaused && c.s.fault==Fault::None && c.s.mode==Mode::Pid;
}
constexpr bool exactHardBoundary() {
  auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);
  keepMain(c,5100);
  if(!c.sensorPaused || c.s.fault!=Fault::None)return false;
  auto late=c;late.mainAt=5101;late.sample(80,5101);
  if(late.s.fault!=Fault::Stale || late.s.relay)return false;
  c.sample(80,5100);
  return !c.sensorPaused && c.s.fault==Fault::None && c.s.mode==Mode::Pid;
}
constexpr bool recoveryCannotBypassFaults() {
  auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);
  keepMain(c,1601);auto hot=c,stalled=c;
  hot.mainAt=2100;hot.sample(126,2100);
  if(hot.s.fault!=Fault::Overtemp || hot.s.power!=0 || hot.s.relay)return false;
  stalled.sample(80,2602); // incoming sensor read must not hide dead Main
  if(stalled.s.fault!=Fault::MainStalled || stalled.s.power!=0)return false;
  auto density=prepared();r.op=Op::Enable;density.command(r,100,true);
  for(unsigned n=1;n<=18;++n){density.mainAt=100+n*250;density.sample(NAN,density.mainAt);density.tick(density.mainAt,false);}
  if(density.s.fault!=Fault::Sensor || density.s.relay)return false;
  density.mainAt+=250;density.sample(80,density.mainAt);
  return density.s.fault==Fault::Sensor && density.s.mode==Mode::Fault;
}
constexpr bool stopCancelsRecovery() {
  auto c=prepared();Request r;r.op=Op::Enable;c.command(r,100,true);keepMain(c,1601);
  r.op=Op::Off;c.command(r,1601,false);keepMain(c,2100);c.sample(80,2100);c.tick(2100,false);
  return c.s.mode==Mode::Off && !c.sensorPaused && !c.s.relay && c.s.power==0;
}
constexpr bool pauseKeepsDeadlines() {
  auto t=prepared();Request r;r.op=Op::Test;r.testSeconds=2;t.command(r,100,true);
  keepMain(t,1601);if(!t.sensorPaused)return false;
  keepMain(t,2100);t.mainAt=2200;t.sample(80,2200);t.tick(2200,false);
  if(t.s.mode!=Mode::Off || t.s.relay)return false;
  auto c=prepared();c.s.config.idleMinutes=1;r.op=Op::Enable;c.command(r,100,true);
  advance(c,58100);keepMain(c,60100);c.mainAt=60350;c.sample(80,60350);c.tick(60350,false);
  return c.s.mode==Mode::Off && c.s.idleExpired && !c.s.relay;
}
static_assert(pauseAndResume(),"pause freezes I and output; good at 2s recalculates without integration or derivative catch-up");
static_assert(jitterTriplet(),"three faulty reads plus scheduling jitter no longer trip at 1s");
static_assert(exactHardBoundary(),"5s boundary and late good sample preserve latched shutdown");
static_assert(recoveryCannotBypassFaults(),"no recovery past overtemp, dead Main or 18/20 sensor latch");
static_assert(stopCancelsRecovery(),"explicit OFF/inhibit cancels automatic sensor recovery");
static_assert(pauseKeepsDeadlines(),"sensor pause never extends TEST or idle deadlines");
