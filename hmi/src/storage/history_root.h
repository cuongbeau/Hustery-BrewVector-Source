#pragma once

#include <SD_MMC.h>

namespace HistoryRoot {
constexpr char Current[] = "/brewvector";
constexpr char Previous[] = "/giggauino";

inline bool ensure() {
  if (SD_MMC.exists(Current)) return true;
  if (SD_MMC.exists(Previous)) return SD_MMC.rename(Previous, Current);
  return SD_MMC.mkdir(Current);
}
}  // namespace HistoryRoot
