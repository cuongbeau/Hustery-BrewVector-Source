#include "protocol/scale_sample_tracker.h"

constexpr HmiProtocol::ScaleSamplePayload observation(uint32_t seq = 1) {
  HmiProtocol::ScaleSamplePayload s{};
  s.hmiBootId = 7; s.connectionGeneration = 2; s.tareGeneration = 3;
  s.sampleSequence = seq; s.sourceTimestampMs = seq * 50;
  s.weightG = 0; s.source = HmiProtocol::ScaleSource::BleCustom;
  s.flags = HmiProtocol::ScaleSampleConnected;
  return s;
}
constexpr bool repeatedDoesNotRenew() {
  ScaleSampleTracker t; auto s = observation();
  t.accept(s, 100); t.beginShot(true, 100);
  t.accept(s, 800); // A repeated UART frame, even if its age erroneously says zero.
  return t.allowed(850) && !t.allowed(851);
}
constexpr bool freshSameWeight() {
  ScaleSampleTracker t; t.accept(observation(), 100); t.beginShot(true, 100);
  t.accept(observation(2), 700);
  return t.allowed(1400); // Stationary weight is a valid new BLE observation.
}
constexpr bool changedBaseline(int kind) {
  ScaleSampleTracker t; t.accept(observation(), 100); t.beginShot(true, 100);
  auto s = observation(2);
  if (kind == 0) ++s.hmiBootId;
  if (kind == 1) ++s.connectionGeneration;
  if (kind == 2) ++s.tareGeneration;
  t.accept(s, 150);
  t.accept(observation(3), 200); // Returning to an old ID must not re-enable.
  return !t.allowed(200);
}
constexpr bool lossBetweenPolls() {
  ScaleSampleTracker t; t.accept(observation(), 100); t.beginShot(true, 100);
  auto s = observation(2); s.flags = 0; t.accept(s, 150);
  t.accept(observation(3), 160);
  return !t.allowed(160);
}
constexpr bool staleGapBetweenPolls() {
  ScaleSampleTracker t; t.accept(observation(), 100); t.beginShot(true, 100);
  t.accept(observation(2), 851); // No caller polled allowed() during the gap.
  return !t.allowed(851);
}
constexpr bool noScaleAtStart() {
  ScaleSampleTracker t; t.beginShot(false, 100); t.accept(observation(), 120);
  return t.online(120) && !t.allowed(120);
}
constexpr bool staleRemoteAge() {
  ScaleSampleTracker t; auto s = observation(); s.sampleAgeMs = 751;
  t.accept(s, 100); t.beginShot(true, 100);
  return !t.allowed(100);
}
constexpr bool reordered() {
  ScaleSampleTracker t; t.accept(observation(4), 100); t.beginShot(true, 100);
  t.accept(observation(3), 110);
  return t.latest.sampleSequence == 4 && !t.allowed(110);
}
constexpr bool wrap() {
  ScaleSampleTracker t; auto s = observation(0xffffffffU);
  t.accept(s, 0xfffffff0U); t.beginShot(true, 0xfffffff0U);
  t.accept(observation(1), 20);
  return t.allowed(30);
}
static_assert(repeatedDoesNotRenew(), "UART repetition must not extend freshness");
static_assert(freshSameWeight(), "Equal weight with new sequence is fresh");
static_assert(changedBaseline(0) && changedBaseline(1) && changedBaseline(2), "Boot/reconnect/tare locks yield");
static_assert(lossBetweenPolls() && staleGapBetweenPolls(), "Capture losses between control ticks");
static_assert(noScaleAtStart(), "A mid-shot connection is display-only");
static_assert(staleRemoteAge() && reordered(), "Reject stale or out-of-order observations");
static_assert(wrap(), "Clock and sequence wrap must preserve continuity");
static_assert(!ScaleTareTiming::secondDue(149, 100) && ScaleTareTiming::secondDue(150, 100), "50 ms tare spacing");
static_assert(!ScaleTareTiming::settled(349, 150) && ScaleTareTiming::settled(350, 150), "200 ms post-tare settle");
static_assert(!ScaleTareTiming::leaseValid(700, 700), "No tare at/after deadline");
static_assert(ScaleTareTiming::leaseValid(100, 0xfffffff0U), "Lease clock wrap");
static_assert(!ScaleTareTiming::leaseValid(1000, 100), "Reject impossible future lease");
static_assert(ScaleTareTiming::MainWaitMs > ScaleTareTiming::LeaseMs, "Fallback waits beyond possible writes");
