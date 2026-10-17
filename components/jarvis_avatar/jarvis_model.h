#pragma once
// JARVIS Avatar for Tessera screens.
// A circular face that reacts to Home Assistant voice assistant pipeline state.
// Shows idle, listening, processing, speaking, and error states with expressive animations.
//
// Adapted from the tessera-Microplant plugin architecture.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace esphome::jarvis_avatar {

// ===================================================================================================================
// Configuration
// ===================================================================================================================
namespace config {
// How often to poll the voice state from Home Assistant.
constexpr uint32_t POLL_SECONDS = 2;
constexpr uint32_t POLL_MIN_SECONDS = 1;
constexpr uint32_t POLL_MAX_SECONDS = 10;

// After a failed poll, retry sooner.
constexpr uint32_t RETRY_SECONDS = 1;

// If no update for this long, show offline state.
constexpr uint32_t OFFLINE_AFTER_SECONDS = 15;

// Demo cycle length for trying animations without HA connection.
constexpr uint32_t DEMO_CYCLE_SECONDS = 20;

// Animation timings.
constexpr uint32_t BLINK_INTERVAL_SECONDS = 5;
constexpr uint32_t IDLE_PULSE_SECONDS = 3;
constexpr uint32_t LISTENING_PULSE_SECONDS = 1;
constexpr uint32_t PROCESSING_DOT_SECONDS = 2;
constexpr uint32_t SPEAKING_MOUTH_SPEED_MS = 180;
}  // namespace config

// ---- Voice Pipeline States ----

enum class VoiceState : uint8_t {
  UNKNOWN,     // no reading yet / offline
  IDLE,        // waiting, ambient animations
  LISTENING,   // wake word detected, mic active
  PROCESSING,  // speech-to-text or intent processing
  SPEAKING,    // text-to-speech output
  ERROR,       // pipeline error / didn't understand
  WAKE,        // boot / just activated
  SLEEP        // low-power dim state
};

inline const char* voice_state_name(VoiceState s) {
  switch (s) {
    case VoiceState::IDLE:       return "idle";
    case VoiceState::LISTENING:  return "listening";
    case VoiceState::PROCESSING: return "processing";
    case VoiceState::SPEAKING:   return "speaking";
    case VoiceState::ERROR:      return "error";
    case VoiceState::WAKE:       return "wake";
    case VoiceState::SLEEP:      return "sleep";
    default:                     return "unknown";
  }
}

// ---- Parsing helpers ----

inline VoiceState parse_voice_state(const char* text) {
  if (!text || !*text) return VoiceState::UNKNOWN;
  char low[32];
  size_t n = 0;
  for (; text[n] && n + 1 < sizeof(low); ++n) {
    char c = text[n];
    low[n] = (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
  }
  low[n] = 0;
  if (strstr(low, "listen"))   return VoiceState::LISTENING;
  if (strstr(low, "process"))  return VoiceState::PROCESSING;
  if (strstr(low, "speak"))     return VoiceState::SPEAKING;
  if (strstr(low, "error"))     return VoiceState::ERROR;
  if (strstr(low, "wake"))      return VoiceState::WAKE;
  if (strstr(low, "sleep"))     return VoiceState::SLEEP;
  if (strstr(low, "idle"))      return VoiceState::IDLE;
  return VoiceState::UNKNOWN;
}

// ---- Demo state cycling ----

inline VoiceState demo_state(uint32_t epoch) {
  const uint32_t phase = epoch % config::DEMO_CYCLE_SECONDS;
  if (phase < 5)  return VoiceState::IDLE;
  if (phase < 8)  return VoiceState::LISTENING;
  if (phase < 11) return VoiceState::PROCESSING;
  if (phase < 15) return VoiceState::SPEAKING;
  if (phase < 17) return VoiceState::ERROR;
  return VoiceState::IDLE;
}

}  // namespace esphome::jarvis_avatar
