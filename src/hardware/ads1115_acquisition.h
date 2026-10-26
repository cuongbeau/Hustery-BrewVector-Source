#pragma once
#include <stdint.h>

// Historical A/B switch (SPS labels apply to ADS1115 silicon): single-shot
// rate7 vs continuous rate5. Measured-board override below changes polling
// and pre-decimation filtering, not voltage scaling or rate register bits.
#ifndef ADS1115_SINGLE_SHOT_ACQUISITION
#define ADS1115_SINGLE_SHOT_ACQUISITION 1
#endif
#if ADS1115_SINGLE_SHOT_ACQUISITION != 0 && ADS1115_SINGLE_SHOT_ACQUISITION != 1
#error "ADS1115_SINGLE_SHOT_ACQUISITION must be 0 or 1"
#endif

namespace Ads1115Acquisition {
constexpr bool singleShot = ADS1115_SINGLE_SHOT_ACQUISITION == 1;

// Explicit board behavior override, not automatic chip identification. Probe
// 1766577798 matches ADS1015 timing. Keep ADS1115 raw register scaling (no
// right shift): a left-justified 12-bit register has the same voltage scale.
#ifndef ADC_MEASURED_FAST_ACQUISITION
#define ADC_MEASURED_FAST_ACQUISITION 1
#endif
#if ADC_MEASURED_FAST_ACQUISITION != 0 && ADC_MEASURED_FAST_ACQUISITION != 1
#error "ADC_MEASURED_FAST_ACQUISITION must be 0 or 1"
#endif
constexpr bool measuredFast = singleShot && ADC_MEASURED_FAST_ACQUISITION;
constexpr uint32_t pollMs = 3;
// Seven samples at 3 ms: ~21 ms averaging aperture, nominal group delay 9 ms.
// Filter BEFORE decimation. Reset across gaps to avoid blending stale pressure.
struct FeedbackMean {
    int16_t values[7]{};
    int32_t sum = 0;
    uint8_t count = 0, index = 0;
    uint32_t lastUs = 0;
    constexpr void reset() { *this = FeedbackMean{}; }
    constexpr float add(int16_t raw, uint32_t nowUs) {
        if (count && nowUs - lastUs > 6000U) reset();
        lastUs = nowUs;
        if (count == 7) sum -= values[index];
        else ++count;
        values[index] = raw; sum += raw; index = (index + 1) % 7;
        return static_cast<float>(sum) / count;
    }
};

// setup starts the first conversion. A busy conversion is left untouched;
// never wait for it or restart it on each poll. Publish before requesting
// the next conversion, following the single-channel acquisition sequence (single pressure channel).
template<bool SingleShot, typename Adc, typename Consumer>
constexpr bool poll(Adc& adc, uint16_t mux, Consumer consume) {
    if constexpr (SingleShot) {
        if (!adc.conversionComplete()) return false;
    }
    const int16_t raw = adc.getLastConversionResults();
    consume(raw);
    if constexpr (SingleShot) adc.startADCReading(mux, false);
    return true;
}
}
