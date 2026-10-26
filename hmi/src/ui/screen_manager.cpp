#include "ui/screen_manager.h"

#include <math.h>
#include <stdio.h>

#include "ui/components.h"
#include "ui/theme.h"
#include "ui/ready_layout.h"
#include "ui/brew_layout.h"
#include "ui/result_layout.h"
#include "ui/settings_layout.h"
#include "thermal_calibration.h"

using namespace UiComponents;

namespace {
bool isSettingsChild(ScreenId id) {
  return id == ScreenId::Calibration || id == ScreenId::ThermalCal || id == ScreenId::HeaterSettings || id == ScreenId::Connections || id == ScreenId::WebAccess ||
         id == ScreenId::Wifi || id == ScreenId::Networks ||
         id == ScreenId::Scale || id == ScreenId::PressureCal ||
         id == ScreenId::Machine || id == ScreenId::DisplaySound ||
         id == ScreenId::StorageLogs || id == ScreenId::System ||
         id == ScreenId::Diagnostics || id == ScreenId::Firmware ||
         id == ScreenId::ConfirmRestart || id == ScreenId::ConfirmEraseHistory;
}

bool isProfileChild(ScreenId id) {
  return id == ScreenId::ProfileDetail || id == ScreenId::ProfileQuick ||
         id == ScreenId::ProfileSteps || id == ScreenId::ProfileStepControl ||
         id == ScreenId::ProfileStepSafety ||
         id == ScreenId::ProfileStepExit || id == ScreenId::ProfilePreview ||
         id == ScreenId::ProfileNameKeyboard ||
         id == ScreenId::ConfirmQuickConvert;
}

}  // namespace

ScreenManager::ScreenManager(HmiDisplay& display)
    : display_(display), canvas_(&display) {}

bool ScreenManager::begin() {
  canvas_.setColorDepth(16);
  canvas_.setPsram(true);
  const bool spriteReady =
      canvas_.createSprite(UiTheme::SCREEN_W, UiTheme::SCREEN_H) != nullptr;
  const bool chartReady = chart_.begin();
  const bool historyChartReady = historyChart_.begin();
  Serial.printf("[HMI chart] samples=%u memory=%s\n",
                static_cast<unsigned int>(chart_.capacity()),
                chart_.usingPsram() ? "PSRAM" : "DRAM fallback");
  return spriteReady && chartReady && historyChartReady;
}

void ScreenManager::show(ScreenId screen) {
  if (screen_ == screen) return;
  if (screen == ScreenId::Settings && !isSettingsChild(screen_))
    settingsReturn_ = screen_;
  if (screen != ScreenId::Ready) readyMenuOpen_ = false;
  screen_ = screen;
  if(screen == ScreenId::HeaterSettings){heaterPage_=0;heaterEditor_=HeaterSettings::Editor{};}
  if (screen == ScreenId::ThermalCal) {
    thermalCalLoaded_ = thermalCalDirty_ = thermalCalConflict_ = false;
    thermalCalPendingId_ = 0;
    thermalCalMessage_ = "READING...";
  }
  if (screen == ScreenId::PressureCal) {
    pressureCalStep_ = PressureCalStep::Overview;
    pressureCalPendingAction_ = UiAction::None;
    pressureCalMessage_ = nullptr;
  }
  lastDrawMs_ = 0;
  if (screen == ScreenId::Brew) resetChart();
}

void ScreenManager::beginProfileLoad(uint8_t index) {
  selectedProfileIndex_ = index;
  profileDraft_ = ProfileDraft{};
  show(ScreenId::ProfileDetail);
}

void ScreenManager::loadProfileDraft(const ProfileDetailData& source) {
  profileDraft_.load(source);
  selectedProfileIndex_ = source.profile.profileIndex;
  lastDrawMs_ = 0;
}

void ScreenManager::markProfileSynced(uint8_t runtimeIndex) {
  profileDraft_.markSynced(runtimeIndex);
  selectedProfileIndex_ = runtimeIndex;
  lastDrawMs_ = 0;
}

