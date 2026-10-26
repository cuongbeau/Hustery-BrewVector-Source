#include "storage/control_trace_backend.h"

#include <FS.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <time.h>
#include "model/stop_result_values.h"
#include "model/shot_save_policy.h"
#include "storage/history_root.h"

namespace {
constexpr char TRACE_DIR[] = "/brewvector/traces";
constexpr char TRACE_INDEX_PATH[] = "/brewvector/traces/index.csv";
constexpr uint32_t FLUSH_PERIOD_MS = 1000;
constexpr size_t RECENT_INDEX_TAIL_BYTES = 16384;

void safeProfile(char* destination, size_t capacity, const char* source) {
  snprintf(destination, capacity, "%s", source && source[0] ? source : "OFF");
  for (size_t i = 0; destination[i] != '\0'; ++i) {
    if (destination[i] == ',' || destination[i] == '\r' ||
        destination[i] == '\n')
      destination[i] = '_';
  }
}

bool parseSummary(const String& line, ShotSummary& summary) {
  char profile[24] = {};
  unsigned long id = 0;
  unsigned int samples = 0;
  unsigned int flags = 0;
  unsigned long epoch = 0;
  const int fields = sscanf(line.c_str(), "%lu,%23[^,],%f,%f,%f,%u,%u,%lu", &id,
                            profile, &summary.durationS, &summary.weightG,
                            &summary.peakPressureBar, &samples, &flags, &epoch);
  if (fields < 6) return false;
  summary.resultFlags = fields >= 7 ? flags & 3U : 0;
  summary.startedAtEpoch = fields >= 8 ? epoch : 0;
  summary.id = static_cast<uint32_t>(id);
  summary.sampleCount = static_cast<uint16_t>(min(samples, 65535U));
  safeProfile(summary.profileName, sizeof(summary.profileName), profile);
  return true;
}

const char* stopReasonName(HmiProtocol::ShotStopReason reason) {
  switch (reason) {
    case HmiProtocol::ShotStopReason::Profile: return "profile";
    case HmiProtocol::ShotStopReason::BrewSwitch: return "brew_switch";
    case HmiProtocol::ShotStopReason::Hmi: return "hmi";
    case HmiProtocol::ShotStopReason::Manual: return "manual";
    case HmiProtocol::ShotStopReason::AutoYield: return "auto_yield";
    case HmiProtocol::ShotStopReason::HotWaterSwitch: return "hotwater_switch";
    default: return "unknown";
  }
}

const char* profileExitReasonName(uint8_t reason) {
  switch (reason) {
    case 1: return "timeout";
    case 2: return "pressure_above";
    case 3: return "pressure_below";
    case 4: return "flow_above";
    case 5: return "flow_below";
    case 6: return "weight";
    case 7: return "water_pumped";
    case 8: return "global_weight";
    case 9: return "global_time";
    case 10: return "manual_stop";
    default: return "none";
  }
}
}  // namespace

bool ControlTraceBackend::begin() {
  if (ready_) return true;
  initialized_ = true;
  if (SD_MMC.cardType() == CARD_NONE) {
    error_ = "NO TF CARD";
    return false;
  }
  if (!HistoryRoot::ensure()) {
    error_ = "TF DIR ERROR";
    return false;
  }
  if (!SD_MMC.exists(TRACE_DIR) && !SD_MMC.mkdir(TRACE_DIR)) {
    error_ = "TRACE DIR ERROR";
    return false;
  }
  Preferences preferences;
  preferences.begin("trace_store", false);
  latestId_ = preferences.getUInt("latest_id", 0);
  preferences.end();
  ready_ = true;
  error_ = "OK";
  loadRecent();
  return true;
}

void ControlTraceBackend::pushRecent(const ShotSummary& summary) {
  const size_t limit = min(recentCount_, RECENT_CAPACITY - 1);
  for (size_t i = limit; i > 0; --i) recent_[i] = recent_[i - 1];
  recent_[0] = summary;
  if (recentCount_ < RECENT_CAPACITY) ++recentCount_;
}

void ControlTraceBackend::loadRecent() {
  recentCount_ = 0;
  File index = SD_MMC.open(TRACE_INDEX_PATH, FILE_READ);
  if (!index) return;
  const size_t size = index.size();
  if (size > RECENT_INDEX_TAIL_BYTES) {
    index.seek(size - RECENT_INDEX_TAIL_BYTES);
    // The seek normally lands inside a CSV record. Discard that fragment so
    // only complete summaries enter the recent-shot cache.
    index.readStringUntil('\n');
  }
  while (index.available()) {
    const String line = index.readStringUntil('\n');
    ShotSummary summary{};
    if (parseSummary(line, summary)) pushRecent(summary);
  }
  index.close();
}

