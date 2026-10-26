// File: src/hardware/ads1115_interface.cpp
#include "ads1115_interface.h"
#include "ads1115_acquisition.h"
#include "pressure_capture.h"
#include "pressure_capture_policy.h"
#include "adc_probe.h"
#include "valve_sense.h"
#include "src/communication/hmi_link.h"
#include "src/recovery/recovery_portal.h"
#include "pump_control.h"
#include "src/app/orchestrator.h"
#include "config/hardware_pins.h"
#include <Adafruit_ADS1X15.h>
#include <Wire.h>
#include <TwoStateKalmanFilter/TwoStateKalmanFilter.h>
#include "interfaces/calibration_types.h"

namespace ADS1115_Interface {
    static Adafruit_ADS1115 ads;

    static constexpr uint32_t ADC_READ_INTERVAL_MS = 30;
    static constexpr int BUFFER_SIZE = 10;
    static float buffer[BUFFER_SIZE] = {0};
    static int bufferIndex = 0;
    static bool isReady = false;
    static bool hasSample = false;
    static TaskHandle_t taskHandle = nullptr;
    static portMUX_TYPE sampleMux = portMUX_INITIALIZER_UNLOCKED;
    // Pressure-sensor filter parameters; one update per ADC task sample.
    static TwoStateKalmanFilter pressureFilter(0.030f, 0.01f, 0.5f, 0.95f);
    static PressureSnapshot pressureSnapshot;
    static float previousOffset = NAN, previousScale = NAN;
    static PressureCapture::PublishClock publishClock;
    static Ads1115Acquisition::FeedbackMean feedbackMean;

    static void publishReading(float raw) {
        const float offset = gPress.offset;
        const float scale = gPress.scale;
        if (offset != previousOffset || scale != previousScale) {
            pressureFilter.reset();
            previousOffset = offset;
            previousScale = scale;
        }
        PressureSnapshot pressure;
        pressure.sampledAtMs = millis();
        const float calibrated = (raw * (4.096f / 32768.0f) - offset) * scale;
        pressure.valid = isfinite(calibrated) && isfinite(scale) && scale > 0;
        if (pressure.valid) {
            pressure.rawBar = max(0.0f, calibrated);
            // Feed signed calibrated pressure to the filter, as upstream does.
            pressure.sensorFilteredBar = constrain(pressureFilter.updateEstimate(calibrated), 0.0f, 16.0f);
            pressure.sensorRateBarPerS = pressureFilter.getRateEstimate();
        } else {
            pressureFilter.reset();
        }
        portENTER_CRITICAL(&sampleMux);
        pressureSnapshot = pressure;
        if (!hasSample) {
            for (int i = 0; i < BUFFER_SIZE; ++i) buffer[i] = raw;
            hasSample = true;
        } else {
            buffer[bufferIndex] = raw;
        }
        bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
        portEXIT_CRITICAL(&sampleMux);
    }

    static void sampleOnce() {
        if (!isReady) return;
        Ads1115Acquisition::poll<Ads1115Acquisition::singleShot>(
            ads, ADS1X15_REG_CONFIG_MUX_SINGLE_0, publishReading);
    }

