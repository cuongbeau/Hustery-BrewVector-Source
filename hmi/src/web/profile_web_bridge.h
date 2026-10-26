#pragma once

#include <Arduino.h>

class MainLink;

namespace ProfileWebBridge {

// Adapts the browser profile JSON to the typed, framed UART protocol.
// Main remains the sole owner of persistent profiles. The list endpoint only
// returns metadata; the editor requests one canonical profile when it is used.
bool readSummaries(MainLink& link, String& response, String& error);
bool readOne(MainLink& link, uint8_t customIndex, String& response,
             String& error);
bool saveOne(MainLink& link, const String& request, String& response,
             String& error);
bool deleteOne(MainLink& link, const String& request, String& response,
               String& error);
bool replaceAll(MainLink& link, const String& request, String& response,
                String& error);

}  // namespace ProfileWebBridge