bool ControlTraceBackend::start(const HmiProtocol::ControlTracePayload& trace,
                              const char* profileName) {
  if (!begin()) return false;
  Preferences preferences;
  preferences.begin("trace_store", false);
  activeId_ = preferences.getUInt("next_id", 1);
  preferences.putUInt("next_id", activeId_ + 1);
  preferences.end();

  char path[64];
  snprintf(path, sizeof(path), "%s/trace_%06lu.csv", TRACE_DIR,
           static_cast<unsigned long>(activeId_));
  file_ = SD_MMC.open(path, FILE_WRITE);
  if (!file_) {
    error_ = "TF WRITE ERROR";
    return false;
  }

  char profile[32] = {};
  safeProfile(profile, sizeof(profile), profileName);
  file_.println("# BrewVector Control Trace v3 + pump timing diagnostics");
  file_.printf("# id,%lu\n", static_cast<unsigned long>(activeId_));
  file_.printf("# protocol_schema,%u\n", HmiProtocol::SCHEMA_REVISION);
  file_.printf("# profile,%s\n", profile);
  file_.printf("# shot_session_id,%lu\n",
               static_cast<unsigned long>(trace.shotSessionId));
  file_.println("time_s,trace_seq,pressure_raw_bar,pressure_filtered_bar,pressure_rate_bar_s,pump_flow_ml_s,puck_flow_ml_s,output_weight_g,scale_weight_g,model_yield_g,water_pumped_ml,block_temp_c,brew_temp_c,target_command,controller_target_bar,pump_pct,pump_cps,base_pct,p_term_pct,i_term_pct,damping_pct,min_pct,max_pct,pressure_limit_bar,flow_limit_ml_s,flow_q1_ml_s,flow_q9_ml_s,flow_model_reserved,main_tx_drops,rx_gap,control_flags,scale_age_ms,state,mode,step,scale_flags,predictive_gate_open,shot_session_id,diag_seq,diag_window_ms,psm_value_min,psm_value_max,psm_value_changes,zc_raw_edges,zc_accepted_edges,zc_rejected_edges,zc_raw_dt_min_us,zc_dt_min_us,zc_dt_avg_us,zc_dt_max_us,zc_short_count,zc_long_count,psm_decisions,gate_on_count,gate_transitions,max_on_run,max_off_run,gate_high_min_us,gate_high_max_us,gate_low_min_us,gate_low_max_us,psm_pattern_bits,psm_pattern_low,psm_pattern_high,pump_task_gap_min_us,pump_task_gap_max_us,pump_task_late_count");
  active_ = true;
  samples_ = 0;
  receivedGaps_ = 0;
  lastTraceSequence_ = 0;
  activeSessionId_ = trace.shotSessionId;
  lastElapsedS_ = trace.elapsedS;
  bufferLength_ = 0;
  lastFlushMs_ = millis();
  lastAppendMs_ = lastFlushMs_;
  peakPressureBar_ = 0.0f;
  finalWeightG_ = NAN;
  resultFlags_ = 0;
  const time_t wall = time(nullptr);
  startedAtEpoch_ = wall > 1700000000 && isfinite(trace.elapsedS) && trace.elapsedS >= 0
      ? static_cast<uint32_t>(wall) - static_cast<uint32_t>(trace.elapsedS) : 0;
  snprintf(activeProfile_, sizeof(activeProfile_), "%s", profile);
  memset(stepNameWritten_, 0, sizeof(stepNameWritten_));
  error_ = "LOGGING";
  return true;
}

void ControlTraceBackend::appendText(const char* text, size_t length) {
  if (!file_ || !text || length == 0 || length >= BUFFER_CAPACITY) return;
  if (bufferLength_ + length > BUFFER_CAPACITY) flush();
  if (length > BUFFER_CAPACITY - bufferLength_) return;
  memcpy(buffer_ + bufferLength_, text, length);
  bufferLength_ += length;
}

void ControlTraceBackend::flush() {
  if (!file_ || bufferLength_ == 0) return;
  if (file_.write(reinterpret_cast<const uint8_t*>(buffer_), bufferLength_) !=
      bufferLength_) {
    error_ = "TF WRITE ERROR";
  }
  bufferLength_ = 0;
}

