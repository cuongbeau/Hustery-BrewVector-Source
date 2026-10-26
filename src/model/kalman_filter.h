// File: src/model/kalman_filter.h
// Di chuyển từ KalmanFilter.h gốc — giữ header-only
// Refactor: 2026-04-20

#ifndef SIMPLE_KALMAN_FILTER_H
#define SIMPLE_KALMAN_FILTER_H

/*
 * SimpleKalmanFilter - a Kalman Filter implementation for single variable models.
 * Based on the library by Denys Sene.
 */

class SimpleKalmanFilter {
public:
  SimpleKalmanFilter(float mea_e, float est_e, float q) {
    _err_measure = mea_e;
    _err_estimate = est_e;
    _q = q;
  }

  float updateEstimate(float measurement) {
    _kalman_gain = _err_estimate / (_err_estimate + _err_measure);
    _current_estimate = _last_estimate + _kalman_gain * (measurement - _last_estimate);
    _err_estimate = (1.0f - _kalman_gain) * _err_estimate + fabsf(_last_estimate - _current_estimate) * _q;
    _last_estimate = _current_estimate;

    return _current_estimate;
  }

  void setMeasurementError(float mea_e) {
    _err_measure = mea_e;
  }

  void setEstimateError(float est_e) {
    _err_estimate = est_e;
  }

  void setProcessNoise(float q) {
    _q = q;
  }

  float getKalmanGain() {
    return _kalman_gain;
  }

  float getEstimateError() {
    return _err_estimate;
  }

private:
  float _err_measure;
  float _err_estimate;
  float _q;
  float _current_estimate = 0.0f;
  float _last_estimate = 0.0f;
  float _kalman_gain = 0.0f;
};

#endif
