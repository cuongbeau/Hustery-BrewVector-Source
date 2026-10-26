#include "storage/shot_store.h"

#include <FS.h>
#include <Preferences.h>
#include <SD_MMC.h>

#include "board_pins.h"
#include "model/shot_save_policy.h"
#include "storage/history_root.h"

namespace {
constexpr char SHOT_DIR[] = "/brewvector/shots";
constexpr char INDEX_PATH[] = "/brewvector/index.csv";

void safeProfile(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
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
  const int fields = sscanf(line.c_str(), "%lu,%23[^,],%f,%f,%f,%u", &id,
                            profile, &summary.durationS, &summary.weightG,
                            &summary.peakPressureBar, &samples);
  if (fields != 6) return false;
  summary.id = static_cast<uint32_t>(id);
  summary.sampleCount = static_cast<uint16_t>(min(samples, 65535U));
  safeProfile(summary.profileName, sizeof(summary.profileName), profile);
  return true;
}
}  // namespace

bool ShotStore::begin() {
  if (ready_) return true;
  if (initialized_) SD_MMC.end();
  initialized_ = true;
  SD_MMC.setPins(BoardPins::SD_CLK, BoardPins::SD_CMD, BoardPins::SD_D0,
                 BoardPins::SD_D1, BoardPins::SD_D2, BoardPins::SD_D3);
  if (!SD_MMC.begin("/sdcard", false)) {
    error_ = "NO TF CARD";
    return false;
  }
  if (SD_MMC.cardType() == CARD_NONE) {
    error_ = "NO TF CARD";
    SD_MMC.end();
    return false;
  }
  if (!HistoryRoot::ensure()) {
    error_ = "TF DIR ERROR";
    return false;
  }
  if (!SD_MMC.exists(SHOT_DIR) && !SD_MMC.mkdir(SHOT_DIR)) {
    error_ = "TF DIR ERROR";
    return false;
  }
  ready_ = true;
  error_ = "OK";
  // v1 summaries are retained only so Erase History can remove legacy data.
  // Current History uses Control Trace, so scanning the old index at boot only
  // delays Wi-Fi and UART startup as the file grows.
  recentCount_ = 0;
  return true;
}

void ShotStore::pushRecent(const ShotSummary& summary) {
  const size_t limit = min(recentCount_, RECENT_CAPACITY - 1);
  for (size_t i = limit; i > 0; --i) recent_[i] = recent_[i - 1];
  recent_[0] = summary;
  if (recentCount_ < RECENT_CAPACITY) ++recentCount_;
}

void ShotStore::loadRecent() {
  recentCount_ = 0;
  File index = SD_MMC.open(INDEX_PATH, FILE_READ);
  if (!index) return;
  while (index.available()) {
    const String line = index.readStringUntil('\n');
    ShotSummary summary{};
    if (parseSummary(line, summary)) pushRecent(summary);
  }
  index.close();
}

