#include "src/control/heater_core.h"
#include "src/control/heater_flow_model.h"
#include "protocol/heater_legacy_wire.h"
#include <stddef.h>
using namespace HeaterControl;
constexpr bool near(float a,float b) {return std::abs(a-b)<0.001f;}
constexpr bool equations() {
  HeaterControlModel::Pid p;
  p.step(100,90,2,.1f,3,4,100);
  if(!near(p.p,20)||!near(p.i,1)||!near(p.d,0)||!near(p.output,25))return false;
  p.step(100,92,2,.1f,3,4,100,false,2);
  return near(p.p,16)&&near(p.i,1.8f)&&near(p.d,-6)&&near(p.output,15.8f);
}
constexpr bool antiwindupIncludesFF() {
  HeaterControlModel::Pid p;p.step(100,90,2,1,0,15,30);
  if(p.integral!=0||p.output!=30)return false;
  p.step(100,90,2,1,0,0,30); // exact limit permits integration
  if(p.integral!=10||p.output!=30)return false;
  p.step(100,99,0,1,0,0,100);return p.i==11; // no invented 5% cap
}
constexpr bool negativeIntegralAndRecovery() {
  HeaterControlModel::Pid p;p.integral=20;
  p.step(100,101,0,1,0,0,100);
  if(p.integral!=19)return false;
  p.step(100,110,0,1,0,0,100);
  if(p.integral!=9)return false;
  p.step(100,95,2,1,4,0,100,true);
  return p.integral==9&&p.d==0&&p.output==19;
}
constexpr bool thermalFF() {
  HeaterControlModel::Feedforward f;
  if(f.update(100,90,2,true,true)!=0)return false;
  f.kff=.1f;f.inletC=20;f.efficiency=1;f.heatLossW=0;
  if(!near(f.update(100,90,2,true,true),66.88f))return false;
  if(f.update(100,90,2,false,true)!=0||f.update(100,90,2,true,false)!=0)return false;
  if(f.update(100,90,NAN,true,true)!=0)return false;
  f.efficiency=0;if(f.update(100,90,2,true,true)!=0)return false;
  f.efficiency=1;f.safety=1;
  if(!near(HeaterControlModel::Feedforward::scaling(2),1) ||
     !near(HeaterControlModel::Feedforward::scaling(3),2.0f/3) ||
     !near(HeaterControlModel::Feedforward::scaling(4),1.0f/3) ||
     HeaterControlModel::Feedforward::scaling(5)!=0)return false;
  if(!near(f.update(100,102,2,true,true),66.88f))return false;
  if(!near(f.update(100,103,2,true,true),66.88f*2/3))return false;
  return f.update(100,105,2,true,true)==0 && f.safety==0;
}
constexpr bool cadenceAndStop() {
  Core c;c.mainAt=100;c.sample(90,100);c.s.config.ceiling=125;
  Request r;r.op=Op::Enable;c.command(r,100,true);
  for(unsigned n=1;n<=3;++n){c.mainAt=100+n*250;c.sample(90,c.mainAt);c.tick(c.mainAt,false);if(c.s.power!=0)return false;}
  c.mainAt=1100;c.sample(90,1100);c.tick(1100,false);if(c.s.power!=30)return false;
  c.sample(126,1200);return c.s.fault==Fault::Overtemp&&!c.s.relay&&c.s.power==0;
}
constexpr bool integratedFeedforwardGuards() {
  Core c;c.mainAt=100;c.sample(90,100);c.s.config.ceiling=125;
  c.s.config.kp=c.s.config.ki=c.s.config.kd=0;
  c.s.config.ffEnabled=1;c.s.config.heaterW=1000;c.s.config.ffScale=1;
  Request r;r.op=Op::Enable;c.command(r,100,true);
  for(unsigned t=350;t<=1100;t+=250){
    c.mainAt=t;c.sample(90,t);c.flowInput(2,true,true,t);c.tick(t,true);
  }
  if(c.s.power!=30 || c.feedforward.output<=30)return false;
  c.flowInput(0,false,false,1110);c.mainAt=1110;c.tick(1110,false);
  if(c.s.power!=0 || c.s.relay || c.feedforward.output!=0)return false;
  c.flowInput(2,true,true,1200);
  for(unsigned t=1350;t<=2100;t+=250){c.mainAt=t;c.sample(90,t);c.tick(t,true);}
  if(c.s.power!=30)return false;
  c.mainAt=2350;c.sample(90,2350);c.tick(2350,true);
  if(c.feedforward.output!=0)return false; // stale inlet, no new FF
  r.op=Op::Off;c.command(r,2350,false);c.tick(2350,false);
  if(c.s.power!=0||c.s.relay||c.feedforward.output!=0)return false;
  r.op=Op::Test;r.testPower=10;c.command(r,2350,true);
  c.flowInput(2,true,true,2350);c.tick(2350,false);
  return c.s.power==10&&c.feedforward.output==0;
}
constexpr bool derivativeSamples() {
  HeaterControlModel::TemperatureRate r;
  r.sample(50,100); if(r.rate!=0)return false;
  r.sample(52,1100); if(!near(r.rate,2.0f/3))return false;
  r.sample(NAN,1200); if(r.at!=1100)return false;
  r.sample(60,1100); if(r.previous!=52)return false;
  r.sample(54,2100); if(!near(r.rate,10.0f/9))return false;
  r.sample(90,4000,true); if(r.rate!=0)return false;
  HeaterControlModel::Pid p;
  p.step(115,90,2,.02f,5,0,100,false,r.rate);
  p.step(120,90,2,.02f,5,0,100,false,r.rate);
  return p.d==0;
}
constexpr bool derivativeLifecycle() {
  Core c;c.mainAt=100;c.sample(50,100);c.s.config.ceiling=125;c.s.config.kd=5;
  Request r;r.op=Op::Enable;c.command(r,100,true);
  for(unsigned t=350;t<=1100;t+=250){c.mainAt=t;c.sample(50,t);c.tick(t,false);}
  if(c.s.d!=0 || c.s.power!=30)return false;
  c.mainAt=1350;c.sample(51,1350);const float rate=c.temperatureRate.rate;
  c.sample(NAN,1400);if(c.temperatureRate.rate!=rate||c.temperatureRate.at!=1350)return false;
  c.mainAt=3000;c.tick(3000,false);if(!c.sensorPaused)return false;
  const float integral=c.pid.integral;
  c.sample(60,3010);
  if(c.sensorPaused||c.s.d!=0||c.temperatureRate.rate!=0||c.pid.integral!=integral)return false;
  r.op=Op::Off;c.command(r,3010,false);
  return !c.temperatureRate.initialized&&c.s.power==0;
}
constexpr bool derivativeNoiseAndWrap() {
  HeaterControlModel::TemperatureRate r;r.sample(115,0);
  // Alternating +/-1C at the sensor cadence must not become +/-40% D.
  for(unsigned t=250;t<=10000;t+=250){
    r.sample(t%500 ? 116.0f : 114.0f,t);
    if(std::abs(5*r.rate)>5)return false;
  }
  r.reset();r.sample(50,UINT32_MAX-99);r.sample(51,150);
  return near(r.rate,1.0f/2.25f);
}
static_assert(equations(),"1Hz P/I with local derivative on measurement");
static_assert(derivativeSamples(),"filtered rate, invalid/duplicate samples, restart and setpoint kick");
static_assert(derivativeLifecycle(),"accepted samples only, startup, pause recovery and OFF");
static_assert(derivativeNoiseAndWrap(),"sensor noise attenuation and millis wrap");
static_assert(offsetof(Config,heaterW)==31,"v2 migration prefix must preserve all existing config fields");
static_assert(near(HeaterFlowModel::estimate(100,0),7.8f),"legacy measured free flow");
static_assert(near(HeaterFlowModel::estimate(50,0),4.9f),"legacy low-CPS correction");
static_assert(near(HeaterFlowModel::estimate(100,9),1.221491f),"legacy pressure curve");
static_assert(HeaterFlowModel::estimate(0,0)==0 && HeaterFlowModel::estimate(100,9.8f)==0 &&
              HeaterFlowModel::estimate(NAN,0)==0 && HeaterFlowModel::estimate(100,NAN)==0,
              "stopped, out-of-model and invalid inputs do not request flow compensation");