void ControlTraceBackend::append(const HmiProtocol::ControlTracePayload& trace,
                               const char* profileName, const char* phaseName) {
  // Session identity comes from Main. Never infer a new shot from a timing
  // fluctuation or UART scheduling delay.
  if (active_ && trace.shotSessionId != activeSessionId_) finish();
  if (!active_ && !start(trace, profileName)) return;

  if (trace.stepIndex < sizeof(stepNameWritten_) &&
      !stepNameWritten_[trace.stepIndex] && phaseName && phaseName[0]) {
    char safeName[sizeof(HmiProtocol::TelemetryPayload::phaseName)] = {};
    size_t length = 0;
    for (; length + 1 < sizeof(safeName) && phaseName[length]; ++length) {
      const char c = phaseName[length];
      safeName[length] = c == ',' || c == '\r' || c == '\n' ? ' ' : c;
    }
    if (safeName[0]) {
      char comment[48];
      const int count = snprintf(comment, sizeof(comment), "# step_name,%u,%s\n",
                                 static_cast<unsigned>(trace.stepIndex), safeName);
      if (count > 0 && count < static_cast<int>(sizeof(comment))) {
        appendText(comment, static_cast<size_t>(count));
        stepNameWritten_[trace.stepIndex] = true;
      }
    }
  }

  uint32_t gap = 0;
  if (lastTraceSequence_ != 0 && trace.traceSequence > lastTraceSequence_ + 1) {
    gap = trace.traceSequence - lastTraceSequence_ - 1;
    receivedGaps_ += gap;
  }
  lastTraceSequence_ = trace.traceSequence;
  lastElapsedS_ = trace.elapsedS;
  lastAppendMs_ = millis();
  peakPressureBar_ = max(peakPressureBar_, trace.pressureFilteredBar);
  // A trace sample may contain model output. Only ShotEnd establishes yield.

  char line[1400];
  int written = snprintf(
      line, sizeof(line),
      "%.3f,%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.3f,%.3f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.6f,%.4f,%.4f,%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%lu",
      trace.elapsedS, static_cast<unsigned long>(trace.traceSequence),
      trace.pressureRawBar, trace.pressureFilteredBar,
      trace.pressureRateBarPerS, trace.pumpFlowMlPerS, trace.puckFlowMlPerS,
      trace.outputWeightG, trace.scaleWeightG, trace.modelYieldG,
      trace.waterPumpedG,
      trace.blockTempC, trace.brewTempC, trace.targetCommand,
      trace.controllerTargetBar, trace.pumpPercent, trace.pumpCps,
      trace.basePercent, trace.pTermPercent, trace.iTermPercent,
      trace.dampingPercent, trace.minPercent, trace.maxPercent,
      trace.pressureLimitBar, trace.flowLimitMlPerS, trace.flowQ1MlPerS,
      trace.flowQ9MlPerS, trace.flowModelReserved,
      static_cast<unsigned long>(trace.mainTraceDropCount),
      static_cast<unsigned long>(gap), static_cast<unsigned>(trace.controlFlags),
      static_cast<unsigned>(trace.scaleAgeMs),
      static_cast<unsigned>(trace.machineState),
      static_cast<unsigned>(trace.controlMode),
      static_cast<unsigned>(trace.stepIndex),
      static_cast<unsigned>(trace.scaleFlags),
      static_cast<unsigned>(trace.predictiveGateOpen),
      static_cast<unsigned long>(trace.shotSessionId));

  if (written > 0 && written < static_cast<int>(sizeof(line))) {
    const int diagnosticWritten = snprintf(
        line + written, sizeof(line) - static_cast<size_t>(written),
        ",%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
        static_cast<unsigned long>(trace.diagSequence),
        static_cast<unsigned long>(trace.diagWindowMs),
        static_cast<unsigned long>(trace.psmValueMin),
        static_cast<unsigned long>(trace.psmValueMax),
        static_cast<unsigned long>(trace.psmValueChanges),
        static_cast<unsigned long>(trace.zcRawEdges),
        static_cast<unsigned long>(trace.zcAcceptedEdges),
        static_cast<unsigned long>(trace.zcRejectedEdges),
        static_cast<unsigned long>(trace.zcRawDtMinUs),
        static_cast<unsigned long>(trace.zcAcceptedDtMinUs),
        static_cast<unsigned long>(trace.zcAcceptedDtAverageUs),
        static_cast<unsigned long>(trace.zcAcceptedDtMaxUs),
        static_cast<unsigned long>(trace.zcShortIntervals),
        static_cast<unsigned long>(trace.zcLongIntervals),
        static_cast<unsigned long>(trace.psmDecisions),
        static_cast<unsigned long>(trace.gateOnDecisions),
        static_cast<unsigned long>(trace.gateTransitions),
        static_cast<unsigned long>(trace.maxOnRun),
        static_cast<unsigned long>(trace.maxOffRun),
        static_cast<unsigned long>(trace.gateHighMinUs),
        static_cast<unsigned long>(trace.gateHighMaxUs),
        static_cast<unsigned long>(trace.gateLowMinUs),
        static_cast<unsigned long>(trace.gateLowMaxUs),
        static_cast<unsigned long>(trace.psmPatternBits),
        static_cast<unsigned long>(trace.psmPatternLow),
        static_cast<unsigned long>(trace.psmPatternHigh),
        static_cast<unsigned long>(trace.pumpTaskGapMinUs),
        static_cast<unsigned long>(trace.pumpTaskGapMaxUs),
        static_cast<unsigned long>(trace.pumpTaskLateCount));
    if (diagnosticWritten > 0 &&
        diagnosticWritten <
            static_cast<int>(sizeof(line) - static_cast<size_t>(written))) {
      written += diagnosticWritten;
    } else {
      written = -1;
    }
  }
  if (written > 0 && written < static_cast<int>(sizeof(line))) {
    appendText(line, static_cast<size_t>(written));
    ++samples_;
  }
}

