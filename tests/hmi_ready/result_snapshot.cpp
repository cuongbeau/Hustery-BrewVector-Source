#include "model/shot_result.h"

constexpr HmiProtocol::ControlTracePayload trace(uint32_t session = 1) {
  HmiProtocol::ControlTracePayload t{};
  t.shotSessionId = session; t.elapsedS = 24; t.pressureFilteredBar = 9;
  t.outputWeightG = 88; t.scaleWeightG = 30;
  return t;
}
constexpr HmiProtocol::ShotEndPayload end(uint32_t session = 1) {
  HmiProtocol::ShotEndPayload e{};
  e.shotSessionId = session; e.elapsedS = 24.5f; e.scaleWeightG = 32.6f;
  e.outputWeightG = 88; e.modelYieldG = 90; e.flags = HmiProtocol::ShotEndScaleOnline;
  return e;
}
constexpr bool frozenResult() {
  ShotResultSnapshot r; r.observe(trace(), "Flat 9");
  if (hasStopYield(r.summary) || !r.finish(end())) return false;
  auto late = end(); late.scaleWeightG = 40;
  if (r.finish(late)) return false;
  r.observe(trace(), "Changed profile");
  return r.summary.weightG == 32.6f && r.summary.modelYieldG == 90 &&
      r.summary.durationS == 24.5f && r.summary.profileName[0] == 'F' &&
      hasStopYield(r.summary);
}
constexpr bool wrongSessionRejected() {
  ShotResultSnapshot r; r.observe(trace(2), "New shot");
  return !r.finish(end(1)) && !hasStopYield(r.summary);
}
constexpr bool missingScaleNeverUsesModel() {
  ShotResultSnapshot r; r.observe(trace(), "Flat 9");
  auto e = end(); e.flags = 0;
  return r.finish(e) && !hasStopYield(r.summary) &&
      hasEstimatedYield(r.summary) && r.summary.modelYieldG == 90 &&
      r.summary.resultFlags == 1;
}
constexpr bool invalidModelCannotBeDisplayed() {
  ShotResultSnapshot r; r.observe(trace(), "Flat 9");
  auto e = end(); e.flags = 0; e.modelYieldG = NAN;
  return r.finish(e) && !hasStopYield(r.summary) &&
      !hasEstimatedYield(r.summary);
}
constexpr bool nextShotClearsResult() {
  ShotResultSnapshot r; r.observe(trace(), "Old"); r.finish(end());
  r.observe(trace(2), "New");
  return !hasStopYield(r.summary) && r.summary.resultFlags == 0 &&
      r.summary.profileName[0] == 'N';
}
static_assert(frozenResult(), "Stop event is frozen; late scale/trace cannot overwrite");
static_assert(wrongSessionRejected(), "Stale end does not finish another shot");
static_assert(missingScaleNeverUsesModel(), "No scale: model estimate is available but scale yield stays invalid");
static_assert(invalidModelCannotBeDisplayed(), "Invalid model estimate is not displayed");
static_assert(nextShotClearsResult(), "New session clears old yield");
constexpr bool startWithoutTraceClearsResult() {
  ShotResultSnapshot r; r.observe(trace(), "Old"); r.finish(end());
  r.clear();
  return !r.available && !hasStopYield(r.summary) && !r.finish(end());
}
static_assert(startWithoutTraceClearsResult(), "A new run cannot display an old result if traces are missing");
