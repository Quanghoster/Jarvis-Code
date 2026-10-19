#include "jarvis_reactor.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace esphome::jarvis_reactor {

namespace ui = tessera::ui;
using tessera::Font;

// ---- Colours ----
inline const lv_color_t CYAN = lv_color_hex(0x00D4FF);
inline const lv_color_t CYAN_DIM = lv_color_hex(0x006680);
inline const lv_color_t CYAN_GLOW = lv_color_hex(0x003344);
inline const lv_color_t CORE_INNER = lv_color_hex(0xAADDFF);
inline const lv_color_t CORE_MID = lv_color_hex(0x00D4FF);
inline const lv_color_t CORE_OUTER = lv_color_hex(0x002244);
inline const lv_color_t RING_BORDER = lv_color_hex(0x00D4FF);
inline const lv_color_t RING_DIM = lv_color_hex(0x0088AA);
inline const lv_color_t AMBER = lv_color_hex(0xFFAA00);
inline const lv_color_t RED = lv_color_hex(0xFF3333);
inline const lv_color_t GREEN = lv_color_hex(0x00FF66);
inline const lv_color_t PURPLE = lv_color_hex(0xAA66FF);
inline const lv_color_t OFF = lv_color_hex(0x112233);
inline const lv_color_t DARK_BG = lv_color_hex(0x050A14);
inline const lv_color_t HUD_BG = lv_color_hex(0x0A1428);
inline const lv_color_t HUD_SCAN = lv_color_hex(0x001A22);

// ---- Voice state ----
enum class VoiceState : uint8_t {
  UNKNOWN, IDLE, LISTENING, PROCESSING, SPEAKING, ERROR, WAKE, SLEEP
};

