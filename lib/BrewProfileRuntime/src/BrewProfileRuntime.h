#pragma once

// Load dependencies outside the namespace; keep upstream Profile/Phase names
// isolated from the active legacy interfaces/profile_types.h.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <vector>

namespace BrewProfileRuntime {
#include "../upstream/profile.h"
}  // namespace BrewProfileRuntime

// Do not include upstream/profile.h directly elsewhere (its include guard is
// global). Runtime integration and persisted-profile replacement are deferred.
