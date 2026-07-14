#include "StressEngine.h"
#include <string.h>

StressEngine::StressEngine() {
  memset(&_window_1h, 0, sizeof(_window_1h));
  memset(&_window_24h, 0, sizeof(_window_24h));
  memset(&_window_7d, 0, sizeof(_window_7d));
  _smoothing = SMOOTH_BALANCED;
  _smooth_factor = SMOOTH_BALANCED_FACTOR;
  _last_update_time = 0;
}

void StressEngine::begin(uint8_t smoothing) {
  setSmoothing(smoothing);
}

void StressEngine::setSmoothing(uint8_t mode) {
  _smoothing = mode;
  switch (_smoothing) {
    case SMOOTH_SHARP:
      _smooth_factor = SMOOTH_SHARP_FACTOR;
      break;
    case SMOOTH_STABLE:
      _smooth_factor = SMOOTH_STABLE_FACTOR;
      break;
    default:
      _smooth_factor = SMOOTH_BALANCED_FACTOR;
      break;
  }
}

void StressEngine::updateFromDispatcher(uint32_t n_tx_packets, uint32_t n_rx_packets, uint32_t n_dc_delays, uint32_t n_lbt_delays, uint32_t n_recv_errors, uint32_t n_ghost_packets, uint32_t n_lbt_busy_ms) {
  uint32_t now = millis();
  
  // Only update every 5 seconds to avoid overcounting
  if (_last_update_time == 0) {
    _last_update_time = now;
  }
  
  if (now - _last_update_time < UPDATE_INTERVAL_SEC * 1000) {
    return;  // Skip this update
  }
  
  _last_update_time = now;
  
  // Accumulate counters for all windows
  _accumulateWindow(_window_1h, n_tx_packets, n_rx_packets, n_dc_delays, n_lbt_delays, n_recv_errors, n_ghost_packets, n_lbt_busy_ms);
  _accumulateWindow(_window_24h, n_tx_packets, n_rx_packets, n_dc_delays, n_lbt_delays, n_recv_errors, n_ghost_packets, n_lbt_busy_ms);
  _accumulateWindow(_window_7d, n_tx_packets, n_rx_packets, n_dc_delays, n_lbt_delays, n_recv_errors, n_ghost_packets, n_lbt_busy_ms);
  
  // Calculate stress for all windows
  _calculateStress(_window_1h, n_rx_packets);
  _calculateStress(_window_24h, n_rx_packets);
  _calculateStress(_window_7d, n_rx_packets);
  
  // Apply smoothing
  _applySmoothing(_window_1h);
  _applySmoothing(_window_24h);
  _applySmoothing(_window_7d);
}

void StressEngine::_accumulateWindow(RollingWindow& window, uint32_t n_tx_packets, uint32_t n_rx_packets, uint32_t n_dc_delays, uint32_t n_lbt_delays, uint32_t n_recv_errors, uint32_t n_ghost_packets, uint32_t n_lbt_busy_ms) {
  // Accumulate (add to existing values)
  window.packets += n_tx_packets;
  window.dc_delays += n_dc_delays;
  window.lbt_delays += n_lbt_delays;
  window.recv_errors += n_recv_errors;
  window.ghost_packets += n_ghost_packets;
  window.lbt_busy_ms += n_lbt_busy_ms;
}

void StressEngine::_calculateStress(RollingWindow& window, float n_rx_packets) {
  // Calculate ratios based on accumulated totals
  float tx_packets = (float)window.packets;
  if (tx_packets < 1.0f) tx_packets = 1.0f;  // avoid division by zero
  
  window.dc_ratio = (float)window.dc_delays / tx_packets;
  window.lbt_ratio = (float)window.lbt_delays / tx_packets;
  
  // Calculate receive error rate (errors per total received)
  float total_received = n_rx_packets + (float)window.recv_errors;
  if (total_received < 1.0f) total_received = 1.0f;
  window.recv_error_ratio = (float)window.recv_errors / total_received;
  
  // Calculate stress based on ratios
  // Delays: 0 = 0 stress, 10+ delays/pkt = 100 stress
  float raw = 0;
  raw += (window.dc_ratio / LBT_RATIO_RED_MAX) * 100.0f * (W_DC_DELAY / 1000.0f);
  raw += (window.lbt_ratio / LBT_RATIO_RED_MAX) * 100.0f * (W_LBT_DELAY / 1000.0f);
  
  // Receive errors: 0% = 0 stress, 10% = 100 stress
  raw += (window.recv_error_ratio / RECV_ERROR_RED_MAX) * 100.0f * (W_RECV_ERROR / 1000.0f);
  
  window.stress_raw = raw;
}

void StressEngine::_applySmoothing(RollingWindow& window) {
  window.stress_smooth = window.stress_smooth * (1.0f - _smooth_factor) + window.stress_raw * _smooth_factor;
  
  if (window.stress_smooth < 0) window.stress_smooth = 0;
  if (window.stress_smooth > 100) window.stress_smooth = 100;
  
  if (window.stress_smooth <= STRESS_GREEN_MAX) {
    window.status = 0;  // green
  } else if (window.stress_smooth <= STRESS_YELLOW_MAX) {
    window.status = 1;  // yellow
  } else {
    window.status = 2;  // red
  }
}

