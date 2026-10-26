// Physical encoder input, independent of TFT rendering and menus.
#include "state_machine.h"
#include "src/app/orchestrator.h"
#include "src/app/quick_yield.h"
#include "src/control/profile_engine.h"
#include "src/hardware/encoder.h"
#include "config/hardware_pins.h"
#include "src/recovery/recovery_service.h"

namespace StateMachine {
void update() {
  const State state = Orchestrator::getState();
  if (state == IDLE) {
    const int step = Enc::readSteps(!QuickYield::editing());
    if (QuickYield::editing()) QuickYield::rotate(step);
    else if (step && profilesCount>0)
      setCurrentProfileIndex((getCurrentProfileIndex()+step%profilesCount+profilesCount)%profilesCount);
  } else if (state == CALIBRATION_RUN) {
    const int step = Enc::readSteps();
    if (step) Orchestrator::adjustPressureCalibrationPower(step);
  } else {
    Enc::readSteps(); // Discard rotation outside the active input context.
  }
  // Keep the existing input order: Manual/hot-water consume in controlPumpTick
  // before this call; unused steps (including during tare preparation) are drained.

  static bool swPressed = false;
  static uint32_t swDown = 0;
  static uint32_t lastClickMs = 0;
  static bool pendingClick = false;
  static State pressedState = IDLE;
  const uint32_t now = millis();
  const bool pressed = digitalRead(ENC_SW) == LOW;
  if (QuickYield::editing()) { pendingClick=false;swPressed=false;return; }
  if (state != IDLE) pendingClick = false;
  if (pressed && !swPressed) {
    swPressed = true;
    swDown = now;
    pressedState = state;
  }
  if (!pressed && swPressed) {
    const uint32_t duration = now - swDown;
    swPressed = false;
    // A press begun during a shot must not become an idle Recovery gesture.
    if (state != pressedState) { pendingClick = false; return; }
    if (duration < 600) {
      if (state == IDLE) {
        if (pendingClick && now - lastClickMs <= 400U) {
          pendingClick = false;
          RecoveryService::toggle();
        } else {
          pendingClick = true;
          lastClickMs = now;
        }
      } else if (state == SHOT_END) {
        if (now - lastClickMs > 700U) Orchestrator::forceIdle();
        lastClickMs = now;
      }
    } else {
      pendingClick = false;
      if (state == IDLE && duration >= 5000U) RecoveryService::toggle();
      else if (state == CALIBRATION_RUN)
        Orchestrator::stopPressureCalibration("encoder-hold");
      else if (state == SHOT_END) Orchestrator::forceIdle();
      // Brew/hot-water stop belongs to HMI or the machine's physical switch.
    }
  }
  if (pendingClick && now - lastClickMs > 400U) pendingClick = false;
}
} // namespace StateMachine
