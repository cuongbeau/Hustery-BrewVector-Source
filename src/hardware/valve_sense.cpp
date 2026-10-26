// File: src/hardware/valve_sense.cpp
// Tách từ control_logic.h dòng 443-580 (senseTick)
// Refactor: 2026-04-20
// KHÔNG SỬA debounce timing hay logic phát hiện van

#include "valve_sense.h"
#include "config/hardware_pins.h"

namespace ValveSense {

  static constexpr bool BREW_ACTIVE_LOW = true;
  static constexpr uint16_t BREW_ON_MIN_MS  = 60;
  // The stock-board sense signal follows 50 Hz AC.  A 10 ms inactive window
  // is only one half-cycle and can be mistaken for a real valve-off edge.
  // Keep this beyond one complete 20 ms mains cycle while retaining a much
  // faster stop response than the old 150 ms filter.
  static constexpr uint16_t BREW_OFF_MIN_MS = 25;

  static constexpr bool HOT_ACTIVE_LOW = true;
  static constexpr uint16_t HOT_ON_MIN_MS  = 60;
  static constexpr uint16_t HOT_OFF_MIN_MS = 25;

  void init() {
#if USE_BREW_SENSE
    pinMode(CURRENT_SENSE_PIN, INPUT);
#endif
#if USE_HOTWATER_SENSE
    pinMode(HOTWATER_SENSE_PIN, INPUT);
#endif
  }

  bool isMachineValveActive() {
#if USE_BREW_SENSE
    int lv = digitalRead(CURRENT_SENSE_PIN);
    return BREW_ACTIVE_LOW ? (lv == LOW) : (lv == HIGH);
#else
    return false;
#endif
  }

#if USE_BREW_SENSE
  static bool readBrewActiveRaw() {
    int lv = digitalRead(CURRENT_SENSE_PIN);
    return BREW_ACTIVE_LOW ? (lv == LOW) : (lv == HIGH);
  }
#endif

#if USE_HOTWATER_SENSE
  static bool readHotActiveRaw() {
    int lv = digitalRead(HOTWATER_SENSE_PIN);
    return HOT_ACTIVE_LOW ? (lv == LOW) : (lv == HIGH);
  }
#endif

  ValveEvent update() {
    const unsigned long now = millis();
    ValveEvent event = ValveEvent::NONE;

    // ===== Brew sense =====
#if USE_BREW_SENSE
    static bool brew_inited=false, brew_sensed=false, brew_seenInactive=false, brew_lastRaw=false;
    static unsigned long brew_bootMs=0, brew_edgeMs=0;

    if (!brew_inited) {
      brew_bootMs = now;
      brew_lastRaw = readBrewActiveRaw();
      brew_sensed=false;
      brew_seenInactive = (brew_lastRaw==false);
      brew_edgeMs=0; brew_inited=true;
    }
    if (now - brew_bootMs >= 700) {
      bool raw = readBrewActiveRaw();
      if (!raw) brew_seenInactive = true;

      // start
      if (!brew_sensed && brew_seenInactive) {
        if (!brew_lastRaw && raw) brew_edgeMs = now;
        if (raw && brew_edgeMs && (now - brew_edgeMs >= BREW_ON_MIN_MS)) {
          brew_sensed = true; brew_edgeMs = 0;
          event = ValveEvent::BREW_START;
        }
        if (!raw) brew_edgeMs = 0;
      }

      // stop
      if (brew_sensed) {
        if (brew_lastRaw && !raw && brew_edgeMs==0) brew_edgeMs = now;
        if (!raw && brew_edgeMs && (now - brew_edgeMs >= BREW_OFF_MIN_MS)) {
          brew_sensed=false; brew_edgeMs=0;
          event = ValveEvent::BREW_STOP;
        }
        if (raw) brew_edgeMs = 0;
      }

      brew_lastRaw = raw;
    }
#endif

    // Nếu đã có brew event, trả về ngay (brew ưu tiên)
    if (event != ValveEvent::NONE) return event;

    // ===== Hot-water sense =====
#if USE_HOTWATER_SENSE
    static bool hot_inited=false, hot_sensed=false, hot_seenInactive=false, hot_lastRaw=false;
    static unsigned long hot_bootMs=0, hot_edgeMs=0;

    if (!hot_inited) {
      hot_bootMs = now;
      hot_lastRaw = readHotActiveRaw();
      hot_sensed=false;
      hot_seenInactive = (hot_lastRaw==false);
      hot_edgeMs=0; hot_inited=true;
    }
    if (now - hot_bootMs >= 700) {
      bool raw = readHotActiveRaw();
      if (!raw) hot_seenInactive = true;

      // start
      if (!hot_sensed && hot_seenInactive) {
        if (!hot_lastRaw && raw) hot_edgeMs = now;
        if (raw && hot_edgeMs && (now - hot_edgeMs >= HOT_ON_MIN_MS)) {
          hot_sensed = true; hot_edgeMs = 0;
          event = ValveEvent::HOT_START;
        }
        if (!raw) hot_edgeMs = 0;
      }

      // stop
      if (hot_sensed) {
        if (hot_lastRaw && !raw && hot_edgeMs==0) hot_edgeMs = now;
        if (!raw && hot_edgeMs && (now - hot_edgeMs >= HOT_OFF_MIN_MS)) {
          hot_sensed=false; hot_edgeMs=0;
          event = ValveEvent::HOT_STOP;
        }
        if (raw) hot_edgeMs = 0;
      }

      hot_lastRaw = raw;
    }
#endif

    return event;
  }

} // namespace ValveSense