bool ShotStore::save(const ShotChart& chart, const char* profileName) {
  if (!begin() || chart.size() < 2) {
    if (ready_ && chart.size() < 2) error_ = "NO SHOT DATA";
    return false;
  }
  if (chart.lastSourceTimestampMs() == lastSavedSourceTimestampMs_) {
    error_ = "SAVED";
    return true;
  }
  if (!ShotSavePolicy::accepts(chart.lastElapsedS(), chart.peakPressureBar(), chart.size())) {
    error_ = "SHOT FILTERED";
    return false;
  }

  Preferences preferences;
  preferences.begin("shot_store", false);
  uint32_t id = preferences.getUInt("next_id", 1);
  preferences.putUInt("next_id", id + 1);
  preferences.end();

  ShotSummary summary{};
  summary.id = id;
  summary.durationS = chart.lastElapsedS();
  summary.weightG = chart.lastWeightG();
  summary.peakPressureBar = chart.peakPressureBar();
  summary.sampleCount = static_cast<uint16_t>(min(chart.size(), size_t(65535)));
  safeProfile(summary.profileName, sizeof(summary.profileName), profileName);

  char path[48];
  snprintf(path, sizeof(path), "%s/shot_%06lu.csv", SHOT_DIR,
           static_cast<unsigned long>(id));
  File shotFile = SD_MMC.open(path, FILE_WRITE);
  if (!shotFile) {
    error_ = "TF WRITE ERROR";
    return false;
  }
  shotFile.println("# BrewVector Shot v1");
  shotFile.printf("# id,%lu\n", static_cast<unsigned long>(summary.id));
  shotFile.printf("# profile,%s\n", summary.profileName);
  shotFile.println(
      "time_s,pressure_bar,pump_flow_ml_s,weight_g,target_pressure_bar,target_pump_flow_ml_s,step");
  for (size_t i = 0; i < chart.size(); ++i) {
    const ShotChartSample& sample = chart.at(i);
    shotFile.printf("%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u\n", sample.elapsedS,
                    sample.pressureBar, sample.flowGps, sample.weightG,
                    sample.targetPressureBar, sample.targetFlowGps,
                    sample.stepIndex + 1);
  }
  shotFile.close();

  const bool newIndex = !SD_MMC.exists(INDEX_PATH);
  File index = SD_MMC.open(INDEX_PATH, FILE_APPEND);
  if (!index) {
    error_ = "INDEX WRITE ERROR";
    return false;
  }
  if (newIndex)
    index.println("id,profile,duration_s,weight_g,peak_bar,samples");
  index.printf("%lu,%s,%.3f,%.3f,%.3f,%u\n",
               static_cast<unsigned long>(summary.id), summary.profileName,
               summary.durationS, summary.weightG, summary.peakPressureBar,
               summary.sampleCount);
  index.close();

  pushRecent(summary);
  lastSavedSourceTimestampMs_ = chart.lastSourceTimestampMs();
  error_ = "SAVED";
  return true;
}

bool ShotStore::load(uint32_t id, ShotChart& chart) {
  if (!begin()) return false;
  char path[48];
  snprintf(path, sizeof(path), "%s/shot_%06lu.csv", SHOT_DIR,
           static_cast<unsigned long>(id));
  File file = SD_MMC.open(path, FILE_READ);
  if (!file) {
    error_ = "SHOT NOT FOUND";
    return false;
  }
  chart.reset();
  uint32_t sequence = 1;
  while (file.available()) {
    const String line = file.readStringUntil('\n');
    if (line.isEmpty() || line[0] == '#' || line.startsWith("time_s"))
      continue;
    ShotChartSample sample{};
    unsigned int step = 0;
    const int fields = sscanf(line.c_str(), "%f,%f,%f,%f,%f,%f,%u",
                              &sample.elapsedS, &sample.pressureBar,
                              &sample.flowGps, &sample.weightG,
                              &sample.targetPressureBar,
                              &sample.targetFlowGps, &step);
    if (fields != 7) continue;
    sample.sourceTimestampMs = sequence++;
    sample.stepIndex = step > 0 ? static_cast<uint8_t>(step - 1) : 0;
    chart.appendSample(sample);
  }
  file.close();
  if (chart.size() < 2) {
    error_ = "SHOT FILE EMPTY";
    return false;
  }
  error_ = "LOADED";
  return true;
}

bool ShotStore::eraseAll() {
  if (!begin()) return false;
  File directory = SD_MMC.open(SHOT_DIR);
  if (!directory || !directory.isDirectory()) {
    error_ = "SHOT DIR ERROR";
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
  SD_MMC.remove(INDEX_PATH);
  recentCount_ = 0;
  lastSavedSourceTimestampMs_ = 0;
  error_ = "ERASED";
  return true;
}

uint64_t ShotStore::totalBytes() const {
  return ready_ ? SD_MMC.totalBytes() : 0;
}

uint64_t ShotStore::usedBytes() const {
  return ready_ ? SD_MMC.usedBytes() : 0;
}
