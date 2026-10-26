#include "PressureController.h"
#include "SimpleKalmanFilter/SimpleKalmanFilter.h"
#include <Arduino.h> // ESP_LOGI (reset())
#include <algorithm>
#include <math.h>

// Helper function to return the sign of a float
inline float sign(float x) { return (x > 0.0f) - (x < 0.0f); }

// Static utility function for first-order low-pass filtering
void PressureController::applyLowPassFilter(float *filteredValue, float rawValue, float cutoffFreq, float dt) {
    if (filteredValue == nullptr)
        return;

    float alpha = dt / (1.0f / (2.0f * M_PI * cutoffFreq) + dt);
    *filteredValue = alpha * rawValue + (1.0f - alpha) * (*filteredValue);
}

PressureController::PressureController(float dt, float *rawPressureSetpoint, float *rawFlowSetpoint, float *sensorOutput,
                                       float *controllerOutput, int *valveStatus) {
    this->_rawPressureSetpoint = rawPressureSetpoint;
    this->_rawFlowSetpoint = rawFlowSetpoint;
    this->_rawPressure = sensorOutput;
    this->_ctrlOutput = controllerOutput;
    this->_valveStatus = valveStatus;
    this->_dt = dt;
    this->_pressureKalmanFilter = new SimpleKalmanFilter(0.1f, 10.0f, powf(4 * _dt, 2));
    this->_previousPressure = *sensorOutput;
}

void PressureController::filterSetpoint(float rawSetpoint) {
    if (!_setpointFilterInitialized)
        initSetpointFilter();
    float omega = 2.0 * M_PI * _setpointFilterFreq;
    float d2r =
        (omega * omega) * (rawSetpoint - _filteredSetpoint) - 2.0f * _setpointFilterDamping * omega * _filteredSetpointDerivative;
    _filteredSetpointDerivative += std::clamp(d2r * _dt, -_maxPressureRate, _maxPressureRate);
    _filteredSetpoint += _filteredSetpointDerivative * _dt;
}

void PressureController::initSetpointFilter(float val) {
    _filteredSetpoint = *_rawPressureSetpoint;
    if (val != 0.0f)
        _filteredSetpoint = val;
    _filteredSetpointDerivative = 0.0f;
    _setpointFilterInitialized = true;
}

void PressureController::filterSensor() {
    // Use Kalman filter for pressure (as originally intended)
    float newFiltered = this->_pressureKalmanFilter->updateEstimate(*_rawPressure);

    // Calculate pressure derivative using the filtered pressure
    float pressureDerivative = (newFiltered - _lastFilteredPressure) / _dt;
    applyLowPassFilter(&_filteredPressureDerivative, pressureDerivative, _filterEstimatorFrequency, _dt);

    _lastFilteredPressure = newFiltered;
    _filteredPressureSensor = newFiltered;
}

void PressureController::update(ControlMode mode) {
    filterSetpoint(*_rawPressureSetpoint);
    filterSensor();

    if ((mode == ControlMode::FLOW || mode == ControlMode::PRESSURE) && *_rawPressureSetpoint > 0.0f &&
        *_rawFlowSetpoint > 0.0f) {
        float flowOutput = getPumpDutyCycleForFlowRate();
        float pressureOutput = getPumpDutyCycleForPressure();
        *_ctrlOutput = std::min(flowOutput, pressureOutput);
        if (flowOutput < pressureOutput) {
            _errorIntegral = 0.0f; // Reset error buildup in flow target
        }
    } else if (mode == ControlMode::FLOW) {
        *_ctrlOutput = getPumpDutyCycleForFlowRate();
    } else if (mode == ControlMode::PRESSURE) {
        *_ctrlOutput = getPumpDutyCycleForPressure();
    }
    virtualScale();
}

float PressureController::pumpFlowModel(float alpha) const {
    // Positive-displacement model: net = duty * Q_geo - slip (affine in duty).
    // With slip = 0 this reduces to the previous proportional model.
    const float duty = alpha / 100.0f;
    const float slip = getSlip();
    return std::max(0.0f, duty * getGeometricFlow() - slip);
}

float PressureController::getAvailableFlow() const {
    const float P = _filteredPressureSensor;
    const float P2 = P * P;
    const float P3 = P2 * P;
    const float Q =
        _pumpFlowCoefficients[0] * P3 + _pumpFlowCoefficients[1] * P2 + _pumpFlowCoefficients[2] * P + _pumpFlowCoefficients[3];

    return Q;
}

