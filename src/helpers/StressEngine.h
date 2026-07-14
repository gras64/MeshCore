// StressEngine - Local stress monitoring for MeshCore
// Calculates stress from dispatcher counters with 3 time windows (1h/24h/7d)

#pragma once

#include <Arduino.h>

// Smoothing modes
#define SMOOTH_SHARP      0    // reaktiv, wenig Glättung
#define SMOOTH_BALANCED   1    // Standard
#define SMOOTH_STABLE     2    // stark geglättet, trend-orientiert

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

// Time window durations (in seconds)
#define WINDOW_1H_DURATION     3600  // 1 hour
#define WINDOW_24H_DURATION    86400 // 24 hours
#define WINDOW_7D_DURATION     604800 // 7 days

// Update interval (seconds) - how often we call updateFromDispatcher
#define UPDATE_INTERVAL_SEC    5

// Local stress data point for each window
struct RollingWindow {
  uint32_t packets;
  uint32_t dc_delays;
  uint32_t lbt_delays;
  uint32_t recv_errors;
  uint32_t ghost_packets;
  uint32_t lbt_busy_ms;  // Total time medium was busy (ms)
  float    lbt_ratio;
  float    dc_ratio;
  float    recv_error_ratio;
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
  // n_tx_packets: sent packets (for delay ratios)
  // n_rx_packets: received packets (for error rate calculation)
  // n_ghost_packets: ghost packets (parse failures) - tracked separately
  // n_lbt_busy_ms: total time medium was busy (ms)
  void updateFromDispatcher(uint32_t n_tx_packets, uint32_t n_rx_packets, uint32_t n_dc_delays, uint32_t n_lbt_delays, uint32_t n_recv_errors, uint32_t n_ghost_packets, uint32_t n_lbt_busy_ms);

  // Get stress levels for each window (0-100) - read-only
  float getStress1H() const;
  float getStress24H() const;
  float getStress7D() const;

  // Get status for each window (0=green, 1=yellow, 2=red) - read-only
  uint8_t getStatus1H() const;
  uint8_t getStatus24H() const;
  uint8_t getStatus7D() const;

  // Get formatted text reply for CLI (read-only)
  void formatLocalStressReply(char* reply) const;
  void formatStress1H(char* reply) const;
  void formatStress24H(char* reply) const;
  void formatStress7D(char* reply) const;

  // Get formatted JSON fields for stats integration (read-only)
  void formatStressJsonFields(char* reply) const;

  // Clear all data
  void clear();

  // Set smoothing mode (updates both mode and factor)
  void setSmoothing(uint8_t mode);
  uint8_t getSmoothing() const { return _smoothing; }

private:
  RollingWindow _window_1h;
  RollingWindow _window_24h;
  RollingWindow _window_7d;
  uint8_t _smoothing;
  float _smooth_factor;
  uint32_t _last_update_time;  // Track when we last updated
  
  void _accumulateWindow(RollingWindow& window, uint32_t n_tx_packets, uint32_t n_rx_packets, uint32_t n_dc_delays, uint32_t n_lbt_delays, uint32_t n_recv_errors, uint32_t n_ghost_packets, uint32_t n_lbt_busy_ms);
  void _calculateStress(RollingWindow& window, float n_rx_packets);
  void _applySmoothing(RollingWindow& window);
};