ScreenId ScreenManager::handleTap(int x, int y, uint32_t nowMs) {
  (void)nowMs;
  ScreenId next = screen_;
  if (isSettingsChild(screen_) && x < 52 && y < UiTheme::HEADER_H) {
    next = (screen_ == ScreenId::PressureCal || screen_ == ScreenId::ThermalCal) ? ScreenId::Calibration
           : screen_ == ScreenId::Networks ? ScreenId::Wifi
           : (screen_ == ScreenId::Wifi || screen_ == ScreenId::Scale ||
              screen_ == ScreenId::WebAccess) ? ScreenId::Connections
           : (screen_ == ScreenId::Diagnostics || screen_ == ScreenId::StorageLogs ||
              screen_ == ScreenId::Firmware || screen_ == ScreenId::HeaterSettings) ? ScreenId::System
           : screen_ == ScreenId::ConfirmRestart ? ScreenId::System
           : screen_ == ScreenId::ConfirmEraseHistory ? ScreenId::StorageLogs
                                                 : ScreenId::Settings;
  } else if (screen_ == ScreenId::WifiKeyboard) {
    if (y < UiTheme::HEADER_H && x < 52) {
      next = ScreenId::Networks;
    } else if (y >= 207 && x < 72) {
      next = ScreenId::Networks;
    } else {
      handleKeyboardTap(x, y);
    }
  } else if (screen_ == ScreenId::ProfileNameKeyboard) {
    if ((y < UiTheme::HEADER_H && x < 52) || (y >= 207 && x < 72)) {
      next = ScreenId::ProfileDetail;
    } else if (y >= 207 && x >= 200) {
      profileDraft_.setName(profileNameEdit_);
      next = ScreenId::ProfileDetail;
    } else {
      handleProfileNameKeyboardTap(x, y);
    }
  } else if (screen_ == ScreenId::Settings && x < 52 &&
             y < UiTheme::HEADER_H) {
    next = settingsReturn_;
  } else if ((screen_ == ScreenId::History || screen_ == ScreenId::Compare) &&
             x < 52 && y < UiTheme::HEADER_H) {
    next = screen_ == ScreenId::Compare ? ScreenId::ShotResult
                                        : ScreenId::Ready;
  } else if (screen_ == ScreenId::ShotDetail && x < 52 &&
             y < UiTheme::HEADER_H) {
    next = ScreenId::History;
  } else if (screen_ == ScreenId::SavedCompare && x < 52 &&
             y < UiTheme::HEADER_H) {
    next = ScreenId::ShotDetail;
  } else if (isProfileChild(screen_) && x < 52 &&
             y < UiTheme::HEADER_H) {
    next = (screen_ == ScreenId::ProfileDetail) ? ScreenId::Profiles
           : (screen_ == ScreenId::ProfileStepControl ||
              screen_ == ScreenId::ProfileStepSafety ||
              screen_ == ScreenId::ProfileStepExit)
               ? ScreenId::ProfileSteps
               : ScreenId::ProfileDetail;
  } else if (screen_ == ScreenId::Profiles && x < 52 &&
             y < UiTheme::HEADER_H) {
    next = ScreenId::Ready;
  } else if (screen_ == ScreenId::Ready) {
    if (readyMenuOpen_) {
      const auto target = ReadyLayout::menuTargetAt(x, y);
      if (target == ReadyLayout::Target::Settings) next = ScreenId::Settings;
      else if (target == ReadyLayout::Target::History) next = ScreenId::History;
      else { readyMenuOpen_ = false; lastDrawMs_ = 0; }
    } else {
      const auto target = ReadyLayout::targetAt(x, y);
      if (target == ReadyLayout::Target::Menu) {
        readyMenuOpen_ = true;
        lastDrawMs_ = 0;
      } else if (target == ReadyLayout::Target::Profile) next = ScreenId::Profiles;
      // Operating actions are sent by the caller; only Main telemetry may
      // change the live screen. A disabled tap never simulates pump activity.
#if HMI_DEMO_MODE
      else if (target == ReadyLayout::Target::Start) next = ScreenId::Brew;
      else if (target == ReadyLayout::Target::Flush) next = ScreenId::Flush;
#endif
    }
  } else if (screen_ == ScreenId::Profiles && y >= 210) {

    const uint8_t pageCount =
        max<uint8_t>(1, (visibleProfileCount_ + 3) / 4);
    if (x < 82) {
      // Profile creation moved to Web.
    } else if (x < 145 && profilePage_ > 0) {
      --profilePage_;
    } else if (x >= 235 && profilePage_ + 1 < pageCount) {
      ++profilePage_;
    }
  } else if (screen_ == ScreenId::ProfileDetail) {
    // Read/select only; advanced editing is owned by Web.
  } else if (screen_ == ScreenId::ConfirmQuickConvert && y >= 184) {
    if (x < 155) {
      next = ScreenId::ProfileDetail;
    } else if (x >= 165) {
      profileDraft_.generateQuickSteps();
      quickPage_ = 0;
      next = ScreenId::ProfileQuick;
    }
  } else if (screen_ == ScreenId::ProfileQuick) {
    const int rowIndex = y >= 43 && y < 193 ? (y - 43) / 48 : -1;
    if (rowIndex >= 0 && rowIndex < 3 && x >= 218) {
      const bool increase = x >= 268;
      const float direction = increase ? 1.0f : -1.0f;
      if (quickPage_ == 0) {
        const ProfileGlobalField fields[] = {
            ProfileGlobalField::Preinfusion, ProfileGlobalField::BloomTime,
            ProfileGlobalField::MainPressure};
        profileDraft_.adjustGlobal(fields[rowIndex],
                                   direction * (rowIndex == 1 ? 0.5f : 0.1f));
      } else {
        const ProfileGlobalField fields[] = {
            ProfileGlobalField::TargetWeight, ProfileGlobalField::MaxShotTime,
            ProfileGlobalField::FlowLimit};
        const float increments[] = {0.5f, 1.0f, 0.1f};
        profileDraft_.adjustGlobal(fields[rowIndex],
                                   direction * increments[rowIndex]);
      }
    } else if (y >= 207) {
      if (quickPage_ == 0) {
        if (x < 120) next = ScreenId::ProfileDetail;
        else if (x >= 200) quickPage_ = 1;
      } else {
        if (x < 105) quickPage_ = 0;
        else if (x < 215) next = ScreenId::ProfilePreview;
        else next = ScreenId::ProfileDetail;
      }
    }
  } else if (screen_ == ScreenId::ProfileSteps) {
    if (y >= 42 && y < 202) {
      const uint8_t index = stepPage_ * 4 + (y - 42) / 40;
      if (profileDraft_.valid() && index < profileDraft_.data().profile.stepCount) {
        selectedStepIndex_ = index;
        next = ScreenId::ProfileStepControl;
      }
    } else if (y >= 207) {
      const uint8_t pageCount = profileDraft_.valid()
          ? max<uint8_t>(1, (profileDraft_.data().profile.stepCount + 3) / 4)
          : 1;
      if (x < 105) profileDraft_.addStep();
      else if (x < 210 && stepPage_ > 0) --stepPage_;
      else if (x >= 210 && stepPage_ + 1 < pageCount) ++stepPage_;
    }
  } else if (screen_ == ScreenId::ProfileStepControl) {
    if (y >= 43 && y < 88) profileDraft_.cycleStepMode(selectedStepIndex_);
    else if (y >= 88 && y < 205 && x >= 218) {
      const int rowIndex = (y - 88) / 39;
      const float direction = x >= 268 ? 1.0f : -1.0f;
      const ProfileStepField fields[] = {ProfileStepField::Target,
                                         ProfileStepField::Ramp,
                                         ProfileStepField::MaxTime};
      const float increments[] = {0.1f, 0.1f, 0.5f};
      if (rowIndex < 3)
        profileDraft_.adjustStep(selectedStepIndex_, fields[rowIndex],
                                 direction * increments[rowIndex]);
    } else if (y >= 211) {
      if (x >= 107 && x < 214) next = ScreenId::ProfileStepSafety;
      else if (x >= 214) next = ScreenId::ProfileStepExit;
    }
  } else if (screen_ == ScreenId::ProfileStepSafety) {
    if (y >= 54 && y < 104) profileDraft_.cycleBumper(selectedStepIndex_);
    else if (y >= 108 && y < 158 && x >= 218 && profileDraft_.valid() &&
             profileDraft_.data().steps[selectedStepIndex_].bumperType != 0)
      profileDraft_.adjustStep(selectedStepIndex_, ProfileStepField::BumperValue,
                               x >= 268 ? 0.1f : -0.1f);
    else if (y >= 164 && y < 205) {
      if (x < 106) {
        if (profileDraft_.moveStep(selectedStepIndex_, -1) &&
            selectedStepIndex_ > 0)
          --selectedStepIndex_;
      } else if (x < 213) {
        if (profileDraft_.deleteStep(selectedStepIndex_)) {
          if (selectedStepIndex_ >= profileDraft_.data().profile.stepCount)
            selectedStepIndex_ = profileDraft_.data().profile.stepCount - 1;
          next = ScreenId::ProfileSteps;
        }
      } else if (profileDraft_.moveStep(selectedStepIndex_, 1)) {
        ++selectedStepIndex_;
      }
    }
    else if (y >= 211) {
      if (x < 107) next = ScreenId::ProfileStepControl;
      else if (x >= 214) next = ScreenId::ProfileStepExit;
    }
  } else if (screen_ == ScreenId::ProfileStepExit) {
    if (y >= 42 && y < 177 && x >= 218) {
      const int rowIndex = (y - 42) / 45;
      const uint8_t fieldIndex = exitPage_ * 3 + rowIndex;
      if (fieldIndex < 6) {
        const float increments[] = {0.1f, 0.1f, 0.1f, 0.1f, 0.5f, 0.5f};
        profileDraft_.adjustExit(
            selectedStepIndex_, static_cast<ProfileExitField>(fieldIndex),
            (x >= 268 ? 1.0f : -1.0f) * increments[fieldIndex]);
      }
    } else if (y >= 180 && y < 210) {
      if (x < 120 && exitPage_ > 0) --exitPage_;
      else if (x >= 200 && exitPage_ == 0) ++exitPage_;
    } else if (y >= 211) {
      if (x < 107) next = ScreenId::ProfileStepControl;
      else if (x < 214) next = ScreenId::ProfileStepSafety;
    }
  } else if (screen_ == ScreenId::History && y >= 216) {
    const uint8_t pageCount = max<uint8_t>(1, (visibleHistoryCount_ + 3) / 4);
    if (ResultLayout::Prev.contains(x, y) && historyPage_ > 0) --historyPage_;
    else if (ResultLayout::Next.contains(x, y) && historyPage_ + 1 < pageCount) ++historyPage_;
  } else if (screen_ == ScreenId::Flush && ReadyLayout::StopFlush.contains(x, y)) {
#if HMI_DEMO_MODE
    next = ScreenId::Ready;
#endif
  } else if (screen_ == ScreenId::Brew && BrewLayout::Stop.contains(x, y)) {
#if HMI_DEMO_MODE
    next = ScreenId::ShotResult;
#endif
  } else if (screen_ == ScreenId::ShotResult && ResultLayout::Home.contains(x, y)) {
#if HMI_DEMO_MODE
    next = ScreenId::Ready;
#endif
  } else if (screen_ == ScreenId::ShotDetail && ResultLayout::Back.contains(x, y)) {
    next = ScreenId::History;
  } else if (screen_ == ScreenId::Settings) {
    const int index = SettingsLayout::tileAt(x, y);
    const ScreenId pages[] = {ScreenId::Connections, ScreenId::Calibration,
                              ScreenId::DisplaySound, ScreenId::System};
    if (index >= 0) next = pages[index];
  } else if (screen_ == ScreenId::Calibration) {
    if (x >= 8 && x < 312 && y >= 52 && y < 110) next = ScreenId::PressureCal;
    if (x >= 8 && x < 312 && y >= 126 && y < 184) next = ScreenId::ThermalCal;
  } else if (screen_ == ScreenId::ThermalCal) {
    if (!thermalCalPendingId_ && x >= 8 && x < 148 && y >= 202 && y < 236) {
      thermalCalLoaded_ = thermalCalDirty_ = thermalCalConflict_ = false;
      thermalCalMessage_ = "READING...";
    } else if (thermalCalEditable_ && thermalCalLoaded_ && !thermalCalPendingId_ &&
               !thermalCalConflict_ && y >= 96 && y < 140) {
      if (x >= 8 && x < 88) thermalOffsetDraft_ = max(ThermalCalibration::MinOffsetC, thermalOffsetDraft_ - 0.1f);
      if (x >= 232 && x < 312) thermalOffsetDraft_ = min(ThermalCalibration::MaxOffsetC, thermalOffsetDraft_ + 0.1f);
      thermalCalDirty_ = fabsf(thermalOffsetDraft_ - thermalOffsetBase_) > 0.001f;
      thermalCalMessage_ = thermalCalDirty_ ? "UNSAVED" : nullptr;
    }
  } else if (screen_ == ScreenId::Connections) {
    const int index = SettingsLayout::connectionAt(x, y);
    const ScreenId pages[] = {ScreenId::Wifi, ScreenId::Scale, ScreenId::WebAccess};
    if (index >= 0) next = pages[index];
  } else if (screen_ == ScreenId::WebAccess && SettingsLayout::WebWifi.contains(x, y)) {
    next = ScreenId::Wifi;
  } else if (screen_ == ScreenId::Wifi && y >= 190 && x < 165) {
    next = ScreenId::Networks;
  } else if (screen_ == ScreenId::Networks && y >= 42 && y < 180) {
    const int index = (y - 42) / 48;
    if (index >= 0 && index < wifiNetworkCount_) selectedNetwork_ = index;
  } else if (screen_ == ScreenId::Networks && y >= 184 && x >= 160 &&
             selectedNetwork_ >= 0 && selectedNetwork_ < wifiNetworkCount_) {
    snprintf(wifiSsid_, sizeof(wifiSsid_), "%s",
             wifiNetworks_[selectedNetwork_].ssid);
    wifiPassword_[0] = '\0';
    keyboardSymbols_ = false;
    keyboardUpper_ = false;
    passwordVisible_ = true;
    next = ScreenId::WifiKeyboard;
  } else if (screen_ == ScreenId::HeaterSettings) {
    handleHeaterTap(x,y);
  } else if (screen_ == ScreenId::System) {
    const int index = SettingsLayout::systemAt(x, y);
    const ScreenId pages[] = {ScreenId::HeaterSettings, ScreenId::Diagnostics, ScreenId::StorageLogs,
                              ScreenId::ConfirmRestart};
    if (index >= 0) next = pages[index];
  } else if (screen_ == ScreenId::StorageLogs && y >= 202) {
    next = ScreenId::ConfirmEraseHistory;
  } else if (screen_ == ScreenId::Scale) {
    if (x < 52 && y < UiTheme::HEADER_H) next = ScreenId::Connections;
    else if (y >= 180 && x < 160) {
      selectedScaleDevice_ = -1;
      next = ScreenId::ScaleDevices;
    }
  } else if (screen_ == ScreenId::ScaleDevices) {
    if (x < 52 && y < UiTheme::HEADER_H) next = ScreenId::Scale;
    else if (y >= 42 && y < 186) {
      const int index = (y - 42) / 36;
      if (index >= 0 && index < scaleDeviceCount_) selectedScaleDevice_ = index;
    } else if (y >= 184 && x >= 160 && selectedScaleDevice_ >= 0) {
      next = ScreenId::Scale;
    }
  } else if (screen_ == ScreenId::ConfirmRestart && y >= 184 && x < 155) {
    next = ScreenId::System;
  } else if (screen_ == ScreenId::ConfirmEraseHistory && y >= 184 && x < 155) {
    next = ScreenId::StorageLogs;
  } else if (screen_ == ScreenId::PressureCal) {
    if (pressureCalStep_ == PressureCalStep::Overview && y >= 186) {
      pressureCalStep_ = PressureCalStep::Zero;
    } else if (pressureCalStep_ == PressureCalStep::Reference) {
      if (y >= 118 && y < 162) {
        if (x < 108)
          pressureCalibrationReferenceBar_ =
              max(0.5f, pressureCalibrationReferenceBar_ - 0.1f);
        else if (x >= 212)
          pressureCalibrationReferenceBar_ =
              min(16.0f, pressureCalibrationReferenceBar_ + 0.1f);
      }
    } else if (pressureCalStep_ == PressureCalStep::Review && y >= 190) {
      next = ScreenId::Settings;
    }
  }
  show(next);
  return screen_;
}

