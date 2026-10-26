#pragma once

namespace PumpFlowCalibration {

constexpr float ReferenceQ0MlPerS = 10.79f;
constexpr float ReferenceSlopeMlPerSPerBar = -0.5854f;

constexpr float interpolate(float q1MlPerS, float q9MlPerS,
                            float pressureBar) {
  return q1MlPerS + (q9MlPerS - q1MlPerS) * (pressureBar - 1.0f) / 8.0f;
}

constexpr float legacyScale(float mlPerClick) {
  return (50.0f * mlPerClick) / ReferenceQ0MlPerS;
}

constexpr float legacyQ1(float mlPerClick) {
  return (ReferenceQ0MlPerS + ReferenceSlopeMlPerSPerBar) *
         legacyScale(mlPerClick);
}

constexpr float legacyQ9(float mlPerClick) {
  return (ReferenceQ0MlPerS + 9.0f * ReferenceSlopeMlPerSPerBar) *
         legacyScale(mlPerClick);
}

}  // namespace PumpFlowCalibration
