// File: src/hardware/ads1115_interface.h
#ifndef ADS1115_INTERFACE_H
#define ADS1115_INTERFACE_H

#include <Arduino.h>

namespace ADS1115_Interface {
    struct PressureSnapshot {
        float rawBar = 0;
        float sensorFilteredBar = 0;
        float sensorRateBarPerS = 0;
        uint32_t sampledAtMs = 0;
        bool valid = false;
    };
    PressureSnapshot getPressureSnapshot();
    void init();
    // Compatibility no-op: dedicated ADC owner. Measured-board mode polls
    // 3 ms and publishes mean7 at 30 ms; capture preserves individual raw.
    // Acquisition A/B selection: ads1115_acquisition.h; calibration unchanged.
    void updateFast();
    float getAveragedVoltage();     // Trung bình 10 mẫu cho màn calibration
    float getVoltage();             // Đọc giá trị tức thời (từ buffer)
}

#endif // ADS1115_INTERFACE_H
