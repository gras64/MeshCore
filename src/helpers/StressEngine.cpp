#include "StressEngine.h"
#include <string.h>

StressEngine::StressEngine() {
  memset(_windows, 0, sizeof(_windows));
  _smoothing = SMOOTH_BALANCED;
  _smooth_factor = SMOOTH_BALANCED_FACTOR;
}

void StressEngine::begin(uint8_t smoothing) {
  _smoothing = smoothing;
  switch (_smoothing) {
    case SMOOTH_SHARP:      _smooth_factor = SMOOTH_SHARP_FACTOR; break;
    case SMOOTH_STABLE:     _smooth_factor = SMOOTH_STABLE_FACTOR; break;
    default:                _smooth_factor = SMOOTH_BALANCED_FACTOR; break;
  }
}

void StressEngine::updateFromDispatcher(uint32_t n_packets, uint32_t n_retries, uint32_t n_dc_delays, uint32_t n_lbt_delays, uint32_t n_recv_errors) {
  // Simply store the current dispatcher counters - stress calculation happens on query
  for (int w = 0; w < 3; w++) {
    StressWindow* win = &_windows[w];
    win->packets = n_packets;
    win->dc_delays = n_dc_delays;
    win->lbt_delays = n_lbt_delays;
    win->recv_errors = n_recv_errors;
  }
}

void StressEngine::_calculateStress(StressWindow* w, uint8_t window) const {
  // Calculate ratios (delays per packet sent)
  float packets = (float)w->packets;
  if (packets < 1.0f) packets = 1.0f;  // avoid division by zero
  
  w->dc_ratio = (float)w->dc_delays / packets;
  w->lbt_ratio = (float)w->lbt_delays / packets;
  
  // Calculate receive error rate (errors per total received)
  float total_received = (float)w->packets + (float)w->recv_errors;
  if (total_received < 1.0f) total_received = 1.0f;
  w->recv_error_ratio = (float)w->recv_errors / total_received;
  
  // Calculate stress based on ratios
  // Delays: 0 = 0 stress, 10+ delays/pkt = 100 stress
  float raw = 0;
  raw += (w->dc_ratio / LBT_RATIO_RED_MAX) * 100.0f * (W_DC_DELAY / 1000.0f);
  raw += (w->lbt_ratio / LBT_RATIO_RED_MAX) * 100.0f * (W_LBT_DELAY / 1000.0f);
  
  // Receive errors: 0% = 0 stress, 10% = 100 stress
  raw += (w->recv_error_ratio / RECV_ERROR_RED_MAX) * 100.0f * (W_RECV_ERROR / 1000.0f);
  
  w->stress_raw = raw;
}

void StressEngine::_applySmoothing(StressWindow* w) const {
  w->stress_smooth = w->stress_smooth * (1.0f - _smooth_factor) + w->stress_raw * _smooth_factor;
  
  if (w->stress_smooth < 0) w->stress_smooth = 0;
  if (w->stress_smooth > 100) w->stress_smooth = 100;
  
  if (w->stress_smooth <= STRESS_GREEN_MAX) {
    w->status = 0;  // green
  } else if (w->stress_smooth <= STRESS_YELLOW_MAX) {
    w->status = 1;  // yellow
  } else {
    w->status = 2;  // red
  }
}

float StressEngine::getStress(uint8_t window) const {
  const StressWindow* w = &_windows[window];
  _calculateStress(const_cast<StressWindow*>(w), window);
  _applySmoothing(const_cast<StressWindow*>(w));
  return w->stress_smooth;
}

uint8_t StressEngine::getStatus(uint8_t window) const {
  return _windows[window].status;
}

void StressEngine::formatLocalStressReply(char* reply, uint8_t window) const {
  const char* status_emoji[] = {"green", "yellow", "red"};
  
  const StressWindow* w = &_windows[window];
  _calculateStress(const_cast<StressWindow*>(w), window);
  _applySmoothing(const_cast<StressWindow*>(w));
  
  sprintf(reply,
    "stress: %.1f\n"
    "status: %s\n"
    "packets: %u\n"
    "dc_delays: %u (%.2f/pkt)\n"
    "lbt_delays: %u (%.2f/pkt)\n"
    "recv_errors: %u (%.1f%%)",
    w->stress_smooth, status_emoji[w->status],
    w->packets,
    w->dc_delays, w->dc_ratio,
    w->lbt_delays, w->lbt_ratio,
    w->recv_errors, w->recv_error_ratio * 100.0f);
}

void StressEngine::formatStressJsonFields(char* reply, uint8_t window) const {
  const StressWindow* w = &_windows[window];
  _calculateStress(const_cast<StressWindow*>(w), window);
  _applySmoothing(const_cast<StressWindow*>(w));
  
  sprintf(reply,
    "\"stress\":%.1f,\"stress_status\":%d,\"dc_ratio\":%.2f,\"lbt_ratio\":%.2f,\"recv_error_pct\":%.1f",
    w->stress_smooth, w->status, w->dc_ratio, w->lbt_ratio, w->recv_error_ratio * 100.0f);
}

void StressEngine::clear() {
  memset(_windows, 0, sizeof(_windows));
}