float PressureController::getSlip() const {
    const float P = _filteredPressureSensor;
    const float P2 = P * P;
    const float P3 = P2 * P;
    const float slip =
        _pumpSlipCoefficients[0] * P3 + _pumpSlipCoefficients[1] * P2 + _pumpSlipCoefficients[2] * P + _pumpSlipCoefficients[3];
    return std::max(0.0f, slip); // leakage is never negative
}

// Full-drive curve is the duty=1 slice (Q_geo - slip), so Q_geo = full-drive + slip.
float PressureController::getGeometricFlow() const { return getAvailableFlow() + getSlip(); }

float PressureController::getPumpDutyCycleForFlowRate() const {
    if (*_rawFlowSetpoint <= 0.0f) {
        return 0.0f;
    }
    const float geometricFlow = getGeometricFlow();
    if (geometricFlow <= 0.0f) {
        return 0.0f;
    }
    // Feedforward duty to hit the target flow, accounting for slip (incl. Q_t = 0 hold duty).
    float duty = ((*_rawFlowSetpoint + getSlip()) / geometricFlow) * 100.0f;
    return std::clamp(duty, 0.0f, 100.0f);
}

void PressureController::setPumpFlowCoeff(float oneBarFlow, float nineBarFlow) {
    // Set the affine pump flow model coefficients based on flow measurement at 1 bar and 9 bar
    _pumpFlowCoefficients[0] = 0.0f;
    _pumpFlowCoefficients[1] = 0.0f;
    _pumpFlowCoefficients[2] = (nineBarFlow - oneBarFlow) / 8;
    _pumpFlowCoefficients[3] = oneBarFlow - _pumpFlowCoefficients[2] * 1.0f;
}

void PressureController::setPumpFlowPolyCoeffs(float a, float b, float c, float d) {
    _pumpFlowCoefficients[0] = a;
    _pumpFlowCoefficients[1] = b;
    _pumpFlowCoefficients[2] = c;
    _pumpFlowCoefficients[3] = d;
}

void PressureController::setPumpSlipPolyCoeffs(float a, float b, float c, float d) {
    _pumpSlipCoefficients[0] = a;
    _pumpSlipCoefficients[1] = b;
    _pumpSlipCoefficients[2] = c;
    _pumpSlipCoefficients[3] = d;
}

void PressureController::setGains(float commutationGain, float convergenceGain, float integralGain) {
    _commutationGain = commutationGain;
    _convergenceGain = convergenceGain;
    _integralGain = integralGain;
}

void PressureController::tare() {
    _coffeeOutput = 0.0f;
    _pumpVolume = 0.0f;
    _puckSaturationVolume = 0.0f;
    _puckState[0] = false;
    _puckState[1] = false;
    _puckState[2] = false;
    _puckCounter = 0;
    _pumpFlowRate = 0.0f;
    // IIR-filtered accumulators: unreset, these carry a shot's end-of-brew
    // depressurization transient into the next shot's puck-conductance-
    // derivative arming signature, suppressing arming — GH #839.
    _filteredPressureDerivative = 0.0f;
    _waterThroughPuckFlowRate = 0.0f;
    _lastPuckConductance = 0.0f;
    _puckConductance = 0.0f;
    _puckConductanceDerivative = 0.0f;
    _coffeeFlowRate = 0.0f;
    _puckResistance = INFINITY;
}

