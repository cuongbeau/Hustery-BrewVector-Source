// File: src/model/predictive_weight.cpp
// Predictive Weight: Detect thời điểm nước xuyên qua puck
// Dùng khi không có cân khả dụng từ HMI.

#include "predictive_weight.h"

PredictiveWeight::PredictiveWeight() :
  outputFlowStarted(false),
  puckResistance(0.f),
  previousPuckResistance(0.f),
  resistanceDelta(0.f),
  plateauTicks(0),
  learnedGateWaterG(20.0f),
  learnedGateTimeMs(0),
  learnedGateAvailable(false)
{}

bool PredictiveWeight::isOutputFlow() {
  return outputFlowStarted;
}

void PredictiveWeight::configureLearnedGate(float waterG, uint32_t timeMs,
                                             bool available) {
  learnedGateAvailable = available && isfinite(waterG) && waterG >= 8.0f &&
                         waterG <= 45.0f && timeMs >= 1500 && timeMs <= 30000;
  learnedGateWaterG = learnedGateAvailable ? waterG : 20.0f;
  learnedGateTimeMs = learnedGateAvailable ? timeMs : 0;
}

void PredictiveWeight::update(float pSmooth, float fSmooth, float pChange, float fChange, 
                               float waterPumped, uint32_t elapsedMs,
                               bool inSoak, float pTarget) {
  // Latch: một khi TRUE → TRUE vĩnh viễn cho đến reset()
  if (outputFlowStarted) {
    outputFlowStarted = true;
    return;
  }

  // A learned profile uses the previous scale-confirmed first-drip point. Both
  // volume and time carry a loose guard so a changed puck cannot trigger from
  // one noisy input alone.
  if (learnedGateAvailable) {
    const bool volumeReached = waterPumped >= learnedGateWaterG;
    const bool volumeGuard = waterPumped >= learnedGateWaterG * 0.75f;
    const bool timeReached = elapsedMs >= learnedGateTimeMs;
    const bool timeGuard = elapsedMs >= learnedGateTimeMs * 65U / 100U;
    if ((volumeReached && timeGuard) || (timeReached && volumeGuard)) {
      outputFlowStarted = true;
    }
    return;
  }

  // 1. Factory fallback for a profile that has no supervised observation yet.
  // The first valid scale shot will replace this with a per-profile threshold.
  //    (Trigger chính thường kích hoạt ở 8-15g,
  //     20g đủ margin mà không bỏ lỡ quá nhiều)
  if (waterPumped >= 20.0f) {
    outputFlowStarted = true;
    return;
  }

  // 2. Tính trở kháng puck
  //    Đơn vị: bar * 1000 / (g/s). Range thực tế: 1000-18000
  previousPuckResistance = puckResistance;
  puckResistance = pSmooth * 1000.f / (fSmooth > 0.001f ? fSmooth : 0.001f);
  resistanceDelta = puckResistance - previousPuckResistance;

  // 3. Bỏ qua nếu áp quá thấp hoặc mới bắt đầu
  if (pSmooth < 2.1f || waterPumped < 5.0f) return;

  // 4. Soak Detection: áp sụt mạnh = nước đã xuyên puck
  //    Chỉ kích hoạt khi đang ở phase áp thấp (pre-infusion)
  if (inSoak) {
    if (pTarget - pSmooth > 1.2f) {
      outputFlowStarted = true;
      return;
    }
  }

  // 5. Resistance Plateau (cơ chế chính, hoạt động với MỌI profile)
  //    Điều kiện: áp & flow ổn định + trở kháng không tăng mạnh
  //    Debounce: phải ổn định ít nhất 30 ticks (~500ms @ 60Hz)
  if (fabsf(pChange) < 5.0f && fabsf(fChange) < 2.0f) {
    if (puckResistance > 1100.0f && resistanceDelta < 500.0f) {
      plateauTicks++;
      if (plateauTicks >= 30) {  // ~500ms @ 60Hz
        outputFlowStarted = true;
      }
    } else {
      plateauTicks = 0;
    }
  } else {
    plateauTicks = 0;
  }
}

void PredictiveWeight::reset() {
  puckResistance = 0.f;
  previousPuckResistance = 0.f;
  resistanceDelta = 0.f;
  plateauTicks = 0;
  outputFlowStarted = false;
}
