#pragma once
#include <stdint.h>

// The approved 320x240 standby frame. Drawing and touch use the same bounds.
namespace ReadyLayout {
struct Rect {
  int x, y, w, h;
  constexpr bool contains(int px, int py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};
constexpr Rect Profile{8, 129, 200, 49};
constexpr Rect Yield{216, 129, 96, 49};
constexpr Rect Start{8, 186, 160, 46};
constexpr Rect Manual{8, 170, 149, 32};
constexpr Rect Flush{174, 186, 88, 46};
constexpr Rect Menu{268, 186, 44, 46};
constexpr Rect MenuHistory{168, 98, 144, 40};
constexpr Rect MenuSettings{168, 138, 144, 40};
constexpr Rect YieldCancel{8, 191, 148, 43};
constexpr Rect YieldApply{164, 191, 148, 43};
constexpr Rect StopFlush{24, 188, 272, 44};
enum class Target { None, Profile, Start, Manual, Flush, Menu, History, Settings, Yield };
constexpr Target targetAt(int x, int y) {
  return Profile.contains(x, y) ? Target::Profile :
         Yield.contains(x, y) ? Target::Yield :
         Start.contains(x, y) ? Target::Start :
         Flush.contains(x, y) ? Target::Flush :
         Menu.contains(x, y) ? Target::Menu : Target::None;
}
constexpr Target menuTargetAt(int x, int y) {
  return MenuHistory.contains(x, y) ? Target::History :
         MenuSettings.contains(x, y) ? Target::Settings : Target::None;
}
constexpr Target enabledTarget(Target target, bool start, bool manual, bool flush) {
  return (target == Target::Start && !start) ||
         (target == Target::Manual && !manual) ||
         (target == Target::Flush && !flush) ? Target::None : target;
}
// A soft primary pulse, a smaller echo, then a long rest. The returned
// intensity lets the renderer blend colours instead of switching outlines.
// Geometry stays fixed, so the animation never changes the touch target.
constexpr uint8_t heartbeatPulse(uint32_t phaseMs, uint32_t startMs,
                                 uint32_t peakMs, uint32_t endMs,
                                 uint8_t amplitude) {
  return phaseMs <= startMs || phaseMs >= endMs ? 0U :
         phaseMs <= peakMs
             ? static_cast<uint8_t>((phaseMs - startMs) * amplitude /
                                    (peakMs - startMs))
             : static_cast<uint8_t>((endMs - phaseMs) * amplitude /
                                    (endMs - peakMs));
}
constexpr uint8_t heartbeatIntensity(uint32_t nowMs) {
  return heartbeatPulse(nowMs % 2400U, 0U, 280U, 560U, 160U) >
                 heartbeatPulse(nowMs % 2400U, 850U, 1090U, 1330U, 100U)
             ? heartbeatPulse(nowMs % 2400U, 0U, 280U, 560U, 160U)
             : heartbeatPulse(nowMs % 2400U, 850U, 1090U, 1330U, 100U);
}
}
