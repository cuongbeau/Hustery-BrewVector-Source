#include "PSM.h"

PSM* _thePSM;

// ESP32 tasks may run on a different core from the GPIO ISR. Disabling only
// the caller's interrupts is insufficient for the manual stopped-state reset.
static portMUX_TYPE psmStateMux = portMUX_INITIALIZER_UNLOCKED;

PSM::PSM(unsigned char sensePin, unsigned char controlPin, unsigned int range, int mode, unsigned char divider, unsigned char interruptMinTimeDiff) {
  _thePSM = this;

  pinMode(sensePin, INPUT_PULLUP);
  PSM::_sensePin = sensePin;

  pinMode(controlPin, OUTPUT);
  PSM::_controlPin = controlPin;

  PSM::_divider = divider > 0 ? divider : 1;

  uint8_t interruptNum = digitalPinToInterrupt(PSM::_sensePin);

  if (interruptNum != NOT_AN_INTERRUPT) {
    attachInterrupt(interruptNum, onInterrupt, mode);
  }

  PSM::_range = range;
  PSM::_interruptMinTimeDiff = interruptMinTimeDiff;
}

void onPSMInterrupt() __attribute__((weak));
void onPSMInterrupt() {}

void PSM::onInterrupt() {
  portENTER_CRITICAL_ISR(&psmStateMux);
  _thePSM->handleInterrupt();
  portEXIT_CRITICAL_ISR(&psmStateMux);
}

void PSM::handleInterrupt() {
  const uint32_t nowUs = micros();
  _thePSM->_diagRawEdges++;
  if (_thePSM->_diagLastRawUs != 0) {
    const uint32_t rawDt = nowUs - _thePSM->_diagLastRawUs;
    if (rawDt < _thePSM->_diagRawDtMinUs) {
      _thePSM->_diagRawDtMinUs = rawDt;
    }
  }
  _thePSM->_diagLastRawUs = nowUs;

  if (_thePSM->_interruptMinTimeDiff > 0 && millis() - _thePSM->_lastMillis < _thePSM->_interruptMinTimeDiff) {
    _thePSM->_diagRejectedEdges++;
    return;
  }
  _thePSM->_lastMillis = millis();

  _thePSM->_diagAcceptedEdges++;
  if (_thePSM->_diagLastAcceptedUs != 0) {
    const uint32_t acceptedDt = nowUs - _thePSM->_diagLastAcceptedUs;
    if (acceptedDt < _thePSM->_diagAcceptedDtMinUs) {
      _thePSM->_diagAcceptedDtMinUs = acceptedDt;
    }
    if (acceptedDt > _thePSM->_diagAcceptedDtMaxUs) {
      _thePSM->_diagAcceptedDtMaxUs = acceptedDt;
    }
    _thePSM->_diagAcceptedDtSumUs += acceptedDt;
    _thePSM->_diagAcceptedDtSamples++;
    if (acceptedDt < 8000U) _thePSM->_diagAcceptedShortIntervals++;
    if (acceptedDt > 12000U) _thePSM->_diagAcceptedLongIntervals++;
  }
  _thePSM->_diagLastAcceptedUs = nowUs;

  onPSMInterrupt();

  if (_thePSM->_dividerCounter >= _thePSM->_divider - 1) {
    _thePSM->_dividerCounter -= _thePSM->_divider - 1;
    _thePSM->calculateSkip();
  } else {
    _thePSM->_dividerCounter++;
  }
}

void PSM::set(unsigned int value) {
  portENTER_CRITICAL(&psmStateMux);
  const unsigned int clamped = value < PSM::_range ? value : PSM::_range;
  if (!PSM::_diagValueInitialized) {
    PSM::_diagPreviousValue = clamped;
    PSM::_diagValueInitialized = true;
  } else if (clamped != PSM::_diagPreviousValue) {
    PSM::_diagPreviousValue = clamped;
    PSM::_diagValueChanges++;
  }
  if (clamped < PSM::_diagValueMin) PSM::_diagValueMin = clamped;
  if (clamped > PSM::_diagValueMax) PSM::_diagValueMax = clamped;

  if (value < PSM::_range) {
    PSM::_value = value;
  } else {
    PSM::_value = PSM::_range;
  }
  portEXIT_CRITICAL(&psmStateMux);
}

