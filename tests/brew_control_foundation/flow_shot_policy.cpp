// Executed by the C++17 compiler via static_assert, no target hardware needed.
#include "src/control/flow_shot_policy.h"

constexpr bool shotIsolation() {
  FlowShotPolicy p;
  if (p.accepts(0)) return false;
  p.begin(8, true);
  if (p.accepts(7) || p.accepts(0) || !p.accepts(8)) return false;
  if (!p.accepts(8)) return false; // Repeated snapshot can be read, not integrated twice.
  p.begin(9, true);
  return !p.accepts(8) && p.accepts(9);
}
constexpr bool scaleLoss() {
  FlowShotPolicy p;
  p.begin(1, true);
  if (!p.allowScale(true)) return false;
  if (p.allowScale(false)) return false;
  if (p.allowScale(true)) return false; // Reconnect cannot prove baseline continuity.
  p.begin(2, false);
  if (p.allowScale(true)) return false; // First connect during shot is display-only.
  p.begin(3, true);
  return p.allowScale(true); // A new shot can use the scale again.
}
static_assert(shotIsolation(), "Stale owner totals must not enter a new shot");
static_assert(scaleLoss(), "Loss or mid-shot connect must not enable yield exits");
