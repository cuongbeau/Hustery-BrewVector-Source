// File: interfaces/shot_types.h
// Di chuyển từ root → interfaces/ (giữ nguyên nội dung)
// Refactor: 2026-04-20

#ifndef SHOT_TYPES_H
#define SHOT_TYPES_H

#include <Arduino.h>

// ======================= DỮ LIỆU SHOT =======================
// ShotData: model runtime của 1 shot (giá trị tức thời + thống kê).
struct ShotData {
  // --- Giá trị tức thời (update mỗi vòng) ---
  float dt = 0.010f;          // Thời gian thực giữa 2 nhịp loop (s)
  float temperature = 0;
  float blockTempC = 0;       // Nhiệt đo thật tại thân thermoblock
  float pathTempEstC = 0;     // Ước lượng nhiệt đoạn PTFE/van/group compact
  float brewTempEstC = 0;     // Ước lượng nhiệt nước tại họng pha
  float thermalLoadG = 0;     // Tải nước gần đây dùng cho thermal estimator
  float pressure = 0;         // Mẫu ADS mới nhất: raw trace + controller input
  float smoothedPressure = 0;
  float pressureChangeSpeed = 0; // bar/s
  float pumpPower = 0;       // % (hiển thị)
  float cps = 0;             // nhịp thực tế (0-50)
  float targetPressure = 0;  // bar
  float flow_gps = 0;        // g/s
  float smoothedPumpFlow = 0;
  float pumpFlowChangeSpeed = 0; // g/s^2

  // --- Giá trị hiển thị (Visual Smoothing - Đã lọc mạnh để đồ thị phẳng) ---
  float visualPressure = 0;      // bar
  float visualFlow = 0;          // g/s

  String profileName = "OFF";

  // --- Cờ & thời gian shot ---
  bool puckWeak = false;
  bool shotTooLong = false;
  unsigned long shotTime = 0;

  // --- Thống kê tích lũy ---
  float maxPressure = 0;
  float tempMax = -1e9;
  float tempMin = +1e9;
  float tempAvg = 0;
  float flowAvg = 0;
  float outGrams = 0;        // gram đã ra: ưu tiên dùng cân nếu có, fallback predictive model / flow tích lũy
  float waterPumped = 0;     // g/ml water pumped (không trừ rò rỉ)
};

// ======================= KẾT QUẢ SHOT CUỐI =======================
struct LastShotResult {
  float weight = 0.0f;
  float time = 0.0f;
  float peakPressure = 0.0f;
  String profileName = "N/A";
  
  // Dữ liệu phục vụ Adaptive Control v6.5
  float conductance = 0.35f;      // Độ dẫn G (g/s/bar)
  float adaptiveStall = 12.0f;    // Stall pressure tự học (bar)
};

// ======================= STATES =======================
enum State {
  IDLE,
  PROFILE_RUN,
  HOTWATER_RUN,
  SHOT_END,
  FLUSH_RUN,
  SETTINGS_MENU,
  CALIBRATION_RUN,
};

#endif // SHOT_TYPES_H
