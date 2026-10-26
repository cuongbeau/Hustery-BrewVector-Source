#pragma once
#include <Arduino.h>
#include <functional>

namespace WebJobs {
using Work = std::function<void(String& body, int& status)>;
// HTTP worker only; accepts once, returns a ticket, never retries a mutation.
void submit(Work work);
void registerRoutes();
}
