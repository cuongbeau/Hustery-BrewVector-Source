#include "src/hardware/adc_probe_engine.h"
using namespace AdcProbe;
struct FakeBus {
  uint32_t us = 0, due = 0, writes = 0, publishes = 0;
  uint16_t config = 0xc3e0, raw = 0x1237;
  bool fail = false, stuck = false, corrupt = false, fastChip = false;
  constexpr uint32_t now() { return us; }
  constexpr bool write(uint8_t reg, uint16_t v) {
    if (fail || reg != 1) return false;
    config = v; ++writes;
    const uint8_t code = (v >> 5) & 7;
    const uint32_t duration = code == 0 ? 125000 : code == 2 ? 31250 : code == 4 ? 7813 : 1163;
    due = us + (fastChip ? duration / 16 : duration);
    return true;
  }
  constexpr bool read(uint8_t reg, uint16_t& v) {
    if (fail) return false;
    if (reg == 0) { v = raw; return true; }
    const bool ready = !stuck && static_cast<int32_t>(us - due) >= 0;
    v = (config & 0x7fff) | (ready ? 0x8000 : 0);
    if (corrupt) v ^= 0x20;
    return true;
  }
  constexpr void publish(int16_t r) { if (r == static_cast<int16_t>(raw)) ++publishes; }
};
constexpr bool complete(bool fast, bool continuous, uint32_t start) {
  FakeBus bus; bus.us = start; bus.due = start;
  if (continuous) bus.config = 0xc2a0;
  bus.fastChip = fast;
  const uint16_t original = bus.config;
  Engine e; e.begin(7, 0, start);
  // Pending never touches ADC until the pump owner acknowledges hold.
  e.tick(bus,bus.us,true,false,false);
  if(bus.writes || !e.report.pumpHeld) return false;
  for(int i=0;i<3000 && Engine::active(e.report.state);++i){
    bus.us += 1000; e.tick(bus,bus.us,true,false,true);
  }
  if(e.report.state != State::Complete || e.report.pumpHeld || e.report.count != 24 || bus.publishes != 1) return false;
  if((bus.config & 0x7fff) != (original & 0x7fff)) return false;
  for(uint8_t i=0;i<24;++i) {
    const auto& s=e.report.samples[i];
    if(s.rateCode != Engine::rate(i) || s.lowNibble != 7 || s.raw != 0x1237 || s.lastBusyUs >= s.readyUs) return false;
  }
  return fast ? e.report.samples[0].readyUs < 20000 : e.report.samples[0].readyUs >= 125000;
}
constexpr bool interruption(bool cancellation) {
  FakeBus bus; Engine e; e.begin(1,0,0);
  for(int i=0;i<15;++i){bus.us+=1000;e.tick(bus,bus.us,true,false,true);}
  const auto count=e.report.count;
  bus.us+=1000;e.tick(bus,bus.us,!cancellation ? false : true,cancellation,true);
  if(!e.report.pumpHeld || e.report.state!=State::Restoring || bus.publishes) return false;
  for(int i=0;i<300;++i){bus.us+=1000;e.tick(bus,bus.us,false,false,true);}
  return e.report.state==State::Cancelled && e.report.count==count && bus.publishes==1 && !e.report.pumpHeld;
}
constexpr bool restoreFailure() {
  FakeBus bus; Engine e; e.begin(1,0,0);
  bus.us=1000;e.tick(bus,bus.us,true,false,true); // saved config, conversion started
  bus.fail=true; bus.us+=1000;e.tick(bus,bus.us,true,true,true);
  if(e.report.state!=State::RestoreFailed || !e.report.pumpHeld || bus.publishes) return false;
  bus.fail=false;
  bus.us+=1000;e.tick(bus,bus.us,true,true,true); // Explicit retry
  for(int i=0;i<300;++i){bus.us+=1000;e.tick(bus,bus.us,true,false,true);}
  return !e.report.pumpHeld && e.report.state==State::Cancelled && bus.publishes==1;
}
constexpr bool ackFailure() {
  FakeBus bus; Engine e;e.begin(1,0,0);
  for(int i=0;i<300;++i){bus.us+=1000;e.tick(bus,bus.us,true,false,false);}
  return e.report.state==State::Failed && e.report.error==Error::PumpAck && !e.report.pumpHeld && bus.writes==0;
}
static_assert(complete(false,false,0), "ADS1115 timing, untouched raw bits, exact restore");
static_assert(complete(true,false,0), "ADS1015-like timing");
static_assert(complete(false,true,0xfffffff0U), "Continuous restore and micros wrap");
static_assert(interruption(true) && interruption(false), "Cancel/start brew waits for restore and fresh sample");
static_assert(restoreFailure(), "Failed restore retains pump lock; explicit retry recovers");
static_assert(ackFailure(), "No ADC writes without owner hold acknowledgement");
constexpr bool stuckAndMismatch() {
  FakeBus bus; Engine e; e.begin(1,0,0);
  bus.us=1000;e.tick(bus,bus.us,true,false,true);
  bus.stuck=true;
  for(int i=0;i<260;++i){bus.us+=1000;e.tick(bus,bus.us,true,false,true);}
  if(e.report.state!=State::RestoreFailed || !e.report.pumpHeld || bus.publishes || e.report.error!=Error::Timeout) return false;
  bus=FakeBus{};e.begin(2,0,0);
  // Drain baseline, start first test conversion, then corrupt readback.
  for(int i=0;i<5;++i){bus.us+=1000;e.tick(bus,bus.us,true,false,true);}
  bus.corrupt=true;bus.us+=1000;e.tick(bus,bus.us,true,false,true);
  if(e.report.error!=Error::Config || !e.report.pumpHeld) return false;
  bus.corrupt=false;
  for(int i=0;i<300;++i){bus.us+=1000;e.tick(bus,bus.us,true,false,true);}
  return e.report.state==State::Failed && !e.report.pumpHeld && bus.publishes==1;
}
static_assert(stuckAndMismatch(), "Conversion timeout holds pump, config mismatch aborts and restores");
