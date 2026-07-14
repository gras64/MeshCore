// StressEngine - Local stress monitoring for MeshCore
// Calculates stress from dispatcher counters (delays/errors per packet)
// No time-windowing - reports current cumulative stress state

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

// Local stress data point
struct StressWindow {
  uint32_t packets;
  uint32_t dc_delays;
  uint32_t lbt_delays;
  uint32_t recv_errors;
  uint32_t ghost_packets;  // Separate counter for ghost packets
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
  // n_tx_packets: sent packets (for delay ratios)
  // n_rx_packets: received packets (for error rate calculation)
  // n_ghost_packets: ghost packets (parse failures) - tracked separately
  void updateFromDispatcher(uint32_t n_tx_packets, uint32_t n_rx_packets, uint32_t n_dc_delays, uint32_t n_lbt_delays, uint32_t n_recv_errors, uint32_t n_ghost_packets);

  // Get stress level for local node (0-100) - read-only
  float getStress() const;

  // Get status (0=green, 1=yellow, 2=red) - read-only
  uint8_t getStatus() const;

  // Get formatted text reply for CLI (read-only)
  void formatLocalStressReply(char* reply) const;

  // Get formatted JSON fields for stats integration (read-only)
  void formatStressJsonFields(char* reply) const;

  // Clear all data
  void clear();

  // Set smoothing mode (updates both mode and factor)
  void setSmoothing(uint8_t mode);
  uint8_t getSmoothing() const { return _smoothing; }

private:
  StressWindow _window;
  uint8_t _smoothing;
  float _smooth_factor;

  void _calculateStress(float n_rx_packets);
  void _applySmoothing();
};
