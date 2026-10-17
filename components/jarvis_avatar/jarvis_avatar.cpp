#include "jarvis_avatar.h"
#include <algorithm>
#include <cmath>

namespace esphome::jarvis_avatar {

namespace ui = tessera::ui;

// ---- Animation helpers (generic, from MicroPlant) ----

namespace {

void lift(void *var, int32_t v) { lv_obj_set_style_translate_y(static_cast<lv_obj_t *>(var), v, 0); }

void fade(void *var, int32_t v) { lv_obj_set_style_opa(static_cast<lv_obj_t *>(var), static_cast<lv_opa_t>(v), 0); }

void scale_width(void *var, int32_t v) {
  auto *o = static_cast<lv_obj_t *>(var);
  lv_obj_set_width(o, v);
  lv_obj_set_x(o, lv_obj_get_x(o) + (lv_obj_get_width(o) - v) / 2);  // keep centred
}

// Ring pulse: opacity cycles
void pulse(lv_anim_t *a, int32_t v) {
  auto *o = static_cast<lv_obj_t *>(a->var);
  lv_obj_set_style_opa(o, static_cast<lv_opa_t>(v), 0);
}

// Processing dot: translate Y with fade
void dot_bounce(lv_anim_t *a, int32_t v) {
  auto *o = static_cast<lv_obj_t *>(a->var);
  const int reach = static_cast<int>(reinterpret_cast<intptr_t>(lv_anim_get_user_data(a)));
  lv_obj_set_style_translate_y(o, -reach * v / 255, 0);
  lv_obj_set_style_opa(o, static_cast<lv_opa_t>(v), 0);
}

// Speaking mouth: sine wave openness
void mouth_move(lv_anim_t *a, int32_t v) {
  auto *o = static_cast<lv_obj_t *>(a->var);
  const int base_h = static_cast<int>(reinterpret_cast<intptr_t>(lv_anim_get_user_data(a)));
  const int h = base_h + (base_h * v / 255);
  lv_obj_set_height(o, h);
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

lv_obj_t *dot(lv_obj_t *parent, theme::Role role) {
  lv_obj_t *o = ui::block(parent, role);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  return o;
}

lv_obj_t *stroke(lv_obj_t *parent, int size) {
  lv_obj_t *o = lv_line_create(parent);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(o, 0, 0);
  lv_obj_set_size(o, size, size);
  lv_obj_set_style_line_rounded(o, true, 0);
  return o;
}

}  // namespace

// ---- Expression definitions ----

enum class MouthShape : uint8_t { FLAT, SMILE, GRIN, OPEN, FROWN, WAVY };

struct JarvisAvatar::Expression {
  float eye_open;       // 0 = closed, 1 = wide
  float brow_raise;     // 0 = neutral, 1 = raised, -1 = furrowed
  MouthShape mouth;
  bool glow;            // ring/core active
  bool dots;            // processing dots visible
  const char *glyph;    // text symbol or empty
  theme::Role tint;     // colour accent
  uint8_t pulse_speed;  // 0 = none, higher = faster (used as divisor)
};

static JarvisAvatar::Expression expr_of(VoiceState state, uint8_t variation) {
  using E = JarvisAvatar::Expression;
  switch (state) {
    case VoiceState::IDLE:
      // Subtle variation: occasional gentle smile
      if (variation % 4 == 0)
        return E{0.85f, 0.0f, MouthShape::SMILE, true, false, "", theme::MARK_BLUE, 3};
      return E{0.85f, 0.0f, MouthShape::FLAT, true, false, "", theme::MARK_BLUE, 3};

    case VoiceState::LISTENING:
      return E{1.0f, 0.3f, MouthShape::FLAT, true, false, "", theme::MARK_GREEN, 1};

    case VoiceState::PROCESSING:
      return E{0.7f, 0.1f, MouthShape::FLAT, true, true, "", theme::MARK_AMBER, 2};

    case VoiceState::SPEAKING:
      return E{0.9f, 0.0f, MouthShape::OPEN, true, false, "", theme::MARK_BLUE, 0};

    case VoiceState::ERROR:
      return E{0.6f, -0.4f, MouthShape::FROWN, false, false, "?", theme::MARK_RED, 0};

    case VoiceState::WAKE:
      return E{1.0f, 0.5f, MouthShape::GRIN, true, false, "", theme::MARK_GREEN, 0};

    case VoiceState::SLEEP:
      return E{0.15f, 0.0f, MouthShape::FLAT, false, false, "z", theme::MARK_PURPLE, 0};

    case VoiceState::UNKNOWN:
    default:
      return E{0.5f, 0.0f, MouthShape::FLAT, false, false, "...", theme::MUTED, 0};
  }
}

// ---- Build the avatar ----

void JarvisAvatar::create(lv_obj_t *parent, int x, int y, int size) {
  x_ = x;
  y_ = y;
  size_ = size < 24 ? 24 : size;
  root_ = box(parent, x, y, size_);

  // Face background circle (subtle fill)
  face_ = dot(root_, theme::CARD);
  const int face_d = at(0.92f);
  lv_obj_set_size(face_, face_d, face_d);
  lv_obj_set_pos(face_, at(0.04f), at(0.04f));
  lv_obj_set_style_bg_opa(face_, LV_OPA_20, 0);

  // Outer ring / halo
  ring_ = dot(root_, theme::MARK_BLUE);
  const int ring_d = at(1.0f);
  lv_obj_set_size(ring_, ring_d, ring_d);
  lv_obj_set_pos(ring_, 0, 0);
  lv_obj_set_style_border_width(ring_, at(0.04f) < 2 ? 2 : at(0.04f), 0);
  lv_obj_set_style_border_opa(ring_, LV_OPA_40, 0);
  lv_obj_set_style_bg_opa(ring_, LV_OPA_TRANSP, 0);

  // Core glow dot (centre, behind face)
  core_ = dot(root_, theme::MARK_BLUE);
  const int core_d = at(0.25f);
  lv_obj_set_size(core_, core_d, core_d);
  lv_obj_set_pos(core_, at(0.5f) - core_d / 2, at(0.5f) - core_d / 2);
  lv_obj_set_style_opa(core_, LV_OPA_30, 0);

  // Eyes
  eye_size_ = at(0.08f);
  eye_size_ = eye_size_ < 4 ? 4 : eye_size_;
  for (auto **eye : {&eye_l_, &eye_r_}) {
    *eye = dot(root_, theme::QR_DARK);
    lv_obj_set_size(*eye, eye_size_, eye_size_);
  }
  // Position: left and right of centre
  lv_obj_set_pos(eye_l_, at(0.35f) - eye_size_ / 2, at(0.38f) - eye_size_ / 2);
  lv_obj_set_pos(eye_r_, at(0.65f) - eye_size_ / 2, at(0.38f) - eye_size_ / 2);

  // Eyebrows (thin lines above eyes)
  brow_l_ = stroke(root_, size_);
  brow_r_ = stroke(root_, size_);
  lv_obj_set_style_line_width(brow_l_, std::max(2, at(0.025f)), 0);
  lv_obj_set_style_line_width(brow_r_, std::max(2, at(0.025f)), 0);

  // Mouth (polyline)
  mouth_ = stroke(root_, size_);
  lv_obj_set_style_line_width(mouth_, std::max(2, at(0.03f)), 0);

  // Processing dots (hidden by default)
  for (auto **d : {&dot_l_, &dot_c_, &dot_r_}) {
    *d = dot(root_, theme::MARK_AMBER);
    const int dd = at(0.05f);
    lv_obj_set_size(*d, dd, dd);
    lv_obj_set_style_opa(*d, LV_OPA_TRANSP, 0);
  }
  lv_obj_set_pos(dot_l_, at(0.35f) - at(0.025f), at(0.55f));
  lv_obj_set_pos(dot_c_, at(0.5f) - at(0.025f), at(0.55f));
  lv_obj_set_pos(dot_r_, at(0.65f) - at(0.025f), at(0.55f));

  // Glyph label
  const tessera::Font glyph_font = size_ >= ui::px(120) ? tessera::Font::HEADLINE : tessera::Font::BODY;
  glyph_ = ui::label(root_, glyph_font, theme::MUTED);
  lv_obj_set_size(glyph_, at(0.3f), ui::line_height(glyph_font));
  lv_obj_set_pos(glyph_, at(0.7f), at(0.15f));
  ui::set_text(glyph_, "");

  on_theme();
  apply(expr_of(VoiceState::UNKNOWN, 0));
  state_ = VoiceState::UNKNOWN;
  expressed_ = true;
}

// ---- Apply expression to face ----

void JarvisAvatar::set_state(VoiceState state) {
  if (!root_ || (expressed_ && state == state_)) return;
  state_ = state;
  expr_idx_ = 0;
  apply(expr_of(state, 0));
}

void JarvisAvatar::apply(const Expression &expr) {
  // Eye openness (height)
  const int eh = std::max(2, static_cast<int>(eye_size_ * expr.eye_open + 0.5f));
  for (auto *eye : {eye_l_, eye_r_}) {
    lv_anim_delete(eye, nullptr);
    lv_obj_set_height(eye, eh);
    lv_obj_set_y(eye, lv_obj_get_y(eye) + (eye_size_ - eh) / 2);  // recentre
  }

  // Eyebrows: angle and position based on raise
  const float brow_y = 0.30f + expr.brow_raise * 0.04f;
  const float brow_slant = expr.brow_raise * 0.03f;
  lv_point_precise_t brow_pts[2];

  // Left brow
  brow_pts[0] = {at(0.30f), at(brow_y + brow_slant)};
  brow_pts[1] = {at(0.42f), at(brow_y - brow_slant)};
  place_line(brow_l_, brow_pts, 2);

  // Right brow
  brow_pts[0] = {at(0.58f), at(brow_y - brow_slant)};
  brow_pts[1] = {at(0.70f), at(brow_y + brow_slant)};
  place_line(brow_r_, brow_pts, 2);

  // Mouth shapes (5-point polyline, like MicroPlant)
  static constexpr float MOUTH_DEFS[6][5] = {
    {0.015f, 0.015f, 0.015f, 0.015f, 0.015f},  // FLAT
    {0.0f,   0.022f, 0.03f, 0.022f, 0.0f},    // SMILE
    {-0.01f, 0.03f,  0.045f, 0.03f, -0.01f},  // GRIN
    {0.015f, 0.015f, 0.025f, 0.015f, 0.015f}, // OPEN (slightly taller centre)
    {0.035f, 0.012f, 0.004f, 0.012f, 0.035f},  // FROWN
    {0.022f, 0.006f, 0.022f, 0.006f, 0.022f}, // WAVY
  };
  const int mi = static_cast<int>(expr.mouth);
  const float half = (expr.mouth == MouthShape::OPEN) ? 0.08f : 0.07f;
  for (int i = 0; i < 5; ++i) {
    mouth_pts_[i] = {at(0.5f - half + half * 0.5f * i),
                     at(0.60f + MOUTH_DEFS[mi][i])};
  }
  place_line(mouth_, mouth_pts_, 5);

  // Ring glow
  lv_obj_set_style_border_color(ring_, ui::color(expr.tint), 0);
  lv_obj_set_style_opa(ring_, expr.glow ? LV_OPA_40 : LV_OPA_TRANSP, 0);

  // Core glow
  lv_obj_set_style_bg_color(core_, ui::color(expr.tint), 0);
  lv_obj_set_style_opa(core_, expr.glow ? LV_OPA_30 : LV_OPA_TRANSP, 0);

  // Processing dots
  for (auto *d : {dot_l_, dot_c_, dot_r_}) {
    lv_obj_set_style_opa(d, expr.dots ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  }

  // Glyph
  ui::set_text(glyph_, expr.glyph ? expr.glyph : "");
  ui::set_color(glyph_, expr.tint);
}

// ---- Theme update ----

void JarvisAvatar::on_theme() {
  if (!root_) return;
  lv_obj_set_style_border_color(ring_, ui::color(theme::MARK_BLUE), 0);
  lv_obj_set_style_bg_color(face_, ui::color(theme::CARD), 0);
  for (auto *eye : {eye_l_, eye_r_}) {
    lv_obj_set_style_bg_color(eye, ui::color(theme::QR_DARK), 0);
  }
  lv_obj_set_style_line_color(brow_l_, ui::color(theme::QR_DARK), 0);
  lv_obj_set_style_line_color(brow_r_, ui::color(theme::QR_DARK), 0);
  lv_obj_set_style_line_color(mouth_, ui::color(theme::QR_DARK), 0);
  if (expressed_) apply(expr_of(state_, expr_idx_));
}

// ---- Idle animations ----

void JarvisAvatar::on_tick(uint32_t epoch, bool quiet) {
  if (!root_ || quiet || !epoch) return;

  const bool asleep = (state_ == VoiceState::SLEEP);
  if (asleep) return;  // no animations in sleep

  // Blink every ~5 seconds, but not if eyes are nearly closed
  if (epoch % config::BLINK_INTERVAL_SECONDS == 2) {
    auto expr = expr_of(state_, expr_idx_);
    if (expr.eye_open > 0.4f) {
      for (auto *eye : {eye_l_, eye_r_}) {
        const int h = lv_obj_get_height(eye);
        start(eye, h, 2, 80, nullptr, [](lv_anim_t *a, int32_t v) {
          auto *e = static_cast<lv_obj_t *>(a->var);
          lv_obj_set_height(e, v);
        }, 0, 0, 100);
      }
    }
  }

  // State-specific ambient animations
  switch (state_) {
    case VoiceState::IDLE:
      // Slow ring pulse
      if (epoch % config::IDLE_PULSE_SECONDS == 0) {
        start(ring_, 40, 80, 1200, nullptr, pulse, 0, 0, 1200, LV_ANIM_REPEAT_INFINITE);
      }
      // Occasionally vary expression for liveliness
      if (epoch % 11 == 0) {
        expr_idx_++;
        apply(expr_of(state_, expr_idx_));
      }
      break;

    case VoiceState::LISTENING:
      // Fast ring pulse
      if (epoch % config::LISTENING_PULSE_SECONDS == 0) {
        start(ring_, 60, 160, 600, nullptr, pulse, 0, 0, 600, LV_ANIM_REPEAT_INFINITE);
      }
      break;

    case VoiceState::PROCESSING:
      // Three dots bounce in sequence
      if (epoch % config::PROCESSING_DOT_SECONDS == 0) {
        for (int i = 0; i < 3; ++i) {
          lv_anim_delete((&dot_l_)[i], nullptr);
          start((&dot_l_)[i], 0, 255, 400, nullptr, dot_bounce,
                at(0.04f), i * 150, 400, LV_ANIM_REPEAT_INFINITE);
        }
      }
      break;

    case VoiceState::SPEAKING:
      // Mouth "breathing" open/close
      if (epoch % 1 == 0) {  // every second
        lv_anim_delete(mouth_, nullptr);
        const int base_h = at(0.03f);
        start(mouth_, base_h, base_h * 2, config::SPEAKING_MOUTH_SPEED_MS,
              nullptr, [](lv_anim_t *a, int32_t v) {
                auto *o = static_cast<lv_obj_t *>(a->var);
                lv_obj_set_style_line_width(o, v, 0);
              }, base_h, 0, config::SPEAKING_MOUTH_SPEED_MS, LV_ANIM_REPEAT_INFINITE);
      }
      break;

    default:
      break;
  }
}

// ---- Wake animation ----

void JarvisAvatar::wake() {
  if (!root_) return;
  // Brief scale-up and glow flare
  start(core_, LV_OPA_30, LV_OPA_100, 300, fade, nullptr, 0, 0, 600);
  start(ring_, LV_OPA_40, LV_OPA_80, 300, fade, nullptr, 0, 0, 600);
}

// ---- Touch detection ----

bool JarvisAvatar::hit(int px, int py) const {
  if (!root_) return false;
  const int margin = ui::px(8);
  return px >= x_ - margin && px <= x_ + size_ + margin &&
         py >= y_ - margin && py <= y_ + size_ + margin;
}

// ---- Helpers ----

void JarvisAvatar::place_line(lv_obj_t *line, lv_point_precise_t *points, int count) {
  lv_line_set_points(line, points, static_cast<uint32_t>(count));
}

}  // namespace esphome::jarvis_avatar
