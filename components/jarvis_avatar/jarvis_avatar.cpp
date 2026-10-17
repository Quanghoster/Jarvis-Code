#include "jarvis_avatar.h"
#include <algorithm>
#include <cmath>

namespace esphome::jarvis_avatar {

namespace ui = tessera::ui;

// ---- Colours matching the dashboard reactor ----
namespace colour {
constexpr lv_color_t CYAN = lv_color_hex(0x00D4FF);
constexpr lv_color_t CYAN_DIM = lv_color_hex(0x006680);
constexpr lv_color_t CORE_INNER = lv_color_hex(0xAADDFF);
constexpr lv_color_t CORE_MID = lv_color_hex(0x00D4FF);
constexpr lv_color_t CORE_OUTER = lv_color_hex(0x002244);
constexpr lv_color_t RING_BORDER = lv_color_hex(0x00D4FF);
constexpr lv_color_t RING_DIM = lv_color_hex(0x0088AA);
constexpr lv_color_t AMBER = lv_color_hex(0xFFAA00);
constexpr lv_color_t RED = lv_color_hex(0xFF3333);
constexpr lv_color_t GREEN = lv_color_hex(0x00FF66);
constexpr lv_color_t PURPLE = lv_color_hex(0xAA66FF);
constexpr lv_color_t OFF = lv_color_hex(0x112233);
}  // namespace colour

// ---- Animation helpers ----

namespace {

void pulse_scale(void *var, int32_t v) {
  auto *o = static_cast<lv_obj_t *>(var);
  // v is 0-1000, map to scale 0.85-1.0
  const float scale = 0.85f + 0.15f * v / 1000.0f;
  lv_obj_set_style_transform_scale_x(o, static_cast<lv_coord_t>(scale * 256), 0);
  lv_obj_set_style_transform_scale_y(o, static_cast<lv_coord_t>(scale * 256), 0);
}

void pulse_opacity(void *var, int32_t v) {
  auto *o = static_cast<lv_obj_t *>(var);
  lv_obj_set_style_opa(o, static_cast<lv_opa_t>(v * 255 / 1000), 0);
}

void rotate_ring(lv_anim_t *a, int32_t v) {
  auto *o = static_cast<lv_obj_t *>(a->var);
  lv_obj_set_style_transform_rotation(o, v, 0);
}

void bar_height(lv_anim_t *a, int32_t v) {
  auto *o = static_cast<lv_obj_t *>(a->var);
  lv_obj_set_height(o, v);
  lv_obj_set_y(o, lv_obj_get_y(o) + (lv_obj_get_height(o) - v) / 2);
}

void start(lv_obj_t *o, int32_t from, int32_t to, uint32_t ms, lv_anim_exec_xcb_t plain,
           lv_anim_custom_exec_cb_t custom, intptr_t user, uint32_t delay = 0, uint32_t back_ms = 0, uint32_t repeat = 1) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_delay(&a, delay);
  if (back_ms) lv_anim_set_reverse_duration(&a, back_ms);
  lv_anim_set_repeat_count(&a, repeat);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  if (plain) lv_anim_set_exec_cb(&a, plain);
  if (custom) lv_anim_set_custom_exec_cb(&a, custom);
  lv_anim_set_user_data(&a, reinterpret_cast<void *>(user));
  lv_anim_start(&a);
}

lv_obj_t *box(lv_obj_t *parent, int x, int y, int size) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, size, size);
  return o;
}

lv_obj_t *circle(lv_obj_t *parent, lv_color_t col) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, col, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  return o;
}

}  // namespace

// ---- Build the reactor avatar ----