void ControlTraceBackend::poll(uint32_t nowMs) {
  // A UART disconnect must not leave the active file indefinitely open.
  // Signed delta prevents an append timestamp taken later in the same HMI
  // loop from underflowing into a false multi-billion-ms timeout.
  if (active_ &&
      static_cast<int32_t>(nowMs - lastAppendMs_) >= 2000) {
    finish();
    return;
  }
  if (active_ &&
      static_cast<int32_t>(nowMs - lastFlushMs_) >=
          static_cast<int32_t>(FLUSH_PERIOD_MS)) {
    flush();
    lastFlushMs_ = nowMs;
  }
}

bool ControlTraceBackend::appendSummary() {
  ShotSummary summary{};
  summary.id = activeId_;
  summary.durationS = lastElapsedS_;
  summary.weightG = finalWeightG_;
  summary.resultFlags = resultFlags_;
  summary.startedAtEpoch = startedAtEpoch_;
  summary.peakPressureBar = peakPressureBar_;
  summary.sampleCount = static_cast<uint16_t>(min(samples_, uint32_t(65535)));
  safeProfile(summary.profileName, sizeof(summary.profileName), activeProfile_);
  const bool newIndex = !SD_MMC.exists(TRACE_INDEX_PATH);
  File index = SD_MMC.open(TRACE_INDEX_PATH, FILE_APPEND);
  if (!index) return false;
  if (newIndex)
    index.println("id,profile,duration_s,weight_g,peak_bar,samples,result_flags,started_at_epoch");
  index.printf("%lu,%s,%.3f,%.3f,%.3f,%u,%u,%lu\n",
               static_cast<unsigned long>(summary.id), summary.profileName,
               summary.durationS, summary.weightG, summary.peakPressureBar,
               summary.sampleCount, summary.resultFlags,
               static_cast<unsigned long>(summary.startedAtEpoch));
  index.close();
  pushRecent(summary);
  return true;
}

void ControlTraceBackend::finish() { finishInternal(nullptr); }

void ControlTraceBackend::finish(const HmiProtocol::ShotEndPayload& shotEnd) {
  if (!active_ || (shotEnd.shotSessionId != 0 &&
                   shotEnd.shotSessionId != activeSessionId_))
    return;
  finishInternal(&shotEnd);
}