UiAction ScreenManager::actionForTap(int x, int y) const {
  if (screen_ == ScreenId::ThermalCal) {
    if (thermalCalEditable_ && thermalCalLoaded_ && thermalCalDirty_ &&
        !thermalCalPendingId_ && !thermalCalConflict_ &&
        x >= 172 && x < 312 && y >= 202 && y < 236)
      return UiAction::SaveThermalOffset;
    return UiAction::None;
  }
  if(screen_==ScreenId::HeaterSettings){
    if(x>=8 && x<88 && y>=40 && y<70)return UiAction::HeaterOff;
    if(heaterBusy_ || !heaterFresh_)return UiAction::None;
    if(heaterApplyAllowed_ && heaterPage_==0 && y>=150 && y<184){if(x>=8 && x<154)return UiAction::HeaterEnable;if(x>=166 && x<312)return UiAction::HeaterClear;}
    if(x>=172 && x<312 && y>=203 && y<235 && heaterApplyAllowed_ && heaterEditor_.dirty && heaterEditor_.valid())return UiAction::HeaterSave;
    return UiAction::None;
  }
  if(screen_==ScreenId::YieldEditor) {
    if(!yieldActionsEnabled_)return UiAction::None;
    if(ReadyLayout::YieldApply.contains(x,y))return UiAction::ApplyYieldEdit;
    if(ReadyLayout::YieldCancel.contains(x,y))return UiAction::CancelYieldEdit;
    return UiAction::None;
  }
  if (screen_ == ScreenId::Ready) {
    if (readyMenuOpen_) {
      return ReadyLayout::menuTargetAt(x, y) == ReadyLayout::Target::History
                 ? UiAction::ShowHistory : UiAction::None;
    }
    const auto target = ReadyLayout::enabledTarget(ReadyLayout::targetAt(x, y),
        readyStartEnabled_, readyManualEnabled_, readyControlsEnabled_);
    if (target == ReadyLayout::Target::Start)
      return UiAction::StartBrew;
    if (target == ReadyLayout::Target::Manual)
      return UiAction::SelectManual;
    if (target == ReadyLayout::Target::Flush)
      return UiAction::StartFlush;
    if (target == ReadyLayout::Target::Yield)
      return readyYieldEnabled_ ? UiAction::BeginYieldEdit : UiAction::None;
  } else if (screen_ == ScreenId::Flush && ReadyLayout::StopFlush.contains(x, y)) {
    return UiAction::Stop;
  } else if (screen_ == ScreenId::Brew && BrewLayout::Stop.contains(x, y)) {
    return UiAction::Stop;
  } else if (screen_ == ScreenId::ShotResult) {
    if (ResultLayout::History.contains(x, y)) return UiAction::ShowHistory;
    if (ResultLayout::Home.contains(x, y)) return UiAction::DismissResult;
  } else if (screen_ == ScreenId::Profiles && profileIndexForTap(x, y) >= 0) {
    return UiAction::OpenProfile;
  } else if (screen_ == ScreenId::ProfileDetail && y >= 164 && y < 201 &&
             x >= 213 && selectedProfileIndex_ != 0xFF) {
    return UiAction::SelectProfile;
  } else if ((false) ||
             (screen_ == ScreenId::ProfilePreview && y >= 198)) {
    return profileDraft_.dirty() && profileDraft_.validate() == nullptr
               ? UiAction::UploadProfile
               : UiAction::None;
  } else if (screen_ == ScreenId::Wifi && y >= 190 && x >= 165) {
    return UiAction::ForgetWifi;
  } else if (screen_ == ScreenId::Wifi && y >= 124 && y < 176) {
    return UiAction::ToggleWifiAutoConnect;
  } else if (screen_ == ScreenId::Networks && y >= 184 && x < 160) {
    return UiAction::RescanWifi;
  } else if (screen_ == ScreenId::WifiKeyboard && y >= 207 && x >= 200) {
    return UiAction::ConnectWifi;
  } else if (screen_ == ScreenId::DisplaySound) {
    if (y >= 42 && y < 83) return UiAction::ChangeBrightness;
    if (y >= 84 && y < 125) return UiAction::ChangeAutoDim;
    if (y >= 126 && y < 167) return UiAction::ToggleTouchSounds;
    if (y >= 168 && y < 209) return UiAction::ChangeVolume;
    if (y >= 210) return UiAction::TestSound;
  } else if (screen_ == ScreenId::StorageLogs && y >= 94 && y < 128) {
    return UiAction::ToggleAutoSave;
  } else if (screen_ == ScreenId::History && historyIndexForTap(x, y) >= 0) {
    return UiAction::LoadHistoryShot;
  } else if (screen_ == ScreenId::Scale && y >= 180) {
    return (x < 160) ? UiAction::ScanScale : UiAction::UnpairScale;
  } else if (screen_ == ScreenId::ScaleDevices && y >= 180) {
    return (x < 160) ? UiAction::RescanScales
                     : (selectedScaleDevice_ >= 0 ? UiAction::PairScale : UiAction::None);
  } else if (screen_ == ScreenId::PressureCal) {
    if (pressureCalPendingAction_ != UiAction::None) return UiAction::None;
    if (pressureCalStep_ == PressureCalStep::Zero && y >= 190)
      return UiAction::ZeroPressure;
    if (pressureCalStep_ == PressureCalStep::Reference) {
      if (y >= 172 && y < 202)
        return pressureCalTestRunning_ ? UiAction::Stop
                                       : UiAction::StartPressureCalibration;
      if (y >= 204 && pressureCalTestRunning_)
        return UiAction::SetPressureAt;
    }
  } else if (screen_ == ScreenId::ConfirmRestart && y >= 184 && x >= 165) {
    return UiAction::RestartHmi;
  } else if (screen_ == ScreenId::ConfirmEraseHistory && y >= 184 && x >= 165) {
    return UiAction::EraseHistory;
  }
  return UiAction::None;
}