void JarvisAvatar::create(lv_obj_t *parent, int x, int y, int size) {
  x_ = x;
  y_ = y;
  size_ = size < 32 ? 32 : size;
  root_ = box(parent, x, y, size_);

  const int cx = size_ / 2;  // centre x
  const int cy = size_ / 2;  // centre y

  // Ambient glow overlay (behind everything, large soft glow)
  glow_overlay_ = circle(root_, colour::CYAN);
  const int glow_d = at(1.4f);
  lv_obj_set_size(glow_overlay_, glow_d, glow_d);
  lv_obj_set_pos(glow_overlay_, cx - glow_d / 2, cy - glow_d / 2);
  lv_obj_set_style_bg_opa(glow_overlay_, LV_OPA_10, 0);
  lv_obj_set_style_shadow_color(glow_overlay_, colour::CYAN, 0);
  lv_obj_set_style_shadow_width(glow_overlay_, at(0.3f), 0);
  lv_obj_set_style_shadow_opa(glow_overlay_, LV_OPA_30, 0);

  // Outer ring (glow ring)
  outer_ring_ = circle(root_, lv_color_black());
  const int outer_d = at(0.95f);
  lv_obj_set_size(outer_ring_, outer_d, outer_d);
  lv_obj_set_pos(outer_ring_, cx - outer_d / 2, cy - outer_d / 2);
  lv_obj_set_style_border_color(outer_ring_, colour::RING_BORDER, 0);
  lv_obj_set_style_border_width(outer_ring_, std::max(2, at(0.04f)), 0);
  lv_obj_set_style_border_opa(outer_ring_, LV_OPA_60, 0);
  lv_obj_set_style_shadow_color(outer_ring_, colour::CYAN, 0);
  lv_obj_set_style_shadow_width(outer_ring_, at(0.15f), 0);
  lv_obj_set_style_shadow_opa(outer_ring_, LV_OPA_50, 0);
  lv_obj_set_style_bg_opa(outer_ring_, LV_OPA_TRANSP, 0);

  // Middle ring (rotating decorative ring)
  mid_ring_ = circle(root_, lv_color_black());
  const int mid_d = at(0.78f);
  lv_obj_set_size(mid_ring_, mid_d, mid_d);
  lv_obj_set_pos(mid_ring_, cx - mid_d / 2, cy - mid_d / 2);
  lv_obj_set_style_border_color(mid_ring_, colour::RING_DIM, 0);
  lv_obj_set_style_border_width(mid_ring_, std::max(1, at(0.025f)), 0);
  lv_obj_set_style_border_opa(mid_ring_, LV_OPA_40, 0);
  lv_obj_set_style_bg_opa(mid_ring_, LV_OPA_TRANSP, 0);

  // Inner ring
  inner_ring_ = circle(root_, lv_color_black());
  const int inner_d = at(0.62f);
  lv_obj_set_size(inner_ring_, inner_d, inner_d);
  lv_obj_set_pos(inner_ring_, cx - inner_d / 2, cy - inner_d / 2);
  lv_obj_set_style_border_color(inner_ring_, colour::RING_BORDER, 0);
  lv_obj_set_style_border_width(mid_ring_, std::max(1, at(0.02f)), 0);
  lv_obj_set_style_border_opa(inner_ring_, LV_OPA_30, 0);
  lv_obj_set_style_bg_opa(inner_ring_, LV_OPA_TRANSP, 0);

  // Central reactor core
  core_ = circle(root_, colour::CORE_MID);
  const int core_d = at(0.45f);
  lv_obj_set_size(core_, core_d, core_d);
  lv_obj_set_pos(core_, cx - core_d / 2, cy - core_d / 2);
  lv_obj_set_style_bg_color(core_, colour::CORE_MID, 0);
  lv_obj_set_style_shadow_color(core_, colour::CYAN, 0);
  lv_obj_set_style_shadow_width(core_, at(0.2f), 0);
  lv_obj_set_style_shadow_opa(core_, LV_OPA_60, 0);

  // Core highlight (simulates radial gradient bright spot)
  core_highlight_ = circle(root_, colour::CORE_INNER);
  const int hl_d = at(0.18f);
  lv_obj_set_size(core_highlight_, hl_d, hl_d);
  lv_obj_set_pos(core_highlight_, cx - hl_d / 2 - at(0.05f), cy - hl_d / 2 - at(0.05f));
  lv_obj_set_style_opa(core_highlight_, LV_OPA_70, 0);

  // Status dot (small indicator near bottom)
  status_dot_ = circle(root_, colour::OFF);
  const int sd_d = at(0.06f);
  lv_obj_set_size(status_dot_, sd_d, sd_d);
  lv_obj_set_pos(status_dot_, cx - sd_d / 2, cy + at(0.35f));
  lv_obj_set_style_shadow_color(status_dot_, colour::OFF, 0);
  lv_obj_set_style_shadow_width(status_dot_, at(0.05f), 0);

  // Voice visualiser bars (hidden by default, shown when speaking)
  const int bar_w = std::max(2, at(0.025f));
  const int bar_gap = std::max(3, at(0.04f));
  for (int i = 0; i < 5; ++i) {
    voice_bars_[i] = lv_obj_create(root_);
    lv_obj_remove_style_all(voice_bars_[i]);
    lv_obj_remove_flag(voice_bars_[i], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_size(voice_bars_[i], bar_w, 2);
    lv_obj_set_pos(voice_bars_[i],
                   cx - (2 * bar_gap + 2 * bar_w) + i * (bar_w + bar_gap),
                   cy + at(0.42f));
    lv_obj_set_style_bg_color(voice_bars_[i], colour::CYAN, 0);
    lv_obj_set_style_radius(voice_bars_[i], bar_w / 2, 0);
    lv_obj_set_style_opa(voice_bars_[i], LV_OPA_TRANSP, 0);
  }

  apply_state(VoiceState::UNKNOWN);
  expressed_ = true;
}

// ---- Apply state-specific visuals ----

void JarvisAvatar::set_state(VoiceState state) {
  if (!root_ || (expressed_ && state == state_)) return;
  state_ = state;
  apply_state(state);
}

void JarvisAvatar::apply_state(VoiceState state) {
  stop_animations();

  lv_color_t ring_col = colour::RING_BORDER;
  lv_color_t core_col = colour::CORE_MID;
  lv_color_t status_col = colour::OFF;
  lv_opa_t core_opa = LV_OPA_80;
  lv_opa_t glow_opa = LV_OPA_10;
  uint32_t pulse_ms = 0;
  bool show_bars = false;

  switch (state) {
    case VoiceState::IDLE:
      ring_col = colour::RING_BORDER;
      core_col = colour::CORE_MID;
      status_col = colour::CYAN_DIM;
      core_opa = LV_OPA_80;
      glow_opa = LV_OPA_15;
      pulse_ms = 2000;  // slow gentle pulse
      break;

    case VoiceState::LISTENING:
      ring_col = colour::GREEN;
      core_col = colour::GREEN;
      status_col = colour::GREEN;
      core_opa = LV_OPA_100;
      glow_opa = LV_OPA_30;
      pulse_ms = 800;   // fast attentive pulse
      break;

    case VoiceState::PROCESSING:
      ring_col = colour::AMBER;
      core_col = colour::AMBER;
      status_col = colour::AMBER;
      core_opa = LV_OPA_90;
      glow_opa = LV_OPA_25;
      pulse_ms = 600;   // rapid processing pulse
      break;

    case VoiceState::SPEAKING:
      ring_col = colour::CYAN;
      core_col = colour::CORE_INNER;
      status_col = colour::CYAN;
      core_opa = LV_OPA_100;
      glow_opa = LV_OPA_35;
      pulse_ms = 400;   // quick speaking pulse
      show_bars = true;
      break;

    case VoiceState::ERROR:
      ring_col = colour::RED;
      core_col = colour::RED;
      status_col = colour::RED;
      core_opa = LV_OPA_70;
      glow_opa = LV_OPA_20;
      pulse_ms = 1200;  // erratic slow pulse
      break;

    case VoiceState::WAKE:
      ring_col = colour::GREEN;
      core_col = colour::CORE_INNER;
      status_col = colour::GREEN;
      core_opa = LV_OPA_100;
      glow_opa = LV_OPA_50;
      pulse_ms = 300;   // bright quick pulse
      break;

    case VoiceState::SLEEP:
      ring_col = colour::OFF;
      core_col = colour::OFF;
      status_col = colour::PURPLE;
      core_opa = LV_OPA_30;
      glow_opa = LV_OPA_5;
      pulse_ms = 4000;  // very slow dim pulse
      break;

    case VoiceState::UNKNOWN:
    default:
      ring_col = colour::RING_DIM;
      core_col = colour::OFF;
      status_col = colour::OFF;
      core_opa = LV_OPA_50;
      glow_opa = LV_OPA_10;
      pulse_ms = 3000;
      break;
  }

  // Apply colours
  lv_obj_set_style_border_color(outer_ring_, ring_col, 0);
  lv_obj_set_style_shadow_color(outer_ring_, ring_col, 0);
  lv_obj_set_style_shadow_color(core_, core_col, 0);
  lv_obj_set_style_bg_color(core_, core_col, 0);
  lv_obj_set_style_bg_color(core_highlight_, core_col, 0);
  lv_obj_set_style_bg_color(status_dot_, status_col, 0);
  lv_obj_set_style_shadow_color(status_dot_, status_col, 0);
  lv_obj_set_style_bg_opa(core_, core_opa, 0);
  lv_obj_set_style_bg_opa(glow_overlay_, glow_opa, 0);

  // Voice bars
  for (auto *bar : voice_bars_) {
    lv_obj_set_style_opa(bar, show_bars ? LV_OPA_80 : LV_OPA_TRANSP, 0);
    lv_anim_delete(bar, nullptr);
  }

  // Start animations if pulse interval set
  if (pulse_ms > 0) {
    start_pulse_animation();
    start_ring_animation();
  }

  // Wake flash
  if (state == VoiceState::WAKE) {
    lv_obj_set_style_bg_opa(glow_overlay_, LV_OPA_80, 0);
    start(glow_overlay_, 80, 10, 800, pulse_opacity, nullptr, 0, 0, 700);
  }
}

// ---- Animations ----

void JarvisAvatar::start_pulse_animation() {
  // Core pulse: scale and opacity
  lv_anim_delete(core_, nullptr);
  lv_anim_delete(core_highlight_, nullptr);

  uint32_t pulse_ms = 2000;
  switch (state_) {
    case VoiceState::LISTENING: pulse_ms = 800; break;
    case VoiceState::PROCESSING: pulse_ms = 600; break;
    case VoiceState::SPEAKING: pulse_ms = 400; break;
    case VoiceState::ERROR: pulse_ms = 1200; break;
    case VoiceState::WAKE: pulse_ms = 300; break;
    case VoiceState::SLEEP: pulse_ms = 4000; break;
    case VoiceState::UNKNOWN: pulse_ms = 3000; break;
    default: break;
  }

  // Core scale pulse
  start(core_, 850, 1000, pulse_ms, nullptr, pulse_scale, 0, 0, pulse_ms / 2, LV_ANIM_REPEAT_INFINITE);
  // Core opacity pulse
  start(core_, 700, 1000, pulse_ms, nullptr, pulse_opacity, 0, 0, pulse_ms / 2, LV_ANIM_REPEAT_INFINITE);
  // Highlight subtle pulse
  start(core_highlight_, 500, 700, pulse_ms * 1.2f, nullptr, pulse_opacity, 0, pulse_ms / 4, pulse_ms / 2, LV_ANIM_REPEAT_INFINITE);
}

void JarvisAvatar::start_ring_animation() {
  // Middle ring slow rotation
  lv_anim_delete(mid_ring_, nullptr);
  uint32_t rot_ms = (state_ == VoiceState::PROCESSING) ? 2000 :
                    (state_ == VoiceState::SPEAKING) ? 3000 : 8000;
  start(mid_ring_, 0, 3600, rot_ms, nullptr, rotate_ring, 0, 0, 0, LV_ANIM_REPEAT_INFINITE);

  // Outer ring subtle counter-rotation
  lv_anim_delete(outer_ring_, nullptr);
  if (state_ == VoiceState::LISTENING || state_ == VoiceState::SPEAKING) {
    start(outer_ring_, 3600, 0, rot_ms * 1.5f, nullptr, rotate_ring, 0, 0, 0, LV_ANIM_REPEAT_INFINITE);
  }
}

void JarvisAvatar::stop_animations() {
  lv_anim_delete(core_, nullptr);
  lv_anim_delete(core_highlight_, nullptr);
  lv_anim_delete(mid_ring_, nullptr);
  lv_anim_delete(outer_ring_, nullptr);
  lv_anim_delete(glow_overlay_, nullptr);
  for (auto *bar : voice_bars_) {
    lv_anim_delete(bar, nullptr);
  }
}

// ---- Theme update ----

void JarvisAvatar::on_theme() {
  if (!root_) return;
  // Re-apply current state to pick up theme-aware colours
  if (expressed_) apply_state(state_);
}

// ---- Idle animations ----

void JarvisAvatar::on_tick(uint32_t epoch, bool quiet) {
  if (!root_ || quiet || !epoch) return;

  const bool asleep = (state_ == VoiceState::SLEEP);
  if (asleep) return;

  // Voice bars animation when speaking
  if (state_ == VoiceState::SPEAKING) {
    for (int i = 0; i < 5; ++i) {
      lv_anim_delete(voice_bars_[i], nullptr);
      int h = at(0.08f + 0.06f * std::sin(epoch * 0.5f + i * 1.2f));
      h = std::max(2, h);
      start(voice_bars_[i], 2, h, 150 + i * 30, nullptr, bar_height, 0, 0, 200, LV_ANIM_REPEAT_INFINITE);
    }
  }

  // Status dot blink for error state
  if (state_ == VoiceState::ERROR && epoch % 2 == 0) {
    lv_anim_delete(status_dot_, nullptr);
    start(status_dot_, LV_OPA_30, LV_OPA_100, 400, fade, nullptr, 0, 0, 400, LV_ANIM_REPEAT_INFINITE);
  }
}

// ---- Wake animation ----

void JarvisAvatar::wake() {
  if (!root_) return;
  // Bright flash
  lv_anim_delete(glow_overlay_, nullptr);
  lv_anim_delete(outer_ring_, nullptr);
  start(glow_overlay_, 10, 80, 200, pulse_opacity, nullptr, 0, 0, 600);
  // Ring expansion
  start(outer_ring_, 1000, 1200, 300, nullptr, pulse_scale, 0, 0, 500);
}

// ---- Touch detection ----

bool JarvisAvatar::hit(int px, int py) const {
  if (!root_) return false;
  const int margin = ui::px(8);
  return px >= x_ - margin && px <= x_ + size_ + margin &&
         py >= y_ - margin && py <= y_ + size_ + margin;
}

}  // namespace esphome::jarvis_avatar