constexpr bool configBounds() {
  Config c;c.ceiling=125;if(!validConfig(c)||c.ffEnabled)return false;
  c.ffEnabled=1;c.heaterW=0;if(validConfig(c))return false;
  c.heaterW=1300;c.ffScale=1.01f;if(validConfig(c))return false;
  c.ffScale=.25f;c.inletC=NAN;return !validConfig(c);
}
static_assert(configBounds(),"FF default off and finite bounded configuration");
static_assert(HeaterLegacyWire::accepts(2,Op::Off) &&
              !HeaterLegacyWire::accepts(3,Op::Off) &&
              !HeaterLegacyWire::accepts(2,Op::Configure) &&
              !HeaterLegacyWire::accepts(2,Op::Enable) &&
              !HeaterLegacyWire::accepts(2,Op::Test) &&
              !HeaterLegacyWire::accepts(2,Op::ClearFault),
              "old HMI can only stop, never arm or configure the new heater");
static_assert(antiwindupIncludesFF(),"upstream rollback uses sum including FF, no separate I cap");
static_assert(negativeIntegralAndRecovery(),"integrator unwinds; local pause recovery suppresses catch-up/kick");
static_assert(thermalFF(),"thermal power, Kff default off, gates and thermoblock safety scaling");
static_assert(cadenceAndStop(),"1Hz PID, immediate raw overtemp independent of PID scheduling");
static_assert(integratedFeedforwardGuards(),"FF obeys total power ceiling, inlet freshness, OFF and TEST");

