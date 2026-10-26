#pragma once

#include <Arduino.h>

class MainLink;

namespace MainFirmwareUpdater {

void begin(MainLink& link);
void poll();

bool beginUpload(const String& filename);
bool writeUpload(const uint8_t* data, size_t length);
bool finishUpload();
void abortUpload();
bool startTransfer();
void cancelTransfer();

bool fileReady();
bool busy();
uint32_t imageSize();
uint8_t progressPercent();
const char* stateText();
const char* errorText();
String statusJson();

}  // namespace MainFirmwareUpdater