    static void sampleTask(void*) {
        TickType_t lastWake = xTaskGetTickCount();
        bool wasFast = false;
        while (true) {
            const State state = Orchestrator::getState();
            const bool probeSafe = state == IDLE && !ValveSense::isMachineValveActive() &&
                digitalRead(CURRENT_SENSE_PIN) == HIGH && digitalRead(HOTWATER_SENSE_PIN) == HIGH &&
                !HmiLink::firmwareUpdateActive() && !RecoveryPortal::updateActive();
            if (AdcProbe::tick(probeSafe, [](int16_t raw) { publishReading(raw); })) {
                feedbackMean.reset();
                lastWake = xTaskGetTickCount();
                publishClock.lastUs = micros();
                vTaskDelay(pdMS_TO_TICKS(AdcProbe::engaged() ? 1 :
                    (Ads1115Acquisition::measuredFast ? Ads1115Acquisition::pollMs : ADC_READ_INTERVAL_MS)));
                continue;
            }
            const bool fast = PressureCapture::tick(
                state == PROFILE_RUN, state == IDLE, millis(), micros());
            if (Ads1115Acquisition::measuredFast) {
                // Capture only records; arming/stopping it does not change the
                // production acquisition cadence or restart a conversion.
                const bool got = Ads1115Acquisition::poll<true>(ads,
                    ADS1X15_REG_CONFIG_MUX_SINGLE_0, [fast](int16_t raw) {
                        const uint32_t nowUs = micros();
                        const float averaged = feedbackMean.add(raw, nowUs);
                        const bool publish = publishClock.due(nowUs);
                        if (publish) publishReading(averaged);
                        if (fast) {
                            const auto owner = PumpPSM::getControllerTelemetry();
                            PressureCapture::record(raw, publish, nowUs,
                                owner.controllerFilteredPressureBar, owner.appliedPowerPercent);
                        }
                    });
                if (!got && fast) PressureCapture::busy();
            } else if (fast != wasFast) {
                // Discard the pending conversion across a timing transition.
                // Its age is unknown; begin a fresh single shot instead.
                ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0, false);
                lastWake = xTaskGetTickCount();
            } else if (fast) {
                const bool got = Ads1115Acquisition::poll<true>(ads,
                    ADS1X15_REG_CONFIG_MUX_SINGLE_0, [](int16_t raw) {
                        const uint32_t nowUs = micros();
                        const bool publish = publishClock.due(nowUs);
                        if (publish) publishReading(raw); // Never publish meanRaw.
                        const auto owner = PumpPSM::getControllerTelemetry();
                        PressureCapture::record(raw, publish, nowUs,
                            owner.controllerFilteredPressureBar, owner.appliedPowerPercent);
                    });
                if (!got) PressureCapture::busy();
            } else {
                sampleOnce();
                publishClock.lastUs = micros();
            }
            wasFast = fast;
            const TickType_t period = pdMS_TO_TICKS(Ads1115Acquisition::measuredFast
                ? Ads1115Acquisition::pollMs : fast
                ? PressureCaptureWire::POLL_MS : ADC_READ_INTERVAL_MS);
            // If delayed, skip missed task releases instead of burst-reading.
            if ((fast || Ads1115Acquisition::measuredFast) && xTaskGetTickCount() - lastWake >= period) lastWake = xTaskGetTickCount();
            xTaskDelayUntil(&lastWake, period);
        }
    }

    void init() {
        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
        Wire.setClock(400000); // 400kHz Fast Mode

        if (!ads.begin(0x48, &Wire)) {
            Serial.println("[ADS1115] FAIL! Vui lòng kiểm tra kết nối I2C.");
        } else {
            Serial.println("[ADS1115] OK");
            // Keep gain and ADC-to-voltage scaling unchanged for stored pressure
            // calibration. This ports acquisition timing, not the upstream PGA.
            ads.setGain(GAIN_ONE); // +/- 4.096V
            ads.setDataRate(Ads1115Acquisition::singleShot
                ? RATE_ADS1115_860SPS : RATE_ADS1115_250SPS);
            ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0,
                                !Ads1115Acquisition::singleShot);
            Serial.println(Ads1115Acquisition::measuredFast
                ? "[ADC] measured board: rate code 7 (~3300 SPS behavior), poll 3 ms, mean7 -> publish 30 ms"
                : Ads1115Acquisition::singleShot
                ? "[ADS1115] A/B: single-shot 860 SPS, task 30 ms, gain +/-4.096 V"
                : "[ADS1115] A/B: continuous 250 SPS, task 30 ms, gain +/-4.096 V");
            isReady = true;
            BaseType_t created = xTaskCreate(
                sampleTask, "ADSAdc::loop", configMINIMAL_STACK_SIZE * 4,
                nullptr, 1, &taskHandle);
            if (created != pdPASS) {
                taskHandle = nullptr;
                isReady = false;
                Serial.println("[ADS1115] FAIL creating sample task");
            }
        }
    }

    void updateFast() {
        // Sampling no longer depends on Web/HMI/main-loop latency.
    }

    PressureSnapshot getPressureSnapshot() {
        portENTER_CRITICAL(&sampleMux);
        const PressureSnapshot result = pressureSnapshot;
        portEXIT_CRITICAL(&sampleMux);
        return result;
    }

    float getAveragedVoltage() {
        if (!isReady) return 0.0f;

        float snapshot[BUFFER_SIZE];
        portENTER_CRITICAL(&sampleMux);
        for (int i = 0; i < BUFFER_SIZE; ++i) snapshot[i] = buffer[i];
        const bool valid = hasSample;
        portEXIT_CRITICAL(&sampleMux);
        if (!valid) return 0.0f;

        float sum = 0;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            sum += snapshot[i];
        }
        
        float avgRaw = (float)sum / BUFFER_SIZE;
        
        // Chuyển đổi raw sang Voltage: GAIN_ONE (+/- 4.096V) -> 1 bit = 0.125mV = 4.096V / 32768
        return avgRaw * (4.096f / 32768.0f);
    }

    float getVoltage() {
        if (!isReady) return 0.0f;
        float raw = 0;
        bool valid = false;
        portENTER_CRITICAL(&sampleMux);
        int lastIndex = (bufferIndex == 0) ? (BUFFER_SIZE - 1) : (bufferIndex - 1);
        raw = buffer[lastIndex];
        valid = hasSample;
        portEXIT_CRITICAL(&sampleMux);
        if (!valid) return 0.0f;
        return raw * (4.096f / 32768.0f);
    }
}
