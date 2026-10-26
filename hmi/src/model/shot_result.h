#pragma once
#include "model/shot_record.h"
#include "hmi_protocol.h"
#include "model/stop_result_values.h"
#include <stdio.h>
#if __cplusplus >= 201402L
#define RESULT_CONSTEXPR constexpr
#else
#define RESULT_CONSTEXPR
#endif

// Frozen result independent of SD/autosave and of live BLE after stopping.
class ShotResultSnapshot {
 public:
  RESULT_CONSTEXPR void clear() {
    session_ = 0;
    available = false;
    summary = ShotSummary{};
    summary.weightG = NAN;
  }
  RESULT_CONSTEXPR void observe(const HmiProtocol::ControlTracePayload& trace, const char* profile) {
    if (!trace.shotSessionId) return;
    if (trace.shotSessionId != session_) {
      session_ = trace.shotSessionId;
      summary = ShotSummary{};
      summary.weightG = NAN;
      const char* name = profile ? profile : "BREW";
      for (size_t i = 0; i + 1 < sizeof(summary.profileName) && name[i]; ++i)
        summary.profileName[i] = name[i];
      available = true;
    }
    if (summary.resultFlags & 1U) return;
    summary.durationS = trace.elapsedS;
    if (__builtin_isfinite(trace.pressureFilteredBar) && trace.pressureFilteredBar > summary.peakPressureBar)
      summary.peakPressureBar = trace.pressureFilteredBar;
    if (summary.sampleCount < UINT16_MAX) ++summary.sampleCount;
  }
  RESULT_CONSTEXPR bool finish(const HmiProtocol::ShotEndPayload& end) {
    if (!available || end.shotSessionId != session_ || (summary.resultFlags & 1U)) return false;
    summary.durationS = end.elapsedS;
    const bool online = (end.flags & HmiProtocol::ShotEndScaleOnline) != 0;
    summary.resultFlags = StopResultValues::flags(online, end.scaleWeightG);
    summary.weightG = StopResultValues::yield(online, end.scaleWeightG);
    summary.modelYieldG = end.modelYieldG;
    return true;
  }
  bool available = false;
  ShotSummary summary{};
 private:
  uint32_t session_ = 0;
};
#undef RESULT_CONSTEXPR
