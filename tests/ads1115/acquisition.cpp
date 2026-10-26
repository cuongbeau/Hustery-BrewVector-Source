#include "src/hardware/ads1115_acquisition.h"

struct FakeAdc {
    bool ready=false;
    int reads=0, checks=0, starts=0, publishes=0;
    int16_t value=1234, published=0;
    bool ordered=true;
    constexpr bool conversionComplete(){++checks;return ready;}
    constexpr int16_t getLastConversionResults(){++reads;return value;}
    constexpr void startADCReading(uint16_t mux,bool continuous){
        ordered=ordered && mux==7 && !continuous && publishes==reads;
        ++starts;ready=false;
    }
};
struct Publish {
    FakeAdc& adc;
    constexpr void operator()(int16_t raw)const{
        ++adc.publishes;adc.published=raw;
    }
};
constexpr bool singleShotLifecycle(){
    FakeAdc adc;
    // The initial conversion has not completed: no read, publish or restart.
    if(Ads1115Acquisition::poll<true>(adc,7,Publish{adc}))return false;
    if(adc.reads || adc.publishes || adc.starts)return false;
    adc.ready=true;
    if(!Ads1115Acquisition::poll<true>(adc,7,Publish{adc}))return false;
    if(adc.reads!=1 || adc.published!=1234 || adc.starts!=1)return false;
    // Busy polls must not publish the old result or starve a conversion.
    for(int i=0;i<3;++i)
        if(Ads1115Acquisition::poll<true>(adc,7,Publish{adc}))return false;
    if(adc.reads!=1 || adc.starts!=1)return false;
    adc.ready=true;adc.value=-123;
    Ads1115Acquisition::poll<true>(adc,7,Publish{adc});
    return adc.ordered && adc.reads==2 && adc.publishes==2 &&
        adc.starts==2 && adc.published==-123;
}
constexpr bool legacyContinuous(){
    FakeAdc adc;
    Ads1115Acquisition::poll<false>(adc,7,Publish{adc});
    Ads1115Acquisition::poll<false>(adc,7,Publish{adc});
    return adc.reads==2 && adc.publishes==2 && !adc.checks && !adc.starts;
}
static_assert(singleShotLifecycle(),"Wait-ready, publish once, then rearm without spinning");
static_assert(legacyContinuous(),"Continuous A/B branch keeps previous latest-result reads");
constexpr bool feedbackContracts() {
    Ads1115Acquisition::FeedbackMean mean;
    if (mean.add(-16, 3000) != -16 || mean.add(16, 6000) != 0) return false;
    mean.reset();
    for (int i=0;i<7;++i) mean.add(0, (i+1)*3000);
    // A step is fully settled within six further intervals, not 7 controller ticks.
    for (int i=0;i<6;++i) {
        const float value = mean.add(700, (i+8)*3000);
        if (value != (i+1)*100) return false;
    }
    if (mean.add(700, 42000) != 700) return false;
    if (mean.add(-32768, 51000) != -32768 || mean.count != 1) return false;
    mean.reset();
    for (int i=0;i<20;++i) if (mean.add(32767, i*3000) != 32767) return false;
    mean.reset(); mean.add(16, 0xfffffff0U);
    if (mean.add(32, 2984) != 24 || mean.count != 2) return false;
    mean.reset(); mean.add(0, 3000);
    return mean.add(1,6000) == 0.5f; // Do not quantize fractional averaged counts.
}
static_assert(feedbackContracts(), "Feedback DC/step, signed range, gap reset, wrap and fractional counts");
static_assert(Ads1115Acquisition::singleShot == (ADS1115_SINGLE_SHOT_ACQUISITION==1),
              "Build switch selects expected mode");