bool PSM::resetStoppedState() {
  portENTER_CRITICAL(&psmStateMux);
  // Require a completed OFF decision, not merely a queued zero command.
  if (_value != 0 || !_skip) {
    portEXIT_CRITICAL(&psmStateMux);
    return false;
  }
  digitalWrite(_controlPin, LOW);
  _a = 0;
  _dividerCounter = 1;  // Same seed as construction; keep the selected divider.
  _skip = true;
  // Keep debounce timestamps, click counter, stopAfter and diagnostic history.
  // No redetection, constructor, controller reset or automatic pump restart.
  portEXIT_CRITICAL(&psmStateMux);
  return true;
}

long PSM::getCounter() {
  return PSM::_counter;
}

void PSM::resetCounter() {
  PSM::_counter = 0;
}

void PSM::stopAfter(long counter) {
  PSM::_stopAfter = counter;
}

void PSM::calculateSkip() {
  PSM::_lastMillis = millis();

  PSM::_a += PSM::_value;

  if (PSM::_a >= PSM::_range) {
    PSM::_a -= PSM::_range;
    PSM::_skip = false;
  } else {
    PSM::_skip = true;
  }

  if (PSM::_a > PSM::_range) {
    PSM::_a = 0;
    PSM::_skip = false;
  }

  if (!PSM::_skip) {
    PSM::_counter++;
  }

  if (!PSM::_skip
    && PSM::_stopAfter > 0
    && PSM::_counter > PSM::_stopAfter) {
    PSM::_skip = true;
  }

  const uint32_t nowUs = micros();
  const bool gateState = !PSM::_skip;
  PSM::_diagDecisions++;
  if (gateState) PSM::_diagGateOnDecisions++;

  // Shift in the newest gate decision. A one-second window normally contains
  // 50 decisions after divider=2, so both words preserve the complete pattern.
  PSM::_diagPatternHigh =
      (PSM::_diagPatternHigh << 1) | (PSM::_diagPatternLow >> 31);
  PSM::_diagPatternLow = (PSM::_diagPatternLow << 1) | (gateState ? 1U : 0U);
  if (PSM::_diagPatternBits < 64) PSM::_diagPatternBits++;

  if (gateState) {
    if (PSM::_diagCurrentOnRun < UINT16_MAX) PSM::_diagCurrentOnRun++;
    PSM::_diagCurrentOffRun = 0;
    if (PSM::_diagCurrentOnRun > PSM::_diagMaxOnRun) {
      PSM::_diagMaxOnRun = PSM::_diagCurrentOnRun;
    }
  } else {
    if (PSM::_diagCurrentOffRun < UINT16_MAX) PSM::_diagCurrentOffRun++;
    PSM::_diagCurrentOnRun = 0;
    if (PSM::_diagCurrentOffRun > PSM::_diagMaxOffRun) {
      PSM::_diagMaxOffRun = PSM::_diagCurrentOffRun;
    }
  }

  if (!PSM::_diagGateInitialized) {
    PSM::_diagGateInitialized = true;
    PSM::_diagGateState = gateState;
    PSM::_diagLastGateTransitionUs = nowUs;
  } else if (gateState != PSM::_diagGateState) {
    const uint32_t durationUs = nowUs - PSM::_diagLastGateTransitionUs;
    if (PSM::_diagGateState) {
      if (durationUs < PSM::_diagGateHighMinUs)
        PSM::_diagGateHighMinUs = durationUs;
      if (durationUs > PSM::_diagGateHighMaxUs)
        PSM::_diagGateHighMaxUs = durationUs;
    } else {
      if (durationUs < PSM::_diagGateLowMinUs)
        PSM::_diagGateLowMinUs = durationUs;
      if (durationUs > PSM::_diagGateLowMaxUs)
        PSM::_diagGateLowMaxUs = durationUs;
    }
    PSM::_diagGateTransitions++;
    PSM::_diagGateState = gateState;
    PSM::_diagLastGateTransitionUs = nowUs;
  }

  updateControl();
}

void PSM::updateControl() {
  if (PSM::_skip) {
    digitalWrite(PSM::_controlPin, LOW);
  } else {
    digitalWrite(PSM::_controlPin, HIGH);
  }
}

