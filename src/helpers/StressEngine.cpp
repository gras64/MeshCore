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

void StressEngine::updateFromDispatcher(uint32_t n_packets, uint32_t n_retries, uint32_t n_dc_delays, uint32_t n_lbt_delays) {
  // Simply store the current dispatcher counters - stress calculation happens on query
  for (int w = 0; w < 3; w++) {
    StressWindow* win = &_windows[w];
    win->packets = n_packets;
    win->retries = n_retries;
    win->dc_delays = n_dc_delays;
    win->lbt_delays = n_lbt_delays;
  }
}

void StressEngine::_calculateStress(StressWindow* w, uint8_t window) const {
  uint16_t thresh = _getThreshold(window);
  if (thresh == 0) thresh = 1;
  
  float raw = 0;
  raw += (float)w->packets / (float)thresh * W_PACKETS;
  raw += (float)w->retries / (float)thresh * W_RETRIES;
  raw += (float)w->dc_delays / (float)thresh * W_DC_DELAY;
  raw += (float)w->lbt_delays / (float)thresh * W_LBT_DELAY;
  
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

uint16_t StressEngine::_getThreshold(uint8_t window) const {
  switch (window) {
    case 0: return THRESH_PACKETS_LIVE;
    case 1: return THRESH_PACKETS_DAILY;
    case 2: return THRESH_PACKETS_WEEKLY;
    default: return THRESH_PACKETS_DAILY;
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
  const char* window_name[] = {"live", "daily", "weekly"};
  const char* status_emoji[] = {"green", "yellow", "red"};
  
  const StressWindow* w = &_windows[window];
  _calculateStress(const_cast<StressWindow*>(w), window);
  _applySmoothing(const_cast<StressWindow*>(w));
  
  sprintf(reply,
    "stress: %.1f\n"
    "status: %s\n"
    "packets: %u\n"
    "retries: %u\n"
    "dc_delays: %u\n"
    "lbt_delays: %u",
    w->stress_smooth, status_emoji[w->status],
    w->packets, w->retries, w->dc_delays, w->lbt_delays);
}

void StressEngine::clear() {
  memset(_windows, 0, sizeof(_windows));
}
