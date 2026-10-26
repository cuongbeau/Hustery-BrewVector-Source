#include "hmi/src/ui/ready_layout.h"
#include "hmi/src/ui/brew_layout.h"
#include "hmi/src/ui/result_layout.h"
#include "hmi/src/model/stop_result_values.h"
using namespace ReadyLayout;
static_assert(StopResultValues::flags(true, 0.0f) == 3 &&
              StopResultValues::yield(true, 32.6f) == 32.6f,
              "A physical zero is valid and stop yield preserves the observed reading");
static_assert(StopResultValues::flags(false, 32.6f) == 1 &&
              StopResultValues::flags(true, NAN) == 1 &&
              StopResultValues::flags(true, INFINITY) == 1,
              "Offline/invalid scale never becomes a valid result");
static_assert(ResultLayout::rowAt(20, 39) == 0 && ResultLayout::rowAt(20, 172) == 3 &&
              ResultLayout::rowAt(20, 79) == -1 && ResultLayout::rowAt(20, 218) == -1,
              "History row gaps and pagination cannot select a shot");
static_assert(ResultLayout::History.contains(80, 215) &&
              !ResultLayout::Home.contains(80, 215) && ResultLayout::Home.contains(240, 215) &&
              !ResultLayout::Home.contains(240, 190), "Home and History use separate visible targets");

// Navigation and actuator targets on the 320x240 compact-menu ready screen.
static_assert(targetAt(100, 100) == Target::None, "Temperature area is not interactive");
static_assert(targetAt(80, 150) == Target::Profile, "Profile card is below the chart");
static_assert(targetAt(80, 210) == Target::Start, "Primary start has a dedicated touch target");
static_assert(targetAt(260, 150) == Target::Yield, "Yield editor cannot start pump");
static_assert(targetAt(215, 210) == Target::Flush, "Flush has its own target");
static_assert(targetAt(290, 210) == Target::Menu, "Menu is separate from Flush");
static_assert(Start.w == 160 && Flush.w == 88 && Menu.w == 44 &&
              Start.y == Flush.y && Flush.y == Menu.y &&
              Flush.x - (Start.x + Start.w) == 6 &&
              Menu.x - (Flush.x + Flush.w) == 6,
              "Primary, Flush and Menu share the 46-pixel action row");
static_assert(menuTargetAt(240, 118) == Target::History &&
              menuTargetAt(240, 158) == Target::Settings &&
              menuTargetAt(100, 118) == Target::None &&
              menuTargetAt(80, 210) == Target::None &&
              menuTargetAt(215, 210) == Target::None,
              "Open Menu never passes through to Profile, Brew or Flush");
static_assert(targetAt(171, 210) == Target::None &&
              targetAt(265, 210) == Target::None,
              "Gaps between the three actions have no target");
static_assert(targetAt(80, 182) == Target::None, "Gap between Profile and Start");
static_assert(targetAt(7, 140) == Target::None && targetAt(312, 140) == Target::None,
              "Margins cannot start pump");
static_assert(targetAt(-1, 150) == Target::None && targetAt(320, 224) == Target::None &&
              targetAt(288, 240) == Target::None, "Reject coordinates outside display");
static_assert(StopFlush.contains(160, 210) && !StopFlush.contains(23, 210) &&
              !StopFlush.contains(296, 210), "Stop flush matches visible button");
static_assert(enabledTarget(Target::Start, false, false, false) == Target::None &&
              enabledTarget(Target::Manual, false, false, false) == Target::None &&
              enabledTarget(Target::Flush, false, false, false) == Target::None,
              "Offline/pending controls never send actuator commands");
static_assert(enabledTarget(Target::History, false, false, false) == Target::History &&
              enabledTarget(Target::Settings, false, false, false) == Target::Settings,
              "Offline still permits history and recovery settings");
static_assert(enabledTarget(Target::Start, false, true, true) == Target::None &&
              enabledTarget(Target::Flush, false, true, true) == Target::Flush,
              "OFF profile blocks brew while cleaning remains available");
static_assert(heartbeatIntensity(0) == 0 && heartbeatIntensity(140) == 80 &&
              heartbeatIntensity(280) == 160 && heartbeatIntensity(420) == 80 &&
              heartbeatIntensity(560) == 0 && heartbeatIntensity(850) == 0 &&
              heartbeatIntensity(1090) == 100 && heartbeatIntensity(1210) == 50 &&
              heartbeatIntensity(1600) == 0 && heartbeatIntensity(2400) == 0,
              "Ready heartbeat eases through two separated pulses and a long rest");
static_assert(BrewLayout::Stop.contains(278,44) &&
              !BrewLayout::Stop.contains(243,44) && !BrewLayout::Stop.contains(312,44) &&
              !BrewLayout::Stop.contains(278,59) && !BrewLayout::Stop.contains(160,215),
              "Compact Stop hitbox excludes chart and former footer button");
static_assert(BrewLayout::Stop.y+BrewLayout::Stop.h < BrewLayout::Plot.y &&
              BrewLayout::Plot.y+BrewLayout::Plot.h < BrewLayout::FooterY &&
              BrewLayout::Plot.x+BrewLayout::Plot.w <= 320,
              "Stop, graph and metrics do not overlap");
static_assert(BrewLayout::normalized(75,true)==0 && BrewLayout::normalized(100,true)==1 &&
              BrewLayout::normalized(85,true)==BrewLayout::normalized(4,false),
              "Temperature75..100 aligns with shared pressure/flow0..10");
