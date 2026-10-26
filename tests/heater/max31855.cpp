#include "src/hardware/max31855_frame.h"
using namespace Max31855Frame;
constexpr uint32_t room = (100U << 18) | (400U << 4);
static_assert(celsius(room) == 25 && issues(room) == 0);
static_assert(celsius(0xfffc0000U) == -0.25f);
static_assert(issues(room | 0x10001U) == 1);
static_assert(issues(room | 0x10002U) == 2);
static_assert(issues(room | 0x10004U) == 4);
static_assert(issues(room | 0x10007U) == 7);
static_assert(issues(room | 1U) == 9); // Fault summary inconsistent.
static_assert(issues(room | 0x10000U) == 8);
static_assert(issues(room | 0x20000U) == 8);
static_assert(issues(room | 8U) == 8);
static_assert(issues(0xffffffffU) == 15); // Floating-high frame isn't just OC.
static_assert(issues(0) == 0); // Stuck-low cannot be diagnosed from this alone.
static_assert(issues(2004U << 18) == 16);
static_assert(issues(0xfebc0000U) == 16); // -20.25 C
static_assert(issues(room, false) == 32);
