#include "runtime_diagnostics.h"
#include "src/model/diagnostic_record.h"
#include "ads1115_acquisition.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <string.h>

namespace RuntimeDiagnostics {
namespace {
// Two checksummed slots: interrupted writes leave the previous committed slot.
RTC_NOINIT_ATTR uint8_t retained[2][sizeof(DiagnosticRecord::Record)];
DiagnosticRecord::Record current;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
uint8_t slot = 0, resetReason = 0;
uint32_t savedFaultCount = 0, lastSaveAttempt = 0;
bool storageOk = false;
void commitLocked() {
  ++current.sequence;
  DiagnosticRecord::seal(current);
  slot ^= 1;
  memcpy(retained[slot], &current, sizeof(current));
}
}
void begin() {
  resetReason = static_cast<uint8_t>(esp_reset_reason());
  bool found = false;
  if (resetReason != ESP_RST_POWERON && resetReason != ESP_RST_BROWNOUT) {
    for (uint8_t i = 0; i < 2; ++i) {
      DiagnosticRecord::Record candidate;
      memcpy(&candidate, retained[i], sizeof(candidate));
      if (DiagnosticRecord::valid(candidate) && (!found || DiagnosticRecord::newer(candidate, current))) {
        current = candidate; slot = i; found = true;
      }
    }
  }
  Preferences prefs;
  if (prefs.begin("bv-diag", false)) {
    DiagnosticRecord::Record disk;
    const bool diskOk = prefs.getBytesLength("record") == sizeof(disk) &&
        prefs.getBytes("record", &disk, sizeof(disk)) == sizeof(disk) && DiagnosticRecord::valid(disk);
    if (!found && diskOk) current = disk;
    ++current.bootCount; current.bootId = esp_random();
    commitLocked();
    storageOk = prefs.putBytes("record", &current, sizeof(current)) == sizeof(current);
    if (storageOk) savedFaultCount = current.faultCount;
    prefs.end();
  } else {
    ++current.bootCount; current.bootId = esp_random(); commitLocked();
  }
}
void fault(const MainDiagnosticsWire::FaultEvent& event) {
  portENTER_CRITICAL(&mux);
  current.lastFault = event; current.lastFault.bootId = current.bootId;
  ++current.faultCount; commitLocked();
  portEXIT_CRITICAL(&mux);
}
void poll(bool stoppedAndHeaterOff) {
  // Flash is never written in sensor/SSR tasks or while brewing/heating.
  if (!stoppedAndHeaterOff || millis() - lastSaveAttempt < 5000) return;
  DiagnosticRecord::Record copy;
  portENTER_CRITICAL(&mux);
  const bool pending = !storageOk || savedFaultCount != current.faultCount;
  copy = current;
  portEXIT_CRITICAL(&mux);
  if (!pending) return;
  lastSaveAttempt = millis();
  Preferences prefs;
  const bool opened = prefs.begin("bv-diag", false);
  const bool saved = opened && prefs.putBytes("record", &copy, sizeof(copy)) == sizeof(copy);
  if (opened) prefs.end();
  portENTER_CRITICAL(&mux);
  storageOk = saved;
  if (saved) savedFaultCount = copy.faultCount;
  portEXIT_CRITICAL(&mux);
}
MainDiagnosticsWire::Status snapshot() {
  MainDiagnosticsWire::Status result;
  portENTER_CRITICAL(&mux);
  result.bootId = current.bootId; result.bootCount = current.bootCount;
  result.faultCount = current.faultCount; result.lastFault = current.lastFault;
  result.faultSaved = storageOk && savedFaultCount == current.faultCount;
  portEXIT_CRITICAL(&mux);
  result.resetReason = resetReason; result.uptimeMs = millis();
  result.freeHeap = ESP.getFreeHeap(); result.minFreeHeap = ESP.getMinFreeHeap();
  result.largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  result.adcFast = Ads1115Acquisition::measuredFast;
  result.adcSingleShot = Ads1115Acquisition::singleShot;
  result.adcMeanSamples = Ads1115Acquisition::measuredFast ? 7 : 1;
  result.adcPollMs = Ads1115Acquisition::measuredFast ? Ads1115Acquisition::pollMs : 30;
  strncpy(result.build, __DATE__ " " __TIME__, sizeof(result.build) - 1);
  return result;
}
}