unsigned int PSM::cps() {
  unsigned int range = PSM::_range;
  unsigned int value = PSM::_value;
  unsigned char divider = PSM::_divider;

  PSM::_range = 0xFFFF;
  PSM::_value = 1;
  PSM::_a = 0;
  PSM::_divider = 1;

  unsigned long stopAt = millis() + 1000;

  while (millis() < stopAt) {
    delay(0);
  }

  unsigned int result = PSM::_a;

  PSM::_range = range;
  PSM::_value = value;
  PSM::_a = 0;
  PSM::_divider = divider;

  return result;
}

unsigned long PSM::getLastMillis() {
  return PSM::_lastMillis;
}

unsigned char PSM::getDivider(void) {
  return PSM::_divider;
}

void PSM::setDivider(unsigned char divider) {
  PSM::_divider = divider > 0 ? divider : 1;
}

void PSM::shiftDividerCounter(char value) {
  PSM::_dividerCounter += value;
}

PSM::Diagnostics PSM::takeDiagnostics() {
  Diagnostics result;
  portENTER_CRITICAL(&psmStateMux);
  result.rawEdges = _diagRawEdges;
  result.acceptedEdges = _diagAcceptedEdges;
  result.rejectedEdges = _diagRejectedEdges;
  result.rawDtMinUs = _diagRawDtMinUs == UINT32_MAX ? 0 : _diagRawDtMinUs;
  result.acceptedDtMinUs =
      _diagAcceptedDtMinUs == UINT32_MAX ? 0 : _diagAcceptedDtMinUs;
  result.acceptedDtAverageUs =
      _diagAcceptedDtSamples > 0
          ? _diagAcceptedDtSumUs / _diagAcceptedDtSamples
          : 0;
  result.acceptedDtMaxUs = _diagAcceptedDtMaxUs;
  result.acceptedShortIntervals = _diagAcceptedShortIntervals;
  result.acceptedLongIntervals = _diagAcceptedLongIntervals;
  result.decisions = _diagDecisions;
  result.gateOnDecisions = _diagGateOnDecisions;
  result.gateTransitions = _diagGateTransitions;
  result.gateHighMinUs =
      _diagGateHighMinUs == UINT32_MAX ? 0 : _diagGateHighMinUs;
  result.gateHighMaxUs = _diagGateHighMaxUs;
  result.gateLowMinUs =
      _diagGateLowMinUs == UINT32_MAX ? 0 : _diagGateLowMinUs;
  result.gateLowMaxUs = _diagGateLowMaxUs;
  result.patternLow = _diagPatternLow;
  result.patternHigh = _diagPatternHigh;
  result.maxOnRun = _diagMaxOnRun;
  result.maxOffRun = _diagMaxOffRun;
  result.valueMin = static_cast<uint16_t>(
      _diagValueMin == UINT16_MAX ? _value : _diagValueMin);
  result.valueMax = static_cast<uint16_t>(
      _diagValueMin == UINT16_MAX ? _value : _diagValueMax);
  result.valueChanges = _diagValueChanges;
  result.patternBits = _diagPatternBits;

  _diagRawEdges = 0;
  _diagAcceptedEdges = 0;
  _diagRejectedEdges = 0;
  _diagRawDtMinUs = UINT32_MAX;
  _diagAcceptedDtMinUs = UINT32_MAX;
  _diagAcceptedDtMaxUs = 0;
  _diagAcceptedDtSumUs = 0;
  _diagAcceptedDtSamples = 0;
  _diagAcceptedShortIntervals = 0;
  _diagAcceptedLongIntervals = 0;
  _diagDecisions = 0;
  _diagGateOnDecisions = 0;
  _diagGateTransitions = 0;
  _diagGateHighMinUs = UINT32_MAX;
  _diagGateHighMaxUs = 0;
  _diagGateLowMinUs = UINT32_MAX;
  _diagGateLowMaxUs = 0;
  _diagPatternLow = 0;
  _diagPatternHigh = 0;
  _diagMaxOnRun = 0;
  _diagMaxOffRun = 0;
  _diagValueMin = _value;
  _diagValueMax = _value;
  _diagValueChanges = 0;
  _diagPatternBits = 0;
  portEXIT_CRITICAL(&psmStateMux);
  return result;
}