void PressureController::virtualScale() {
    float newPumpFlowRate = pumpFlowModel(*_ctrlOutput);
    applyLowPassFilter(&_pumpFlowRate, newPumpFlowRate, _filterEstimatorFrequency, _dt);
    _pumpVolume += _pumpFlowRate * _dt;
    applyLowPassFilter(&exportPumpFlowRate, newPumpFlowRate, _filterEstimatorFrequency / 2, _dt);

    // Raw entering water puck flow
    float effectiveCompliance = 3.0f / fmax(0.2f, _filteredPressureSensor); // ml*s/bar
    float flowRaw = _pumpFlowRate - effectiveCompliance * _filteredPressureDerivative;

    applyLowPassFilter(&_waterThroughPuckFlowRate, flowRaw, 0.3f, _dt);
    if (_waterThroughPuckFlowRate > 0.0f && *_valveStatus == 1 && _filteredPressureSensor > 0.8f) {
        _puckCounter++;
        _puckSaturationVolume += _waterThroughPuckFlowRate * _dt;

        // PUCK CONDUCTANCE
        _puckConductance = _waterThroughPuckFlowRate / sqrtf(_filteredPressureSensor);
        if (_puckCounter <= 1)
            _lastPuckConductance = _puckConductance;
        float newPuckConductanceDerivative = (_puckConductance - _lastPuckConductance) / _dt;
        applyLowPassFilter(&_puckConductanceDerivative, newPuckConductanceDerivative, 0.2f, _dt);
        _lastPuckConductance = _puckConductance;

        if (_puckConductanceDerivative < -0.5f && float(_puckCounter) * _dt > 1.0f)
            _puckState[0] = true;

        int timeStamp = 0;
        if (_puckState[0] && _puckConductanceDerivative > -0.1f && !_puckState[1]) {
            _puckState[1] = true;
            timeStamp = _puckCounter;
        }

        _puckResistance = 1.0 / _puckConductance;
        if (_puckState[1]) {
            if (_puckCounter == timeStamp) {
                _coffeeFlowRate = _waterThroughPuckFlowRate;
                _puckResistance = 1.0f / _puckConductance;
            }
            applyLowPassFilter(&_puckResistance, 1.0f / _puckConductance, 0.1f, _dt);
            applyLowPassFilter(&_coffeeFlowRate, _waterThroughPuckFlowRate, 0.2f, _dt);
            if (!_puckState[2]) {
                float timeMissedDrops = 2.0f;
                float missedDops = _coffeeFlowRate * timeMissedDrops / 2;
                _puckState[2] = true;
                _coffeeOutput += _coffeeFlowRate * _dt + missedDops;
            } else {
                _coffeeOutput += _coffeeFlowRate * _dt;
            }
        }
    }
}

float PressureController::getPumpDutyCycleForPressure() {
    if (*_rawPressureSetpoint < 0.2f) {
        initSetpointFilter();
        _errorIntegral = 0.0f;
        *_ctrlOutput = 0.0f;
        _previousPressure = 0.0f;
        return 0.0f;
    }

    float P = _filteredPressureSensor;
    float P_ref = _filteredSetpoint;
    float error = P - P_ref;
    _previousPressure = P;

    float epsilon = _epsilonCoefficient * _filteredSetpoint;
    float deadband = _deadbandCoefficient * _filteredSetpoint;

    float s = _convergenceGain * error;
    float sat_s = 0.0f;
    if (error > 0) {
        float tan = tanhf(s / epsilon - deadband * _convergenceGain / epsilon);
        sat_s = std::max(0.0f, tan);
    } else if (error < 0) {
        float tan = tanhf(s / epsilon + deadband * _convergenceGain / epsilon);
        sat_s = std::min(0.0f, tan);
    }

    float pressureRatio = 0;
    if (P < _maxPressure) {
        pressureRatio = P / _maxPressure;
    }
    float denominator = fmaxf(1.0f - pressureRatio, 0.0001f);
    float Ki = _integralGain / denominator;
    _errorIntegral += error * _dt;
    float iterm = Ki * _errorIntegral;

    float Qa = getGeometricFlow();
    Qa = fmaxf(Qa, 1e-3f);
    float Ceq = _systemCompliance;
    float K = _commutationGain / denominator * Qa / Ceq;
    _pumpDutyCycle = Ceq / Qa * (-_convergenceGain * error - K * sat_s) - iterm;

    if ((sign(error) == -sign(_pumpDutyCycle)) && (fabs(_pumpDutyCycle) > 1.0f)) {
        _errorIntegral -= error * _dt;
        iterm = Ki * _errorIntegral;
    }

    _pumpDutyCycle = Ceq / Qa * (-_convergenceGain * error - K * sat_s) - iterm;
    return std::clamp(_pumpDutyCycle * 100.0f, 0.0f, 100.0f);
}

void PressureController::reset() {
    initSetpointFilter(_filteredPressureSensor);
    _errorIntegral = 0.0f;
    _pumpFlowRate = 0.0f;
    _puckSaturationVolume = 0.0f;
    _puckState[0] = false;
    _puckState[1] = false;
    _puckState[2] = false;
    _puckCounter = 0;
    ESP_LOGI("", "RESET");
}