void ScreenManager::markThermalCalibrationRequest(uint16_t requestId, uint32_t nowMs) {
  thermalCalPendingId_ = requestId;
  thermalCalRequestedAt_ = nowMs;
  thermalCalMessage_ = requestId ? "SAVING..." : "LINK BUSY - TRY AGAIN";
  lastDrawMs_ = 0;
}

void ScreenManager::applyThermalCalibrationReply(const HmiProtocol::CalibrationReplyPayload& reply) {
  if (screen_ != ScreenId::ThermalCal) return;
  using Status = HmiProtocol::CalibrationStatus;
  if (thermalCalPendingId_) {
    if (reply.requestId != thermalCalPendingId_ ||
        reply.operation != HmiProtocol::CalibrationOperation::SetThermalOffset) return;
    thermalCalPendingId_ = 0;
    if (reply.status == Status::Accepted) {
      thermalCalDirty_ = thermalCalConflict_ = false;
      thermalCalMessage_ = "SAVED";
    } else {
      thermalCalMessage_ = reply.status == Status::StorageError ? "SAVE FAILED" :
          reply.status == Status::Conflict ? "CHANGED - RELOAD" : "MAIN BUSY / INVALID";
      thermalCalConflict_ = reply.status == Status::Conflict;
      lastDrawMs_ = 0;
      return;
    }
  }
  if (reply.status != Status::Accepted || !ThermalCalibration::valid(reply.thermalOffsetC)) return;
  if (thermalCalLoaded_ && thermalCalDirty_ && fabsf(reply.thermalOffsetC - thermalOffsetBase_) > 0.001f) {
    thermalCalConflict_ = true;
    thermalCalMessage_ = "CHANGED - RELOAD";
  } else if (!thermalCalDirty_) {
    thermalOffsetBase_ = thermalOffsetDraft_ = reply.thermalOffsetC;
    if (!thermalCalLoaded_) thermalCalMessage_ = nullptr;
    thermalCalLoaded_ = true;
  }
  lastDrawMs_ = 0;
}

