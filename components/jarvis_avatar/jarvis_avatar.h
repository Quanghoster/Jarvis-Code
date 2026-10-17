#pragma once
// The JARVIS face: a circular avatar drawn with LVGL objects in a square.
// Its expression follows the voice pipeline state, with idle ambient animations,
// listening pulse, processing dots, speaking mouth movement, and error confusion.
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

  // Once a second: blink, pulse, processing dot cycle, etc.
  // `quiet` (standby) keeps it still.
  void on_tick(uint32_t epoch, bool quiet);

  // Wake-up animation: brief flourish.
  void wake();

  // Whether a point lies on the avatar (with touch margin).
  bool hit(int px, int py) const;

  bool made() const { return root_ != nullptr; }

  struct Expression;  // how the face looks in each state (jarvis_avatar.cpp)

 private:
  void apply(const Expression &expr);
  void place_arc(lv_obj_t *arc, int start, int end);
  void place_line(lv_obj_t *line, lv_point_precise_t *points, int count);
  int at(float unit) const { return static_cast<int>(unit * size_ + 0.5f); }

  // Object hierarchy
  lv_obj_t *root_{};
  lv_obj_t *face_{};           // background circle
  lv_obj_t *ring_{};           // outer glow ring
  lv_obj_t *eye_l_{}, *eye_r_{};  // two eyes (circles)
  lv_obj_t *brow_l_{}, *brow_r_{}; // eyebrow lines
  lv_obj_t *mouth_{};          // mouth polyline
  lv_obj_t *dot_l_{}, *dot_c_{}, *dot_r_{}; // processing dots
  lv_obj_t *glyph_{};          // text glyph (?, !, ...)
  lv_obj_t *core_{};           // centre glow dot

  // Mouth point storage
  lv_point_precise_t mouth_pts_[5]{};

  int x_ = 0, y_ = 0, size_ = 0, eye_size_ = 0;
  VoiceState state_ = VoiceState::UNKNOWN;
  uint8_t expr_idx_ = 0;       // for cycling through expression variations
  bool expressed_ = false;
};

}  // namespace esphome::jarvis_avatar