void ControlTraceBackend::finishInternal(
    const HmiProtocol::ShotEndPayload* shotEnd) {
  if (!active_) return;
  if (shotEnd) {
    lastElapsedS_ = shotEnd->elapsedS;
    const bool online = (shotEnd->flags & HmiProtocol::ShotEndScaleOnline) != 0;
    resultFlags_ = StopResultValues::flags(online, shotEnd->scaleWeightG);
    finalWeightG_ = StopResultValues::yield(online, shotEnd->scaleWeightG);
    char footer[512];
    const int written = snprintf(
        footer, sizeof(footer),
        "# stop_reason,%s\n"
        "# stop_reason_code,%u\n"
        "# profile_exit_reason,%s\n"
        "# profile_exit_reason_code,%u\n"
        "# stop_step,%u\n"
        "# stop_elapsed_s,%.3f\n"
        "# stop_output_weight_g,%.3f\n"
        "# stop_scale_weight_g,%.3f\n"
        "# stop_model_yield_g,%.3f\n"
        "# stop_flags,%u\n",
        stopReasonName(shotEnd->stopReason),
        static_cast<unsigned>(shotEnd->stopReason),
        profileExitReasonName(shotEnd->profileExitReason),
        static_cast<unsigned>(shotEnd->profileExitReason),
        static_cast<unsigned>(shotEnd->stepIndex), shotEnd->elapsedS,
        shotEnd->outputWeightG, shotEnd->scaleWeightG,
        shotEnd->modelYieldG, static_cast<unsigned>(shotEnd->flags));
    if (written > 0 && written < static_cast<int>(sizeof(footer)))
      appendText(footer, static_cast<size_t>(written));
  }
  flush();
  file_.close();
  if (!ShotSavePolicy::accepts(lastElapsedS_, peakPressureBar_, samples_)) {
    char path[72];
    snprintf(path, sizeof(path), "%s/trace_%06lu.csv", TRACE_DIR,
             static_cast<unsigned long>(activeId_));
    const bool removed = SD_MMC.remove(path);
    activeId_ = 0;
    activeSessionId_ = 0;
    active_ = false;
    error_ = removed ? "SHOT FILTERED" : "FILTER REMOVE ERROR";
    return;
  }
  const bool indexed = appendSummary();
  Preferences preferences;
  preferences.begin("trace_store", false);
  preferences.putUInt("latest_id", activeId_);
  preferences.end();
  latestId_ = activeId_;
  activeId_ = 0;
  activeSessionId_ = 0;
  active_ = false;
  error_ = indexed ? "SAVED" : "INDEX WRITE ERROR";
}

bool ControlTraceBackend::load(uint32_t id, ShotChart& chart) {
  if (!begin() || id == 0) return false;
  char path[72];
  snprintf(path, sizeof(path), "%s/trace_%06lu.csv", TRACE_DIR,
           static_cast<unsigned long>(id));
  File file = SD_MMC.open(path, FILE_READ);
  if (!file) { error_ = "TRACE NOT FOUND"; return false; }
  chart.reset();
  uint32_t sequence = 1;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.isEmpty() || line[0] == '#' || line.startsWith("time_s"))
      continue;
    char raw[1400];
    line.toCharArray(raw, sizeof(raw));
    ShotChartSample sample{};
    float targetCommand = 0.0f;
    float controllerTarget = 0.0f;
    unsigned int mode = 0;
    unsigned int step = 0;
    char* save = nullptr;
    char* token = strtok_r(raw, ",", &save);
    uint8_t column = 0;
    while (token) {
      switch (column) {
        case 0: sample.elapsedS = strtof(token, nullptr); break;
        case 3: sample.pressureBar = strtof(token, nullptr); break;
        case 5: sample.flowGps = strtof(token, nullptr); break;
        case 7: sample.weightG = strtof(token, nullptr); break;
        case 13: targetCommand = strtof(token, nullptr); break;
        case 14: controllerTarget = strtof(token, nullptr); break;
        case 33: mode = static_cast<unsigned int>(strtoul(token, nullptr, 10)); break;
        case 34: step = static_cast<unsigned int>(strtoul(token, nullptr, 10)); break;
      }
      token = strtok_r(nullptr, ",", &save);
      ++column;
    }
    if (column < 35) continue;
    sample.targetPressureBar = mode == 0 && controllerTarget > 0 ? controllerTarget : NAN;
    sample.targetFlowGps = mode == 1 ? targetCommand : 0.0f;
    sample.stepIndex = static_cast<uint8_t>(min(step, 255U));
    sample.sourceTimestampMs = sequence++;
    chart.appendSample(sample);
  }
  file.close();
  if (chart.size() < 2) { error_ = "TRACE EMPTY"; return false; }
  error_ = "LOADED";
  return true;
}

bool ControlTraceBackend::eraseAll() {
  if (!begin()) return false;
  File directory = SD_MMC.open(TRACE_DIR);
  if (!directory || !directory.isDirectory()) {
    error_ = "TRACE DIR ERROR";
    return false;
  }
  File entry = directory.openNextFile();
  while (entry) {
    char path[96];
    snprintf(path, sizeof(path), "%s", entry.path());
    const bool isDirectory = entry.isDirectory();
    entry.close();
    if (!isDirectory) SD_MMC.remove(path);
    entry = directory.openNextFile();
  }
  directory.close();
  recentCount_ = 0;
  latestId_ = 0;
  Preferences preferences;
  preferences.begin("trace_store", false);
  preferences.putUInt("latest_id", 0);
  preferences.end();
  error_ = "ERASED";
  return true;
}

bool ControlTraceBackend::latestPath(char* destination, size_t capacity) const {
  if (!destination || capacity == 0 || latestId_ == 0) return false;
  snprintf(destination, capacity, "%s/trace_%06lu.csv", TRACE_DIR,
           static_cast<unsigned long>(latestId_));
  return true;
}