void ScreenManager::markPressureCalibrationRequest(UiAction action) {
  pressureCalPendingAction_ = action;
  pressureCalMessage_ = "SAVING...";
  lastDrawMs_ = 0;
}

void ScreenManager::applyPressureCalibrationReply(
    HmiProtocol::CalibrationOperation operation,
    HmiProtocol::CalibrationStatus status) {
  const UiAction expected = operation == HmiProtocol::CalibrationOperation::ZeroPressure
                                ? UiAction::ZeroPressure
                                : operation == HmiProtocol::CalibrationOperation::SetPressureAt
                                      ? UiAction::SetPressureAt
                                      : UiAction::None;
  if (expected == UiAction::None || expected != pressureCalPendingAction_)
    return;

  pressureCalPendingAction_ = UiAction::None;
  if (status == HmiProtocol::CalibrationStatus::Accepted) {
    pressureCalMessage_ = "SAVED";
    pressureCalStep_ = expected == UiAction::ZeroPressure
                           ? PressureCalStep::Reference
                           : PressureCalStep::Review;
  } else if (status == HmiProtocol::CalibrationStatus::RejectedState) {
    pressureCalMessage_ = "MAIN BUSY";
  } else if (status == HmiProtocol::CalibrationStatus::SensorError) {
    pressureCalMessage_ = "SENSOR ERROR";
  } else if (status == HmiProtocol::CalibrationStatus::StorageError) {
    pressureCalMessage_ = "SAVE FAILED";
  } else {
    pressureCalMessage_ = "INVALID VALUE";
  }
  lastDrawMs_ = 0;
}



