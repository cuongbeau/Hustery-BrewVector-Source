#ifndef PSM_h
#define PSM_h

#include "Arduino.h"

class PSM
{
public:
  struct Diagnostics {
    uint32_t rawEdges = 0;
    uint32_t acceptedEdges = 0;
    uint32_t rejectedEdges = 0;
    uint32_t rawDtMinUs = 0;
    uint32_t acceptedDtMinUs = 0;
    uint32_t acceptedDtAverageUs = 0;
    uint32_t acceptedDtMaxUs = 0;
    uint32_t acceptedShortIntervals = 0;
    uint32_t acceptedLongIntervals = 0;
    uint32_t decisions = 0;
    uint32_t gateOnDecisions = 0;
    uint32_t gateTransitions = 0;
    uint32_t gateHighMinUs = 0;
    uint32_t gateHighMaxUs = 0;
    uint32_t gateLowMinUs = 0;
    uint32_t gateLowMaxUs = 0;
    uint32_t patternLow = 0;
    uint32_t patternHigh = 0;
    uint16_t maxOnRun = 0;
    uint16_t maxOffRun = 0;
    uint16_t valueMin = 0;
    uint16_t valueMax = 0;
    uint32_t valueChanges = 0;
    uint8_t patternBits = 0;
  };

  PSM(unsigned char sensePin, unsigned char controlPin, unsigned int range, int mode = RISING, unsigned char divider = 1, unsigned char interruptMinTimeDiff = 0);

  void set(unsigned int value);

  long getCounter();
  void resetCounter();

  void stopAfter(long counter);

  unsigned int cps();
  unsigned long getLastMillis();

  unsigned char getDivider(void);
  void setDivider(unsigned char divider = 1);
  void shiftDividerCounter(char value = 1);

  // Local diagnostic only; owner task, stopped pump.
  // Never called automatically at shot start/end.
  bool resetStoppedState();

  // Read and reset one diagnostics window. This is intentionally separate
  // from the PSM control path; collecting a snapshot never changes the
  // accumulator, divider, requested value or gate output.
  Diagnostics takeDiagnostics();

private:
  static inline void onInterrupt();
  void handleInterrupt();
  void calculateSkip();
  void updateControl();

  unsigned char _sensePin;
  unsigned char _controlPin;
  unsigned int _range;
  unsigned char _divider;
  unsigned char _dividerCounter = 1;
  unsigned char _interruptMinTimeDiff;
  volatile unsigned int _value = 0;
  volatile unsigned int _a = 0;
  volatile bool _skip = false;
  volatile long _counter = 0;
  volatile long _stopAfter = -1;
  volatile unsigned long _lastMillis = 0;

  volatile uint32_t _diagRawEdges = 0;
  volatile uint32_t _diagAcceptedEdges = 0;
  volatile uint32_t _diagRejectedEdges = 0;
  volatile uint32_t _diagLastRawUs = 0;
  volatile uint32_t _diagLastAcceptedUs = 0;
  volatile uint32_t _diagRawDtMinUs = UINT32_MAX;
  volatile uint32_t _diagAcceptedDtMinUs = UINT32_MAX;
  volatile uint32_t _diagAcceptedDtMaxUs = 0;
  volatile uint32_t _diagAcceptedDtSumUs = 0;
  volatile uint32_t _diagAcceptedDtSamples = 0;
  volatile uint32_t _diagAcceptedShortIntervals = 0;
  volatile uint32_t _diagAcceptedLongIntervals = 0;
  volatile uint32_t _diagDecisions = 0;
  volatile uint32_t _diagGateOnDecisions = 0;
  volatile uint32_t _diagGateTransitions = 0;
  volatile uint32_t _diagLastGateTransitionUs = 0;
  volatile uint32_t _diagGateHighMinUs = UINT32_MAX;
  volatile uint32_t _diagGateHighMaxUs = 0;
  volatile uint32_t _diagGateLowMinUs = UINT32_MAX;
  volatile uint32_t _diagGateLowMaxUs = 0;
  volatile uint32_t _diagPatternLow = 0;
  volatile uint32_t _diagPatternHigh = 0;
  volatile uint16_t _diagCurrentOnRun = 0;
  volatile uint16_t _diagCurrentOffRun = 0;
  volatile uint16_t _diagMaxOnRun = 0;
  volatile uint16_t _diagMaxOffRun = 0;
  volatile unsigned int _diagValueMin = UINT16_MAX;
  volatile unsigned int _diagValueMax = 0;
  volatile unsigned int _diagPreviousValue = 0;
  volatile uint32_t _diagValueChanges = 0;
  volatile uint8_t _diagPatternBits = 0;
  volatile bool _diagValueInitialized = false;
  volatile bool _diagGateInitialized = false;
  volatile bool _diagGateState = false;
};

extern PSM* _thePSM;

#endif
