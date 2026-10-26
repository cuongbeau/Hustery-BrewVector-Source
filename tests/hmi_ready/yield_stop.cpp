#include "src/control/yield_stop_policy.h"
#include "src/app/quick_yield_session.h"
using namespace YieldStopPolicy;

constexpr bool replay490() {
  QuickYieldSession s;
  s.command(0,1,36,0);s.rotate(-10);s.command(2,1,36,1);s.consume(true);
  // Custom pressure profile, still phase 0: neither Manual nor legacy mode.
  const float limit=threshold(true,false,s.running,0);
  if(reached(limit,216.121f,true))return false; // Pre-tare cup reading.
  if(reached(limit,25.9886f,false))return false; // CSV at 4.96 s.
  if(!reached(limit,27.2048f,false))return false; // CSV at 5.04 s.
  s.consume(true);
  return !reached(threshold(true,false,s.running,0),38.5282f,false);
}
static_assert(replay490(),"Applied 26 g stops custom profile in first phase, once");
static_assert(reached(threshold(true,true,26,36),26,false),"Manual uses exact quick grams");
static_assert(!reached(threshold(true,false,26,0),NAN,false),"Invalid/lost scale cannot stop");
static_assert(!reached(threshold(true,true,0,36),100,false),"Explicit OFF overrides legacy target");
static_assert(threshold(true,false,40,36)==40,"Higher override cannot use old threshold");
static_assert(threshold(true,true,NAN,36)==34,"Unchanged Manual retains 2 g offset");
static_assert(threshold(false,true,26,36)==34,"Hot water retains its own legacy target");
static_assert(!reached(threshold(true,false,NAN,36),100,false),"No override leaves custom exits to executor");
