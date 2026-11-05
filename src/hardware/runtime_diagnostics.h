#pragma once
#include "protocol/main_diagnostics.h"
namespace RuntimeDiagnostics {
void begin();
void fault(const MainDiagnosticsWire::FaultEvent& event);
void poll(bool stoppedAndHeaterOff);
MainDiagnosticsWire::Status snapshot();
}
