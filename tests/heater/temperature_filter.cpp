#include "src/control/heater_core.h"
#include "protocol/heater_sensor_wire.h"
using namespace HeaterControl;

constexpr Core warm(uint32_t at=100) {
  Core c; c.s.config.ceiling=125; c.s.config.stableSeconds=0;
  c.mainAt=at; c.sample(110,at);
  Request r; r.op=Op::Enable; c.command(r,at,true);
  return c;
}
constexpr void reading(Core& c, float value, uint32_t at) {
  c.mainAt=at; c.sample(value,at); c.tick(at,false);
}
constexpr bool isolatedSpike() {
  auto c=warm(); c.pid.integral=3; c.s.i=c.s.config.ki*3;
  const float integral=c.pid.integral;
  reading(c,126,350);
  if(!c.overtempPending || c.s.fault!=Fault::None || c.s.relay || c.s.power ||
      c.s.ready || c.s.raw!=126 || c.s.filtered<=110 || c.s.filtered>=126)return false;
  reading(c,110,600); reading(c,110,850);
  if(!c.overtempPending || c.s.relay || c.s.power || c.pid.integral!=integral)return false;
  reading(c,110,1100);
  return !c.overtempPending && c.s.mode==Mode::Pid && c.s.fault==Fault::None &&
      c.overtempEvents==1 && c.overtempAt==350 && c.overtempPeak==126 &&
      c.pid.integral==integral && c.s.d==0;
}
constexpr bool sustainedHeat() {
  auto c=warm(); reading(c,125,350); reading(c,126,600);
  if(!c.overtempPending || c.s.fault!=Fault::None || c.s.power)return false;
  reading(c,127,850);
  if(c.s.fault!=Fault::Overtemp || c.s.mode!=Mode::Fault || c.s.relay || c.s.power)return false;
  for(uint32_t t=1100;t<=3100;t+=250)reading(c,110,t);
  return c.s.fault==Fault::Overtemp && c.s.mode==Mode::Fault && !c.overtempPending;
}
constexpr bool ticksDoNotConfirm() {
  auto c=warm(); reading(c,126,350);
  for(uint32_t t=360;t<=1300;t+=10){c.mainAt=t;c.tick(t,false);}
  return c.overtempPending && c.hotSamples==1 && c.s.fault==Fault::None && !c.s.relay && !c.s.power;
}
constexpr bool invalidBreaksConfirmation() {
  auto c=warm(); reading(c,126,350); reading(c,NAN,600); reading(c,126,850);
  reading(c,126,1100);
  if(c.s.fault!=Fault::None || !c.overtempPending)return false;
  reading(c,126,1350);
  return c.s.fault==Fault::Overtemp && !c.s.relay;
}
constexpr bool coolHysteresis() {
  auto c=warm(); reading(c,126,350); reading(c,110,600); reading(c,123,850);
  reading(c,110,1100); reading(c,NAN,1350); reading(c,110,1600); reading(c,110,1850);
  if(!c.overtempPending || c.s.power || c.s.fault!=Fault::None)return false;
  reading(c,110,2100);
  return !c.overtempPending && c.s.mode==Mode::Pid;
}
constexpr bool waitForFilteredCooling() {
  auto c=warm(); reading(c,126,350); c.s.filtered=130;
  reading(c,122,600); reading(c,122,850); reading(c,122,1100);
  return c.overtempPending && c.s.filtered>122 && !c.s.relay;
}
constexpr bool noRecoveryPastGuards() {
  auto c=warm(); reading(c,126,350);
  auto stale=c, stalled=c, density=c;
  for(uint32_t t=600;t<=5600;t+=250){stale.mainAt=t;stale.tick(t,false);}
  if(stale.s.fault!=Fault::Stale || stale.s.relay)return false;
  stalled.tick(1351,false);
  if(stalled.s.fault!=Fault::MainStalled || stalled.s.relay)return false;
  for(uint32_t n=1;n<=18;++n)reading(density,NAN,350+n*250);
  return density.s.fault==Fault::Sensor && !density.s.relay;
}
constexpr bool explicitOff() {
  auto c=warm(); reading(c,126,350);
  Request r; r.op=Op::Enable;
  if(c.command(r,350,true)!=Result::Unsafe)return false;
  r.op=Op::Off; c.command(r,350,false);
  reading(c,110,600); reading(c,110,850); reading(c,110,1100);
  return c.s.mode==Mode::Off && !c.s.relay && !c.s.power && !c.overtempPending;
}
constexpr bool originalFilterAndRestart() {
  auto c=warm(); reading(c,80,350);
  // The first changed valid reading must affect feedback immediately.
  if(c.s.filtered<96.36f || c.s.filtered>96.37f)return false;
  const float held=c.s.filtered; reading(c,NAN,600);
  if(c.s.filtered!=held || c.s.valid)return false;
  reading(c,80,850);
  if(c.s.filtered<86.13f || c.s.filtered>86.14f)return false;
  for(uint32_t t=1100;t<=2600;t+=250){c.mainAt=t;c.tick(t,false);}
  reading(c,90,2600);
  return c.s.filtered==90;
}
constexpr bool emergency() {
  auto c=warm();reading(c,135,350);
  if(c.s.fault!=Fault::Overtemp || c.s.relay)return false;
  Core unconfigured;unconfigured.sample(150,100);
  return unconfigured.s.fault==Fault::Overtemp;
}
constexpr bool wrapConfirmation() {
  const uint32_t start=UINT32_MAX-400;auto c=warm(start);
  reading(c,126,start+250);reading(c,126,start+500);reading(c,126,start+750);
  return c.s.fault==Fault::Overtemp && !c.s.relay;
}
constexpr bool missedReadBreaksChain() {
  auto c=warm();reading(c,126,350);reading(c,126,600);reading(c,126,1350);
  return c.s.fault==Fault::None && c.hotSamples==1 && c.overtempPending && !c.s.relay;
}
constexpr bool pendingHonorsDeadlines() {
  auto test=warm();Request r;r.op=Op::Off;test.command(r,100,true);
  r.op=Op::Test;r.testSeconds=1;test.command(r,100,true);
  reading(test,126,350);reading(test,NAN,600);reading(test,NAN,850);
  test.mainAt=1100;test.tick(1100,false);
  if(test.s.mode!=Mode::Off || test.s.power || test.s.relay)return false;
  auto idle=warm();idle.s.config.idleMinutes=1;
  reading(idle,126,350);
  // Refresh the sensor at safe-but-not-recovered 123C, keeping output inhibited.
  for(uint32_t t=600;t<=60100;t+=250)reading(idle,123,t);
  return idle.s.mode==Mode::Off && idle.s.idleExpired && !idle.s.relay;
}
static_assert(isolatedSpike(), "single spike holds output and I; stable cool recovery has no catch-up");
static_assert(sustainedHeat(), "three high readings over 500ms latch regardless of filtered value");
static_assert(ticksDoNotConfirm(), "held high value cannot be counted as fresh sensor samples");
static_assert(invalidBreaksConfirmation(), "invalid reads never count as high confirmation");
static_assert(coolHysteresis(), "warm or invalid samples restart the cooling interval");
static_assert(waitForFilteredCooling(), "raw cool alone cannot resume while filtered remains hot");
static_assert(noRecoveryPastGuards(), "pending heat never bypasses stale, heartbeat or density guards");
static_assert(explicitOff(), "OFF cancels automatic recovery and pending cannot be enabled");
static_assert(originalFilterAndRestart(), "original low-pass responds on first valid change, holds invalids and resets on long gap");
static_assert(emergency(), "extreme temperature still latches immediately");
static_assert(wrapConfirmation(), "heat confirmation survives millis wrap");
static_assert(missedReadBreaksChain(), "long intersample gaps do not confirm a continuous hot interval");
static_assert(pendingHonorsDeadlines(), "pending heat does not extend Test or automatic sleep");
static_assert(HeaterSensorWire::V8_SIZE + 19 == sizeof(HeaterSensorWire::Status), "v9 diagnostic extension is append only");