constexpr bool thermoblockWaterOnset() {
  Core c;c.mainAt=100;c.sample(118,100);
  c.s.config.ceiling=125;c.s.config.target=115;
  c.s.config.kp=2;c.s.config.ki=c.s.config.kd=0;
  c.s.config.maxPower=70;c.s.config.waterMaxPower=100;
  c.s.config.ffEnabled=1;c.s.config.ffScale=1;
  c.s.config.heaterW=1300;c.s.config.inletC=25;
  Request r;r.op=Op::Enable;
  if(c.command(r,100,true)!=Result::Ok)return false;
  c.tick(100,false);
  if(c.s.power!=0 || c.pid.integral!=0)return false;
  c.mainAt=150;c.flowInput(5.4f,true,true,150);c.tick(150,true);
  if(c.s.power!=100 || !c.s.relay || c.windowAt!=150 || c.pid.integral!=0 ||
     c.s.activeMaxPower!=100 || c.pidAt!=100)return false;
  c.tick(160,true);
  if(c.windowAt!=150)return false; // Do not restart PWM on every heater tick.
  c.mainAt=400;c.sample(119,400);c.tick(400,true);
  if(c.s.power<=0 || c.s.power>=100)return false;
  c.mainAt=650;c.sample(120,650);c.tick(650,true);
  if(c.feedforward.output!=0 || c.s.power!=0 || c.s.relay)return false;
  c.mainAt=700;c.flowInput(5.4f,true,true,700);c.tick(700,false);
  return c.feedforward.output==0 && c.s.power==0 &&
      c.s.activeMaxPower==70 && !c.s.relay;
}
static_assert(thermoblockWaterOnset(),"warm thermoblock responds immediately to water, tapers by 120C and withdraws on stop");

