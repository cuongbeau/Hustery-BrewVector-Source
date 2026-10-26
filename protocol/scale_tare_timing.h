#pragma once
#include <stdint.h>

// Scale prestart order, scheduled without delay() in either main loop.
// A late UART request cannot extend the lease or tare after Main's timeout.
namespace ScaleTareTiming {
constexpr uint32_t SecondTareMs = 50;
constexpr uint32_t SettleMs = 200;
constexpr uint32_t LeaseMs = 700;
constexpr uint32_t MainWaitMs = 1000;
constexpr uint32_t FreshMs = 750;
constexpr bool newer(uint32_t a, uint32_t b) {
  return static_cast<int32_t>(a - b) > 0;
}
constexpr bool leaseValid(uint32_t deadline, uint32_t now) {
  return newer(deadline, now) && deadline - now <= LeaseMs;
}
constexpr bool secondDue(uint32_t now, uint32_t first) {
  return now - first >= SecondTareMs;
}
constexpr bool settled(uint32_t now, uint32_t second) {
  return now - second >= SettleMs;
}
constexpr bool fresh(uint32_t now, uint32_t received, uint32_t age) {
  return age <= FreshMs && now - received <= FreshMs - age;
}
}
