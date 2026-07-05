#include "StressEngine.h"
#include <string.h>

StressEngine::StressEngine() {
  memset(&_window, 0, sizeof(_window));
  _smoothing = SMOOTH_BALANCED;
  _smooth_factor = SMOOTH_BALANCED_FACTOR;
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

void StressEngine::updateFromDispatcher(uint32_t n_packets, uint32_t n_dc_delays, uint32_t n_lbt_delays, uint32_t n_recv_errors) {
  // Store current dispatcher counters
  _window.packets = n_packets;
  _window.dc_delays = n_dc_delays;
  _window.lbt_delays = n_lbt_delays;
  _window.recv_errors = n_recv_errors;
  
  // Calculate stress and apply smoothing immediately
  _calculateStress();
  _applySmoothing();
}

void StressEngine::_calculateStress() {
  // Calculate ratios (delays per packet sent)
  float packets = (float)_window.packets;
  if (packets < 1.0f) packets = 1.0f;  // avoid division by zero
  
  _window.dc_ratio = (float)_window.dc_delays / packets;
  _window.lbt_ratio = (float)_window.lbt_delays / packets;
  
  // Calculate receive error rate (errors per total received)
  float total_received = (float)_window.packets + (float)_window.recv_errors;
  if (total_received < 1.0f) total_received = 1.0f;
  _window.recv_error_ratio = (float)_window.recv_errors / total_received;
  
  // Calculate stress based on ratios
  // Delays: 0 = 0 stress, 10+ delays/pkt = 100 stress
  float raw = 0;
  raw += (_window.dc_ratio / LBT_RATIO_RED_MAX) * 100.0f * (W_DC_DELAY / 1000.0f);
  raw += (_window.lbt_ratio / LBT_RATIO_RED_MAX) * 100.0f * (W_LBT_DELAY / 1000.0f);
  
  // Receive errors: 0% = 0 stress, 10% = 100 stress
  raw += (_window.recv_error_ratio / RECV_ERROR_RED_MAX) * 100.0f * (W_RECV_ERROR / 1000.0f);
  
  _window.stress_raw = raw;
}

void StressEngine::_applySmoothing() {
  _window.stress_smooth = _window.stress_smooth * (1.0f - _smooth_factor) + _window.stress_raw * _smooth_factor;
  
  if (_window.stress_smooth < 0) _window.stress_smooth = 0;
  if (_window.stress_smooth > 100) _window.stress_smooth = 100;
  
  if (_window.stress_smooth <= STRESS_GREEN_MAX) {
    _window.status = 0;  // green
  } else if (_window.stress_smooth <= STRESS_YELLOW_MAX) {
    _window.status = 1;  // yellow
  } else {
    _window.status = 2;  // red
  }
}

float StressEngine::getStress() const {
  return _window.stress_smooth;
}

uint8_t StressEngine::getStatus() const {
  return _window.status;
}

void StressEngine::formatLocalStressReply(char* reply) const {
  const char* status_emoji[] = {"green", "yellow", "red"};
  
  sprintf(reply,
    "stress: %.1f\n"
    "status: %s\n"
    "dc_delays: %u (%.2f/pkt)\n"
    "lbt_delays: %u (%.2f/pkt)\n"
    "recv_errors: %u (%.1f%%)",
    _window.stress_smooth, status_emoji[_window.status],
    _window.dc_delays, _window.dc_ratio,
    _window.lbt_delays, _window.lbt_ratio,
    _window.recv_errors, _window.recv_error_ratio * 100.0f);
}

void StressEngine::formatStressJsonFields(char* reply) const {
  sprintf(reply,
    "\"stress\":%.1f,\"stress_status\":%d,\"dc_ratio\":%.2f,\"lbt_ratio\":%.2f,\"recv_error_pct\":%.1f",
    _window.stress_smooth, _window.status, _window.dc_ratio, _window.lbt_ratio, _window.recv_error_ratio * 100.0f);
}

void StressEngine::clear() {
  memset(&_window, 0, sizeof(_window));
}