float StressEngine::getStress1H() const {
  return _window_1h.stress_smooth;
}

float StressEngine::getStress24H() const {
  return _window_24h.stress_smooth;
}

float StressEngine::getStress7D() const {
  return _window_7d.stress_smooth;
}

uint8_t StressEngine::getStatus1H() const {
  return _window_1h.status;
}

uint8_t StressEngine::getStatus24H() const {
  return _window_24h.status;
}

uint8_t StressEngine::getStatus7D() const {
  return _window_7d.status;
}

void StressEngine::formatLocalStressReply(char* reply) const {
  snprintf(reply, 128, "Use: get stress 1h, get stress 24h, or get stress 7d");
}

void StressEngine::formatStress1H(char* reply) const {
  const char* status_emoji[] = {"green", "yellow", "red"};
  snprintf(reply, 128,
    "=== 1 Hour Window ===\n"
    "stress: %.1f\n"
    "status: %s\n"
    "dc_delays: %u (%.2f/pkt)\n"
    "lbt_delays: %u (%.2f/pkt)\n"
    "lbt_busy: %lu ms\n"
    "recv_errors: %u (%.1f%%)\n"
    "ghost_packets: %u",
    _window_1h.stress_smooth, status_emoji[_window_1h.status],
    _window_1h.dc_delays, _window_1h.dc_ratio,
    _window_1h.lbt_delays, _window_1h.lbt_ratio,
    (unsigned long)_window_1h.lbt_busy_ms,
    _window_1h.recv_errors, _window_1h.recv_error_ratio * 100.0f,
    _window_1h.ghost_packets);
}

void StressEngine::formatStress24H(char* reply) const {
  const char* status_emoji[] = {"green", "yellow", "red"};
  snprintf(reply, 128,
    "=== 24 Hour Window ===\n"
    "stress: %.1f\n"
    "status: %s\n"
    "dc_delays: %u (%.2f/pkt)\n"
    "lbt_delays: %u (%.2f/pkt)\n"
    "lbt_busy: %lu ms\n"
    "recv_errors: %u (%.1f%%)\n"
    "ghost_packets: %u",
    _window_24h.stress_smooth, status_emoji[_window_24h.status],
    _window_24h.dc_delays, _window_24h.dc_ratio,
    _window_24h.lbt_delays, _window_24h.lbt_ratio,
    (unsigned long)_window_24h.lbt_busy_ms,
    _window_24h.recv_errors, _window_24h.recv_error_ratio * 100.0f,
    _window_24h.ghost_packets);
}

void StressEngine::formatStress7D(char* reply) const {
  const char* status_emoji[] = {"green", "yellow", "red"};
  snprintf(reply, 128,
    "=== 7 Day Window ===\n"
    "stress: %.1f\n"
    "status: %s\n"
    "dc_delays: %u (%.2f/pkt)\n"
    "lbt_delays: %u (%.2f/pkt)\n"
    "lbt_busy: %lu ms\n"
    "recv_errors: %u (%.1f%%)\n"
    "ghost_packets: %u",
    _window_7d.stress_smooth, status_emoji[_window_7d.status],
    _window_7d.dc_delays, _window_7d.dc_ratio,
    _window_7d.lbt_delays, _window_7d.lbt_ratio,
    (unsigned long)_window_7d.lbt_busy_ms,
    _window_7d.recv_errors, _window_7d.recv_error_ratio * 100.0f,
    _window_7d.ghost_packets);
}

void StressEngine::formatStressJsonFields(char* reply) const {
  sprintf(reply,
    "\"stress_1h\":%.1f,\"stress_24h\":%.1f,\"stress_7d\":%.1f,"
    "\"status_1h\":%d,\"status_24h\":%d,\"status_7d\":%d,"
    "\"dc_delays_1h\":%u,\"dc_delays_24h\":%u,\"dc_delays_7d\":%u,"
    "\"lbt_delays_1h\":%u,\"lbt_delays_24h\":%u,\"lbt_delays_7d\":%u,"
    "\"recv_errors_1h\":%u,\"recv_errors_24h\":%u,\"recv_errors_7d\":%u,"
    "\"ghost_packets_1h\":%u,\"ghost_packets_24h\":%u,\"ghost_packets_7d\":%u",
    _window_1h.stress_smooth, _window_24h.stress_smooth, _window_7d.stress_smooth,
    _window_1h.status, _window_24h.status, _window_7d.status,
    _window_1h.dc_delays, _window_24h.dc_delays, _window_7d.dc_delays,
    _window_1h.lbt_delays, _window_24h.lbt_delays, _window_7d.lbt_delays,
    _window_1h.recv_errors, _window_24h.recv_errors, _window_7d.recv_errors,
    _window_1h.ghost_packets, _window_24h.ghost_packets, _window_7d.ghost_packets);
}

void StressEngine::clear() {
  memset(&_window_1h, 0, sizeof(_window_1h));
  memset(&_window_24h, 0, sizeof(_window_24h));
  memset(&_window_7d, 0, sizeof(_window_7d));
}