uint8_t ScreenManager::sliderValueForTap(int x) const {
  return static_cast<uint8_t>(constrain((x - 145) * 100 / 105, 10, 100));
}

int ScreenManager::profileIndexForTap(int x, int y) const {
  if (screen_ != ScreenId::Profiles || x < 7 || x > 313 || y < 42 ||
      y >= 202)
    return -1;
  const int rowIndex = (y - 42) / 40;
  const int index = static_cast<int>(profilePage_) * 4 + rowIndex;
  return index >= 0 && index < visibleProfileCount_ ? index : -1;
}

int ScreenManager::historyIndexForTap(int x, int y) const {
  if (screen_ != ScreenId::History) return -1;
  const int rowIndex = ResultLayout::rowAt(x, y);
  if (rowIndex < 0) return -1;
  const int index = static_cast<int>(historyPage_) * 4 + rowIndex;
  return index >= 0 && index < visibleHistoryCount_ ? index : -1;
}

void ScreenManager::update(const UiTelemetry& telemetry, uint32_t nowMs) {
  thermalCalEditable_ = telemetry.mainConnected && telemetry.mainCompatible &&
      (telemetry.machineState == HmiProtocol::MachineState::Ready ||
       telemetry.machineState == HmiProtocol::MachineState::Done);
  if (thermalCalPendingId_ && nowMs - thermalCalRequestedAt_ > 1800) {
    thermalCalPendingId_ = 0;
    thermalCalConflict_ = true;
    thermalCalMessage_ = "TIMEOUT - RELOAD";
  }
  visibleProfileCount_ = telemetry.profileCount;
  visibleHistoryCount_ = telemetry.recentShotCount;
  wifiNetworks_ = telemetry.wifiNetworks;
  wifiNetworkCount_ = telemetry.wifiNetworkCount;
  wifiScanning_ = telemetry.wifiScanning;
  scaleDevices_ = telemetry.scaleDiscoveredDevices;
  scaleDeviceCount_ = telemetry.scaleDiscoveredCount;
  scaleScanning_ = telemetry.scaleScanning;
  const bool wasPressureCalTestRunning = pressureCalTestRunning_;
  pressureCalTestRunning_ =
      telemetry.machineState == HmiProtocol::MachineState::Calibration;
  if (pressureCalStep_ == PressureCalStep::Reference &&
      wasPressureCalTestRunning && !pressureCalTestRunning_ &&
      pressureCalPendingAction_ == UiAction::None)
    pressureCalMessage_ = "TEST STOPPED";
  appendChartSample(telemetry);
  appendReadyTemperatureSample(telemetry, nowMs);
  const uint32_t interval =
      (screen_ == ScreenId::Ready && readyHeartbeatActive_) ? 100 :
      (screen_ == ScreenId::Brew || screen_ == ScreenId::Flush ||
       screen_ == ScreenId::Profiles || isProfileChild(screen_))
           ? 80
           : 200;
  if (lastDrawMs_ != 0 && nowMs - lastDrawMs_ < interval) return;
  lastDrawMs_ = nowMs;
  canvas_.fillSprite(UiTheme::BG);
  switch (screen_) {
    case ScreenId::Ready: drawReady(telemetry, nowMs); break;
    case ScreenId::YieldEditor: drawYieldEditor(telemetry); break;
    case ScreenId::Flush: drawFlush(telemetry); break;
    case ScreenId::Brew: drawBrew(telemetry); break;
    case ScreenId::ShotResult: drawResult(telemetry); break;
    case ScreenId::Settings: drawSettings(telemetry); break;
    case ScreenId::Connections: drawConnections(telemetry); break;
    case ScreenId::WebAccess: drawWebAccess(telemetry); break;
    case ScreenId::Wifi: drawWifi(telemetry); break;
    case ScreenId::Networks: drawNetworks(); break;
    case ScreenId::WifiKeyboard: drawWifiKeyboard(); break;
    case ScreenId::ConfirmRestart: drawRestartConfirm(); break;
    case ScreenId::Scale: drawScale(telemetry); break;
    case ScreenId::ScaleDevices: drawScaleDevices(telemetry); break;
    case ScreenId::Calibration: drawCalibration(); break;
    case ScreenId::ThermalCal: drawThermalCal(telemetry); break;
    case ScreenId::PressureCal: drawPressureCal(telemetry); break;

    case ScreenId::Machine: drawMachine(); break;
    case ScreenId::DisplaySound: drawDisplaySound(telemetry); break;
    case ScreenId::StorageLogs: drawStorageLogs(telemetry); break;
    case ScreenId::System: drawSystem(telemetry); break;
    case ScreenId::HeaterSettings: drawHeaterSettings(telemetry); break;
    case ScreenId::Diagnostics: drawDiagnostics(telemetry); break;
    case ScreenId::Firmware: drawFirmware(telemetry); break;
    case ScreenId::Profiles: drawProfiles(telemetry); break;
    case ScreenId::ProfileDetail: drawProfileDetail(telemetry); break;
    case ScreenId::ProfileQuick: drawProfileQuick(); break;
    case ScreenId::ProfileSteps: drawProfileSteps(); break;
    case ScreenId::ProfileStepControl: drawProfileStepControl(); break;
    case ScreenId::ProfileStepSafety: drawProfileStepSafety(); break;
    case ScreenId::ProfileStepExit: drawProfileStepExit(); break;
    case ScreenId::ProfilePreview: drawProfilePreview(telemetry); break;
    case ScreenId::ProfileNameKeyboard: drawProfileNameKeyboard(); break;
    case ScreenId::ConfirmQuickConvert: drawQuickConvertConfirm(); break;
    case ScreenId::History: drawHistory(telemetry); break;
    case ScreenId::ShotDetail: drawShotDetail(telemetry); break;
    case ScreenId::SavedCompare: drawSavedCompare(telemetry); break;
    case ScreenId::ConfirmEraseHistory: drawEraseHistoryConfirm(); break;
    case ScreenId::Compare: drawCompare(telemetry); break;
  }
  // Compose the warning before the single display transfer so the page cannot
  // repeatedly erase a separately drawn alarm between refreshes.
  if (telemetry.heaterAlarm || telemetry.heaterPaused) {
    const uint16_t bg = telemetry.heaterAlarm ? 0xB800 : 0xFFE0;
    const bool brew = screen_ == ScreenId::Brew;
    canvas_.fillRect(0, 0, brew ? 240 : UiTheme::SCREEN_W, brew ? 60 : 42, bg);
    canvas_.setTextColor(telemetry.heaterAlarm ? 0xFFFF : 0x0000, bg);
    canvas_.setTextFont(1);
    canvas_.setTextSize(1);
    canvas_.setCursor(5, 5);
    canvas_.print(!telemetry.heaterAlarm ? "HEATER PAUSED - WAITING SENSOR" :
        telemetry.heaterFresh ? "HEATER FAULT - CUT MAINS POWER" : "MAIN LOST - HEATER STATE UNKNOWN");
    canvas_.setCursor(5, 23); canvas_.print("Open /heater for details");
    if (brew) { canvas_.setCursor(5, 43); canvas_.print("Use machine switch if link lost"); }

  }
  canvas_.pushSprite(0, 0);
}