constexpr bool thermoblockRecoveryAfterStop() {
  Core c;c.mainAt=100;c.sample(105,100);
  c.s.config.target=115;c.s.config.ceiling=125;
  c.s.config.kp=2;c.s.config.ki=c.s.config.kd=0;
  c.s.config.maxPower=70;c.s.config.waterMaxPower=100;
  c.s.config.ffEnabled=1;c.s.config.ffScale=1;
  c.s.config.heaterW=1300;c.s.config.inletC=25;
  Request r;r.op=Op::Enable;
  if(c.command(r,100,true)!=Result::Ok)return false;
  c.mainAt=150;c.flowInput(2.5f,true,true,150);c.tick(150,true);
  if(c.s.power<76 || c.s.power>78)return false; // Immediate flow FF; PID has not run.
  c.mainAt=1100;c.sample(105,1100);c.flowInput(2.5f,true,true,1100);c.tick(1100,true);
  if(!near(c.s.p,20) || c.s.power<96 || c.s.power>98)return false;
  c.mainAt=1110;c.flowInput(0,false,false,1110);c.tick(1110,false);
  return c.feedforward.output==0 && near(c.s.power,20) &&
      c.s.activeMaxPower==70 && c.pidAt==1100;
}
static_assert(thermoblockRecoveryAfterStop(),"PID retains only its body-temperature recovery demand after flow stops");

constexpr bool waterPowerCaps() {
  Core c;c.s.config.ceiling=125;c.s.config.ffEnabled=1;
  c.s.config.waterMaxPower=60;c.mainAt=100;c.sample(90,100);
  Request r;r.op=Op::Enable;if(c.command(r,100,true)!=Result::Ok)return false;
  for(unsigned t=350;t<=1100;t+=250){
    c.mainAt=t;c.sample(90,t);c.flowInput(3.18f,true,true,t);c.tick(t,true);
  }
  if(c.s.power!=60 || c.s.activeMaxPower!=60)return false;
  const float integral=c.pid.integral;
  auto stopped=c;stopped.flowInput(0,false,false,1110);stopped.tick(1110,false);
  if(stopped.s.power>30 || stopped.s.activeMaxPower!=30 ||
     stopped.feedforward.output!=0 || stopped.pid.integral!=integral)return false;
  auto invalid=c;invalid.flowInput(3.18f,true,false,1110);invalid.tick(1110,true);
  if(invalid.s.power>30 || invalid.s.activeMaxPower!=30)return false;
  auto closed=c;closed.flowInput(3.18f,false,true,1110);closed.tick(1110,true);
  if(closed.s.power>30 || closed.s.activeMaxPower!=30)return false;
  auto stale=c;stale.mainAt=2101;stale.sample(90,2101);stale.tick(2101,true);
  if(stale.s.activeMaxPower!=30 || stale.s.power>30)return false;
  auto disabled=c;disabled.s.config.ffEnabled=0;disabled.tick(1110,true);
  if(disabled.s.activeMaxPower!=30 || disabled.s.power>30)return false;
  auto noScale=c;noScale.s.config.ffScale=0;noScale.tick(1110,true);
  if(noScale.s.activeMaxPower!=30 || noScale.s.power>30)return false;
  auto pause=c;pause.mainAt=2601;pause.flowInput(3,true,true,2601);pause.tick(2601,true);
  if(!pause.sensorPaused || pause.s.power!=0 || pause.s.activeMaxPower!=30)return false;
  auto hot=c;hot.mainAt=1110;hot.sample(125,1110);hot.tick(1110,true);
  if(hot.s.fault!=Fault::Overtemp || hot.s.power!=0)return false;
  // Withdraw a high cap even when FF already decayed to zero.
  c.feedforward.output=0;c.tick(1110,false);
  if(c.s.power>30 || c.s.activeMaxPower!=30)return false;
  r.op=Op::Off;c.command(r,1110,false);
  r.op=Op::Test;r.testPower=40;
  return c.command(r,1110,true)==Result::Invalid;
}
static_assert(waterPowerCaps(),"water cap, immediate fallback, stale/invalid/closed/off/pause/overtemp and TEST limits");
constexpr bool waterCapBounds() {
  Config c;c.ceiling=125;c.waterMaxPower=100;if(!validConfig(c))return false;
  c.waterMaxPower=101;if(validConfig(c))return false;
  c.waterMaxPower=0;if(validConfig(c))return false;
  c.waterMaxPower=NAN;return !validConfig(c);
}
static_assert(waterCapBounds(),"finite commissioned water cap 0..100");
