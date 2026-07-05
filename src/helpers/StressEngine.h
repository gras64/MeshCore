#pragma once

#include <Arduino.h>

// Smoothing modes
#define SMOOTH_SHARP      0    // reaktiv, wenig Glättung
#define SMOOTH_BALANCED   1    // Standard
#define SMOOTH_STABLE     2    // stark geglättet, trend-orientiert

// Time window definitions (in seconds)
#define WINDOW_LIVE       3600   // 1h
#define WINDOW_DAILY      86400  // 24h
#define WINDOW_WEEKLY     604800 // 7d

// Stress threshold (normalized 0-100)
#define STRESS_GREEN_MAX    20
#define STRESS_YELLOW_MAX   60

// Weights for stress calculation (fixed-point x1000)
#define W_DC_DELAY     500  // 0.5 - DC delays are most critical
#define W_LBT_DELAY    500  // 0.5 - LBT delays are critical
#define W_RECV_ERROR   300  // 0.3 - Receive errors indicate interference

// LBT/DC delay thresholds (delays per packet sent)
// 10+ delays per packet = red (very bad)
// 5 delays per packet = yellow (elevated)
// 1 delay per packet = green (normal)
#define LBT_RATIO_GREEN_MAX    1.0f
#define LBT_RATIO_YELLOW_MAX   5.0f
#define LBT_RATIO_RED_MAX      10.0f

// Receive error thresholds (errors per packet received)
// 10% error rate = red (very bad)
// 5% error rate = yellow (elevated)
// 1% error rate = green (normal)
#define RECV_ERROR_GREEN_MAX   0.01f
#define RECV_ERROR_YELLOW_MAX  0.05f
#define RECV_ERROR_RED_MAX     0.10f

// Smoothing factors (exponential moving average)
#define SMOOTH_SHARP_FACTOR      0.3f
#define SMOOTH_BALANCED_FACTOR   0.1f
#define SMOOTH_STABLE_FACTOR     0.03f

// Local stress data point (per time window)
struct StressWindow {
  uint32_t packets;
  uint32_t dc_delays;
  uint32_t lbt_delays;
  uint32_t recv_errors;
  float    lbt_ratio;     // lbt_delays / packets (delays per packet)
  float    dc_ratio;      // dc_delays / packets (delays per packet)
  float    recv_error_ratio;  // recv_errors / (packets + recv_errors) (error rate)
  float    stress_raw;
  float    stress_smooth;
  uint8_t  status;  // 0=green, 1=yellow, 2=red
};

class StressEngine {
public:
  StressEngine();

  // Initialize with smoothing mode
  void begin(uint8_t smoothing = SMOOTH_BALANCED);

  // Update from dispatcher counters (local node only)
  void updateFromDispatcher(uint32_t n_packets, uint32_t n_retries, uint32_t n_dc_delays, uint32_t n_lbt_delays, uint32_t n_recv_errors);

  // Get stress level for local node (0-100)
  // window: 0=live(1h), 1=daily(24h), 2=weekly(7d)
  float getStress(uint8_t window) const;

  // Get status (0=green, 1=yellow, 2=red)
  uint8_t getStatus(uint8_t window) const;

  // Get formatted JSON reply for local node
  void formatLocalStressReply(char* reply, uint8_t window) const;

  // Clear all data
  void clear();

  // Set smoothing mode
  void setSmoothing(uint8_t mode) { _smoothing = mode; }
  uint8_t getSmoothing() const { return _smoothing; }

private:
  StressWindow _windows[3]; // 0=live, 1=daily, 2=weekly
  uint8_t _smoothing;
  float _smooth_factor;

  void _calculateStress(StressWindow* w, uint8_t window) const;
  void _applySmoothing(StressWindow* w) const;
  uint16_t _getThreshold(uint8_t window) const;
};
