#pragma once
// The JARVIS Reactor Avatar: a circular reactor core with concentric glowing rings,
// inspired by the Iron Man arc reactor and the View Assist dashboard design.
// Its glow and pulse follow the voice assistant pipeline state.
//
// Adapted from microplant_avatar.h

#include "esphome/components/smart_display/plugin_api.h"
#include "jarvis_model.h"

namespace esphome::jarvis_avatar {

class JarvisAvatar {
 public:
  // Build the avatar in a square of `size` pixels at (x, y) of `parent`.
  void create(lv_obj_t *parent, int x, int y, int size);

  // Set the voice pipeline state; same state again changes nothing.
  void set_state(VoiceState state);

  // Colours for light/dark theme.
  void on_theme();

  // Once a second: pulse, ring rotation, etc.
  // `quiet` (standby) keeps it still.
  void on_tick(uint32_t epoch, bool quiet);

  // Wake-up animation: bright flash and ring expansion.
  void wake();

  // Whether a point lies on the avatar (with touch margin).
  bool hit(int px, int py) const;

  bool made() const { return root_ != nullptr; }

 private:
  void apply_state(VoiceState state);
  void start_pulse_animation();
  void start_ring_animation();
  void stop_animations();
  int at(float unit) const { return static_cast<int>(unit * size_ + 0.5f); }

  // Object hierarchy - reactor style
  lv_obj_t *root_{};
  lv_obj_t *outer_ring_{};     // Outermost glow ring
  lv_obj_t *mid_ring_{};       // Middle ring (rotating)
  lv_obj_t *inner_ring_{};     // Inner decorative ring
  lv_obj_t *core_{};           // Central reactor core
  lv_obj_t *core_highlight_{}; // Core highlight spot
  lv_obj_t *glow_overlay_{};   // Ambient glow behind

  // State indicators
  lv_obj_t *status_dot_{};     // Small status indicator
  lv_obj_t *voice_bars_[5]{};  // Voice visualiser bars (for speaking state)

  int x_ = 0, y_ = 0, size_ = 0;
  VoiceState state_ = VoiceState::UNKNOWN;
  uint32_t last_tick_ = 0;
  bool expressed_ = false;
};

}  // namespace esphome::jarvis_avatar
