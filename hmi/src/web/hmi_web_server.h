#pragma once

class MainLink;
class ShotStore;
class ControlTraceStore;
class HmiSettings;
class HmiDisplay;
class AudioService;
class ScaleManager;

namespace HmiWeb {

void attachDataSources(MainLink& mainLink, ShotStore& shotStore,
                       ControlTraceStore& controlTraceStore,
                       HmiSettings& settings, HmiDisplay& display,
                       AudioService& audio, ScaleManager& scaleManager);
void begin();
void poll();
void stop();
void resume();

}  // namespace HmiWeb