inline VoiceState parse_state(const char* text) {
  if (!text || !*text) return VoiceState::UNKNOWN;
  char low[32];
  size_t n = 0;
  for (; text[n] && n + 1 < sizeof(low); ++n) {
    char c = text[n];
    low[n] = (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
  }
  low[n] = 0;
  if (strstr(low, "listen")) return VoiceState::LISTENING;
  if (strstr(low, "process")) return VoiceState::PROCESSING;
  if (strstr(low, "speak")) return VoiceState::SPEAKING;
  if (strstr(low, "error")) return VoiceState::ERROR;
  if (strstr(low, "wake")) return VoiceState::WAKE;
  if (strstr(low, "sleep")) return VoiceState::SLEEP;
  if (strstr(low, "idle")) return VoiceState::IDLE;
  return VoiceState::UNKNOWN;
}

// ---- Shared painter interface ----
class AvatarPainter {
 public:
  virtual ~AvatarPainter() = default;
  virtual void create(const tessera::TileContext &c, const tessera::Plugin *plugin) = 0;
  virtual void on_state(VoiceState state) = 0;
  virtual void on_data(JsonObjectConst data) {}
  virtual void on_tick(uint32_t epoch) = 0;
  virtual void on_theme() = 0;
  virtual void on_tap() {}
};

// ============================================================================
// ReactorPainter: pulsing core reactor (no concentric arcs)
// ============================================================================
class ReactorPainter : public AvatarPainter {
 public:
  void create(const tessera::TileContext &c, const tessera::Plugin *plugin) override {
    plugin_ = plugin;
    width_ = c.width;
    height_ = c.height;
    const int cx = width_ / 2;
    const int cy = height_ / 2;
    const int size = std::min(width_, height_);
    const float s = size / 240.0f;

    bg_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(bg_);
    lv_obj_remove_flag(bg_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(bg_, 0, 0);
    lv_obj_set_style_bg_color(bg_, DARK_BG, 0);
    lv_obj_set_style_bg_opa(bg_, LV_OPA_100, 0);
    lv_obj_set_size(bg_, width_, height_);
    lv_obj_set_pos(bg_, 0, 0);

    core_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(core_);
    lv_obj_remove_flag(core_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(core_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(core_, CORE_MID, 0);
    lv_obj_set_style_shadow_color(core_, CYAN, 0);
    lv_obj_set_style_shadow_width(core_, static_cast<lv_coord_t>(20 * s), 0);
    lv_obj_set_style_shadow_opa(core_, LV_OPA_60, 0);
    const int core_d = static_cast<int>(size * 0.35f);
    lv_obj_set_size(core_, core_d, core_d);
    lv_obj_set_pos(core_, cx - core_d / 2, cy - core_d / 2);

    highlight_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(highlight_);
    lv_obj_remove_flag(highlight_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(highlight_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(highlight_, CORE_INNER, 0);
    lv_obj_set_style_opa(highlight_, LV_OPA_60, 0);
    const int hl_d = static_cast<int>(size * 0.12f);
    lv_obj_set_size(highlight_, hl_d, hl_d);
    lv_obj_set_pos(highlight_, cx - hl_d / 2 - static_cast<int>(4 * s), cy - hl_d / 2 - static_cast<int>(10 * s));

    tessera::Font title_font = (size >= 200) ? tessera::Font::HEADLINE :
                               (size >= 100) ? tessera::Font::TITLE :
                               tessera::Font::BODY;
    title_ = ui::label(c.parent, title_font, theme::INK);
    lv_obj_set_width(title_, width_);
    lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title_, CYAN, 0);
    ui::set_text(title_, "J.A.R.V.I.S.");
    const int text_y = cy - ui::line_height(title_font) / 2;
    lv_obj_set_pos(title_, 0, text_y);

    status_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(status_);
    lv_obj_remove_flag(status_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(status_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_, OFF, 0);
    lv_obj_set_style_shadow_color(status_, OFF, 0);
    lv_obj_set_style_shadow_width(status_, static_cast<lv_coord_t>(6 * s), 0);
    const int sd_d = std::max(3, static_cast<int>(6 * s));
    lv_obj_set_size(status_, sd_d, sd_d);
    lv_obj_set_pos(status_, cx - sd_d / 2, cy - static_cast<int>(size * 0.42f));

    const int bar_w = std::max(2, static_cast<int>(3 * s));
    const int bar_gap = std::max(3, static_cast<int>(5 * s));
    for (int i = 0; i < 5; ++i) {
      bars_[i] = lv_obj_create(c.parent);
      lv_obj_remove_style_all(bars_[i]);
      lv_obj_remove_flag(bars_[i], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
      lv_obj_set_style_bg_color(bars_[i], CYAN, 0);
      lv_obj_set_style_radius(bars_[i], bar_w / 2, 0);
      lv_obj_set_size(bars_[i], bar_w, 2);
      lv_obj_set_pos(bars_[i],
                      cx - (2 * bar_gap + 2 * bar_w) + i * (bar_w + bar_gap),
                      cy + static_cast<int>(size * 0.38f));
      lv_obj_set_style_opa(bars_[i], LV_OPA_TRANSP, 0);
    }

    apply_state(state_);
  }

  void on_state(VoiceState state) override {
    if (state != state_) {
      state_ = state;
      apply_state(state_);
    }
  }

  void on_tick(uint32_t epoch) override {
    if (!epoch) return;
    const float phase = static_cast<float>(epoch % 3) / 3.0f * 6.2831853f;
    const float breathe = 0.93f + 0.07f * sin(phase);

    const int core_base = std::min(width_, height_);
    const int core_d = static_cast<int>(core_base * 0.35f * breathe);
    const int cx = width_ / 2;
    const int cy = height_ / 2;
    lv_obj_set_size(core_, core_d, core_d);
    lv_obj_set_pos(core_, cx - core_d / 2, cy - core_d / 2);

    if (state_ == VoiceState::SPEAKING) {
      for (int i = 0; i < 5; ++i) {
        int h = 2 + static_cast<int>(12 * sin(static_cast<float>(epoch) * 0.5f + i * 1.2f));
        h = std::max(2, h);
        lv_obj_set_height(bars_[i], h);
        lv_obj_set_y(bars_[i], lv_obj_get_y(bars_[i]) + (lv_obj_get_height(bars_[i]) - h) / 2);
      }
    }

    if (state_ == VoiceState::ERROR && (epoch % 2 == 0)) {
      static bool blink = false;
      blink = !blink;
      lv_obj_set_style_opa(status_, blink ? LV_OPA_100 : LV_OPA_30, 0);
    }
  }

  void on_theme() override {
    apply_state(state_);
    lv_obj_set_style_text_color(title_, CYAN, 0);
  }

 private:
  void apply_state(VoiceState state) {
    lv_color_t core_col = CORE_MID;
    lv_color_t status_col = OFF;
    lv_opa_t core_opa = LV_OPA_80;

    switch (state) {
      case VoiceState::IDLE:
        core_col = CORE_MID;
        status_col = CYAN_DIM;
        core_opa = LV_OPA_80;
        break;
      case VoiceState::LISTENING:
        core_col = GREEN;
        status_col = GREEN;
        core_opa = LV_OPA_100;
        break;
      case VoiceState::PROCESSING:
        core_col = AMBER;
        status_col = AMBER;
        core_opa = LV_OPA_90;
        break;
      case VoiceState::SPEAKING:
        core_col = CORE_INNER;
        status_col = CYAN;
        core_opa = LV_OPA_100;
        break;
      case VoiceState::ERROR:
        core_col = RED;
        status_col = RED;
        core_opa = LV_OPA_70;
        break;
      case VoiceState::WAKE:
        core_col = CORE_INNER;
        status_col = GREEN;
        core_opa = LV_OPA_100;
        break;
      case VoiceState::SLEEP:
        core_col = OFF;
        status_col = PURPLE;
        core_opa = LV_OPA_30;
        break;
      case VoiceState::UNKNOWN:
      default:
        core_col = OFF;
        status_col = OFF;
        core_opa = LV_OPA_50;
        break;
    }

    lv_obj_set_style_shadow_color(core_, core_col, 0);
    lv_obj_set_style_bg_color(core_, core_col, 0);
    lv_obj_set_style_bg_color(highlight_, core_col, 0);
    lv_obj_set_style_bg_color(status_, status_col, 0);
    lv_obj_set_style_shadow_color(status_, status_col, 0);
    lv_obj_set_style_bg_opa(core_, core_opa, 0);

    for (auto *bar : bars_) {
      lv_obj_set_style_opa(bar, (state == VoiceState::SPEAKING) ? LV_OPA_80 : LV_OPA_TRANSP, 0);
    }

    lv_obj_set_style_text_color(title_, CYAN, 0);
  }

  const tessera::Plugin *plugin_ = nullptr;
  int width_ = 0, height_ = 0;
  VoiceState state_ = VoiceState::IDLE;
  lv_obj_t *bg_{};
  lv_obj_t *core_{};
  lv_obj_t *highlight_{};
  lv_obj_t *title_{};
  lv_obj_t *status_{};
  lv_obj_t *bars_[5]{};
};

// ============================================================================
// HudPainter: cleaner View Assist dashboard style
// ============================================================================
class HudPainter : public AvatarPainter {
 public:
  void create(const tessera::TileContext &c, const tessera::Plugin *plugin) override {
    plugin_ = plugin;
    width_ = c.width;
    height_ = c.height;
    const int cx = width_ / 2;
    const int cy = height_ / 2;
    const int size = std::min(width_, height_);
    const float s = size / 240.0f;

    bg_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(bg_);
    lv_obj_remove_flag(bg_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(bg_, 0, 0);
    lv_obj_set_style_bg_color(bg_, HUD_BG, 0);
    lv_obj_set_style_bg_opa(bg_, LV_OPA_100, 0);
    lv_obj_set_size(bg_, width_, height_);
    lv_obj_set_pos(bg_, 0, 0);

    for (int i = 0; i < 8; ++i) {
      scanlines_[i] = lv_obj_create(c.parent);
      lv_obj_remove_style_all(scanlines_[i]);
      lv_obj_remove_flag(scanlines_[i], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
      lv_obj_set_style_bg_color(scanlines_[i], CYAN, 0);
      lv_obj_set_style_bg_opa(scanlines_[i], LV_OPA_10, 0);
      const int sh = std::max(1, static_cast<int>(1 * s));
      lv_obj_set_size(scanlines_[i], width_, sh);
      lv_obj_set_pos(scanlines_[i], 0, static_cast<int>((height_ * 0.2f) + i * height_ * 0.075f));
    }

    ring_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(ring_);
    lv_obj_remove_flag(ring_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(ring_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(ring_, CYAN, 0);
    lv_obj_set_style_border_width(ring_, std::max(2, static_cast<int>(3 * s)), 0);
    lv_obj_set_style_border_opa(ring_, LV_OPA_60, 0);
    lv_obj_set_style_shadow_color(ring_, CYAN, 0);
    lv_obj_set_style_shadow_width(ring_, static_cast<lv_coord_t>(30 * s), 0);
    lv_obj_set_style_shadow_opa(ring_, LV_OPA_40, 0);
    const int ring_d = static_cast<int>(size * 0.70f);
    lv_obj_set_size(ring_, ring_d, ring_d);
    lv_obj_set_pos(ring_, cx - ring_d / 2, cy - ring_d / 2 - static_cast<int>(6 * s));

    core_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(core_);
    lv_obj_remove_flag(core_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(core_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(core_, CORE_MID, 0);
    lv_obj_set_style_shadow_color(core_, CYAN, 0);
    lv_obj_set_style_shadow_width(core_, static_cast<lv_coord_t>(25 * s), 0);
    lv_obj_set_style_shadow_opa(core_, LV_OPA_70, 0);
    const int core_d = static_cast<int>(size * 0.28f);
    lv_obj_set_size(core_, core_d, core_d);
    lv_obj_set_pos(core_, cx - core_d / 2, cy - core_d / 2 - static_cast<int>(6 * s));

    highlight_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(highlight_);
    lv_obj_remove_flag(highlight_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(highlight_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(highlight_, CORE_INNER, 0);
    lv_obj_set_style_opa(highlight_, LV_OPA_50, 0);
    const int hl_d = static_cast<int>(core_d * 0.35f);
    lv_obj_set_size(highlight_, hl_d, hl_d);
    lv_obj_set_pos(highlight_, cx - hl_d / 2 - static_cast<int>(3 * s), cy - hl_d / 2 - static_cast<int>(8 * s));

    status_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(status_);
    lv_obj_remove_flag(status_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(status_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_, CYAN_DIM, 0);
    lv_obj_set_style_shadow_color(status_, CYAN, 0);
    lv_obj_set_style_shadow_width(status_, static_cast<lv_coord_t>(8 * s), 0);
    const int sd = std::max(4, static_cast<int>(8 * s));
    lv_obj_set_size(status_, sd, sd);
    lv_obj_set_pos(status_, cx - sd / 2, cy + static_cast<int>(size * 0.42f));

    tessera::Font title_font = (size >= 200) ? tessera::Font::HEADLINE :
                               (size >= 100) ? tessera::Font::TITLE :
                               tessera::Font::BODY;
    title_ = ui::label(c.parent, title_font, theme::INK);
    lv_obj_set_width(title_, width_);
    lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title_, CYAN, 0);
    ui::set_text(title_, "J.A.R.V.I.S.");
    lv_obj_set_pos(title_, 0, cy + static_cast<int>(size * 0.46f) - ui::line_height(title_font) / 2);

    state_label_ = ui::label(c.parent, Font::BODY, theme::INK_SOFT);
    lv_obj_set_width(state_label_, width_);
    lv_obj_set_style_text_align(state_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(state_label_, CYAN, 0);
    ui::set_text(state_label_, "Idle");
    if (size >= 160) {
      lv_obj_set_pos(state_label_, 0, height_ - ui::line_height(Font::BODY) - static_cast<int>(4 * s));
    } else {
      lv_obj_add_flag(state_label_, LV_OBJ_FLAG_HIDDEN);
    }

    const int bar_w = std::max(2, static_cast<int>(3 * s));
    const int bar_gap = std::max(3, static_cast<int>(5 * s));
    for (int i = 0; i < 5; ++i) {
      bars_[i] = lv_obj_create(c.parent);
      lv_obj_remove_style_all(bars_[i]);
      lv_obj_remove_flag(bars_[i], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
      lv_obj_set_style_bg_color(bars_[i], CYAN, 0);
      lv_obj_set_style_radius(bars_[i], bar_w / 2, 0);
      lv_obj_set_size(bars_[i], bar_w, 2);
      lv_obj_set_pos(bars_[i],
                      cx - (2 * bar_gap + 2 * bar_w) + i * (bar_w + bar_gap),
                      cy + static_cast<int>(size * 0.32f));
      lv_obj_set_style_opa(bars_[i], LV_OPA_TRANSP, 0);
    }

    apply_state(state_);
  }

  void on_state(VoiceState state) override {
    if (state != state_) {
      state_ = state;
      apply_state(state_);
    }
  }

  void on_tick(uint32_t epoch) override {
    if (!epoch) return;
    const int size = std::min(width_, height_);
    const float s = size / 240.0f;
    const int cx = width_ / 2;
    const int cy = height_ / 2;

    float speed = 3.0f;
    switch (state_) {
      case VoiceState::IDLE: speed = 3.0f; break;
      case VoiceState::LISTENING: speed = 1.2f; break;
      case VoiceState::PROCESSING: speed = 0.8f; break;
      case VoiceState::SPEAKING: speed = 0.5f; break;
      case VoiceState::ERROR: speed = 2.0f; break;
      case VoiceState::WAKE: speed = 0.3f; break;
      case VoiceState::SLEEP: speed = 6.0f; break;
      default: speed = 3.0f; break;
    }

    const float phase = static_cast<float>(epoch) / speed * 6.2831853f;
    const float breathe = 0.92f + 0.08f * sin(phase);
    const int core_d = static_cast<int>(size * 0.28f * breathe);
    lv_obj_set_size(core_, core_d, core_d);
    lv_obj_set_pos(core_, cx - core_d / 2, cy - core_d / 2 - static_cast<int>(6 * s));

    const int ring_d = static_cast<int>(size * 0.70f + 4 * s * sin(phase + 1.0f));
    lv_obj_set_size(ring_, ring_d, ring_d);
    lv_obj_set_pos(ring_, cx - ring_d / 2, cy - ring_d / 2 - static_cast<int>(6 * s));

    lv_obj_set_style_border_opa(ring_, static_cast<lv_opa_t>(LV_OPA_50 + LV_OPA_20 * sin(phase)), 0);
    lv_obj_set_style_shadow_opa(ring_, static_cast<lv_opa_t>(LV_OPA_30 + LV_OPA_20 * sin(phase + 0.5f)), 0);

    if (state_ == VoiceState::SPEAKING) {
      for (int i = 0; i < 5; ++i) {
        int h = 2 + static_cast<int>(14 * sin(static_cast<float>(epoch) * 0.6f + i * 1.2f));
        h = std::max(2, std::min(h, static_cast<int>(18 * s)));
        lv_obj_set_height(bars_[i], h);
        lv_obj_set_y(bars_[i], cy + static_cast<int>(size * 0.32f) - h / 2);
        lv_obj_set_style_opa(bars_[i], LV_OPA_80, 0);
      }
    } else {
      for (auto *bar : bars_) {
        lv_obj_set_style_opa(bar, LV_OPA_TRANSP, 0);
      }
    }

    if (state_ == VoiceState::ERROR && (epoch % 2 == 0)) {
      static bool blink = false;
      blink = !blink;
      lv_obj_set_style_opa(status_, blink ? LV_OPA_100 : LV_OPA_30, 0);
    } else {
      lv_obj_set_style_opa(status_, LV_OPA_100, 0);
    }
  }

  void on_theme() override {
    apply_state(state_);
    lv_obj_set_style_text_color(title_, CYAN, 0);
    lv_obj_set_style_text_color(state_label_, CYAN, 0);
  }

 private:
  void apply_state(VoiceState state) {
    lv_color_t ring_col = CYAN;
    lv_color_t core_col = CORE_MID;
    lv_color_t status_col = CYAN_DIM;
    lv_opa_t core_opa = LV_OPA_80;
    const char *label = "Idle";

    switch (state) {
      case VoiceState::IDLE:
        ring_col = CYAN;
        core_col = CORE_MID;
        status_col = CYAN_DIM;
        core_opa = LV_OPA_80;
        label = "Idle";
        break;
      case VoiceState::LISTENING:
        ring_col = GREEN;
        core_col = GREEN;
        status_col = GREEN;
        core_opa = LV_OPA_100;
        label = "Listening";
        break;
      case VoiceState::PROCESSING:
        ring_col = AMBER;
        core_col = AMBER;
        status_col = AMBER;
        core_opa = LV_OPA_90;
        label = "Processing";
        break;
      case VoiceState::SPEAKING:
        ring_col = CYAN;
        core_col = CORE_INNER;
        status_col = CYAN;
        core_opa = LV_OPA_100;
        label = "Speaking";
        break;
      case VoiceState::ERROR:
        ring_col = RED;
        core_col = RED;
        status_col = RED;
        core_opa = LV_OPA_70;
        label = "Error";
        break;
      case VoiceState::WAKE:
        ring_col = GREEN;
        core_col = CORE_INNER;
        status_col = GREEN;
        core_opa = LV_OPA_100;
        label = "Wake";
        break;
      case VoiceState::SLEEP:
        ring_col = OFF;
        core_col = OFF;
        status_col = PURPLE;
        core_opa = LV_OPA_30;
        label = "Sleep";
        break;
      case VoiceState::UNKNOWN:
      default:
        ring_col = RING_DIM;
        core_col = OFF;
        status_col = OFF;
        core_opa = LV_OPA_50;
        label = "Unknown";
        break;
    }

    lv_obj_set_style_border_color(ring_, ring_col, 0);
    lv_obj_set_style_shadow_color(ring_, ring_col, 0);
    lv_obj_set_style_bg_color(core_, core_col, 0);
    lv_obj_set_style_shadow_color(core_, ring_col, 0);
    lv_obj_set_style_bg_opa(core_, core_opa, 0);
    lv_obj_set_style_bg_color(highlight_, core_col, 0);
    lv_obj_set_style_bg_color(status_, status_col, 0);
    lv_obj_set_style_shadow_color(status_, status_col, 0);

    for (auto *bar : bars_) {
      lv_obj_set_style_bg_color(bar, ring_col, 0);
    }

    lv_obj_set_style_text_color(title_, CYAN, 0);
    ui::set_text(state_label_, label);
  }

  const tessera::Plugin *plugin_ = nullptr;
  int width_ = 0, height_ = 0;
  VoiceState state_ = VoiceState::IDLE;
  lv_obj_t *bg_{};
  lv_obj_t *scanlines_[8]{};
  lv_obj_t *ring_{};
  lv_obj_t *core_{};
  lv_obj_t *highlight_{};
  lv_obj_t *status_{};
  lv_obj_t *title_{};
  lv_obj_t *state_label_{};
  lv_obj_t *bars_[5]{};
};

// ============================================================================
// ImagePainter: show a picture with a state-coloured border overlay
// ============================================================================
class ImagePainter : public AvatarPainter {
 public:
  void create(const tessera::TileContext &c, const tessera::Plugin *plugin) override {
    plugin_ = plugin;
    width_ = c.width;
    height_ = c.height;
    const float s = std::min(width_, height_) / 240.0f;

    bg_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(bg_);
    lv_obj_remove_flag(bg_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(bg_, 0, 0);
    lv_obj_set_style_bg_color(bg_, DARK_BG, 0);
    lv_obj_set_style_bg_opa(bg_, LV_OPA_100, 0);
    lv_obj_set_size(bg_, width_, height_);
    lv_obj_set_pos(bg_, 0, 0);

    const char *fit_text = c.options["fit"] | "contain";
    fit_ = strcmp(fit_text, "cover") == 0 ? tessera::Fit::COVER : tessera::Fit::CONTAIN;

    url_ = std::string(c.options["url"] | "");

    if (ui::pictures()) {
      picture_ = ui::picture(c.parent);
      lv_obj_set_size(picture_, width_, height_);
      lv_obj_set_pos(picture_, 0, 0);
    } else {
      fallback_ = ui::label(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_width(fallback_, width_);
      lv_obj_set_style_text_align(fallback_, LV_TEXT_ALIGN_CENTER, 0);
      ui::set_text(fallback_, plugin_->text("no_picture"));
      lv_obj_set_pos(fallback_, 0, (height_ - ui::line_height(Font::BODY)) / 2);
    }

    border_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(border_);
    lv_obj_remove_flag(border_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(border_, static_cast<lv_coord_t>(8 * s), 0);
    lv_obj_set_style_bg_opa(border_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(border_, CYAN, 0);
    lv_obj_set_style_border_width(border_, std::max(2, static_cast<int>(3 * s)), 0);
    lv_obj_set_style_border_opa(border_, LV_OPA_60, 0);
    lv_obj_set_size(border_, width_, height_);
    lv_obj_set_pos(border_, 0, 0);

    status_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(status_);
    lv_obj_remove_flag(status_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(status_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_, CYAN_DIM, 0);
    lv_obj_set_style_shadow_color(status_, CYAN, 0);
    lv_obj_set_style_shadow_width(status_, static_cast<lv_coord_t>(8 * s), 0);
    const int sd = std::max(4, static_cast<int>(10 * s));
    lv_obj_set_size(status_, sd, sd);
    lv_obj_set_pos(status_, width_ - sd - static_cast<int>(8 * s), static_cast<int>(8 * s));

    apply_state(state_);
  }

  void on_state(VoiceState state) override {
    if (state != state_) {
      state_ = state;
      apply_state(state_);
    }
  }

  void on_data(JsonObjectConst data) override {
    load_picture(data);
  }

  void on_tick(uint32_t epoch) override {
    if (!epoch) return;
    if (state_ == VoiceState::ERROR && (epoch % 2 == 0)) {
      static bool blink = false;
      blink = !blink;
      lv_obj_set_style_opa(status_, blink ? LV_OPA_100 : LV_OPA_30, 0);
    } else {
      lv_obj_set_style_opa(status_, LV_OPA_100, 0);
    }
  }

  void on_theme() override {
    apply_state(state_);
  }

 private:
  void load_picture(JsonObjectConst data) {
    if (!picture_) return;

    std::string src;
    if (!url_.empty()) {
      src = url_;
    } else if (data["attributes"]["entity_picture"].is<const char*>()) {
      src = data["attributes"]["entity_picture"].as<const char*>();
    }

    if (!src.empty()) {
      ui::set_picture(picture_, src.c_str(), fit_);
    }
  }

  void apply_state(VoiceState state) {
    lv_color_t col = CYAN;
    lv_opa_t border_opa = LV_OPA_60;

    switch (state) {
      case VoiceState::IDLE: col = CYAN; border_opa = LV_OPA_60; break;
      case VoiceState::LISTENING: col = GREEN; border_opa = LV_OPA_90; break;
      case VoiceState::PROCESSING: col = AMBER; border_opa = LV_OPA_90; break;
      case VoiceState::SPEAKING: col = CYAN; border_opa = LV_OPA_100; break;
      case VoiceState::ERROR: col = RED; border_opa = LV_OPA_80; break;
      case VoiceState::WAKE: col = GREEN; border_opa = LV_OPA_100; break;
      case VoiceState::SLEEP: col = PURPLE; border_opa = LV_OPA_40; break;
      case VoiceState::UNKNOWN:
      default: col = RING_DIM; border_opa = LV_OPA_30; break;
    }

    lv_obj_set_style_border_color(border_, col, 0);
    lv_obj_set_style_border_opa(border_, border_opa, 0);
    lv_obj_set_style_bg_color(status_, col, 0);
    lv_obj_set_style_shadow_color(status_, col, 0);
  }

  const tessera::Plugin *plugin_ = nullptr;
  int width_ = 0, height_ = 0;
  VoiceState state_ = VoiceState::IDLE;
  tessera::Fit fit_ = tessera::Fit::CONTAIN;
  std::string url_;
  lv_obj_t *bg_{};
  lv_obj_t *picture_{};
  lv_obj_t *fallback_{};
  lv_obj_t *border_{};
  lv_obj_t *status_{};
};

// ============================================================================
// AvatarTile: dispatches to the chosen painter
// ============================================================================
class AvatarTile : public tessera::Tile {
 public:
  explicit AvatarTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    const char *variant = c.options["variant"] | "reactor";
    if (strcmp(variant, "hud") == 0) {
      painter_.reset(new HudPainter());
    } else if (strcmp(variant, "image") == 0) {
      painter_.reset(new ImagePainter());
    } else {
      painter_.reset(new ReactorPainter());
    }
    if (painter_) {
      painter_->create(c, plugin_);
    }
  }

  void on_state(JsonObjectConst data) override {
    VoiceState new_state = VoiceState::UNKNOWN;
    if (data["state"].is<const char*>()) {
      new_state = parse_state(data["state"]);
    }
    state_ = new_state;
    if (painter_) {
      painter_->on_state(state_);
      painter_->on_data(data);
    }
  }

  void on_tick(uint32_t epoch) override {
    if (painter_) painter_->on_tick(epoch);
  }

  void on_theme() override {
    if (painter_) painter_->on_theme();
  }

  void on_tap() override {
    if (painter_) painter_->on_tap();
  }

 private:
  const tessera::Plugin *plugin_;
  std::unique_ptr<AvatarPainter> painter_;
  VoiceState state_ = VoiceState::UNKNOWN;
  int width_ = 0, height_ = 0;
};

void JarvisReactor::setup() {
  add_tile("avatar", [this]() { return new AvatarTile(this); });
}

}  // namespace esphome::jarvis_reactor
