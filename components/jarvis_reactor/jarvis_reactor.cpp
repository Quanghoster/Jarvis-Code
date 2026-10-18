#include "jarvis_reactor.h"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace esphome::jarvis_reactor {

namespace ui = tessera::ui;

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
inline const lv_color_t DARK_BG = lv_color_hex(0x0A1428);
inline const lv_color_t GLASS_BORDER = lv_color_hex(0x00D4FF);

// ---- Voice state enum ----
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

// ---- The tile: JARVIS HUD reactor ----
class ReactorTile : public tessera::Tile {
 public:
  explicit ReactorTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    const int cx = width_ / 2;
    const int cy = height_ / 2;
    const int size = std::min(width_, height_);
    const float s = size / 240.0f;  // scale based on 240px

    // Dark background
    bg_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(bg_);
    lv_obj_remove_flag(bg_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(bg_, 0, 0);
    lv_obj_set_style_bg_color(bg_, DARK_BG, 0);
    lv_obj_set_style_bg_opa(bg_, LV_OPA_100, 0);
    lv_obj_set_size(bg_, width_, height_);
    lv_obj_set_pos(bg_, 0, 0);

    // Outer arc ring (large, segmented) - ARC 4
    arc4_ = lv_arc_create(c.parent);
    lv_obj_remove_flag(arc4_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_arc_set_range(arc4_, 0, 360);
    lv_arc_set_value(arc4_, 280);  // partial arc
    lv_arc_set_bg_angles(arc4_, 30, 310);
    lv_obj_set_style_arc_color(arc4_, RING_DIM, 0);
    lv_obj_set_style_arc_width(arc4_, std::max(1, static_cast<int>(2 * s)), 0);
    lv_obj_set_style_arc_opa(arc4_, LV_OPA_30, 0);
    lv_obj_set_size(arc4_, static_cast<lv_coord_t>(size * 0.95f), static_cast<lv_coord_t>(size * 0.95f));
    lv_obj_set_pos(arc4_, cx - static_cast<lv_coord_t>(size * 0.475f), cy - static_cast<lv_coord_t>(size * 0.475f));

    // Middle outer ring with tick marks - ARC 3
    arc3_ = lv_arc_create(c.parent);
    lv_obj_remove_flag(arc3_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_arc_set_range(arc3_, 0, 360);
    lv_arc_set_value(arc3_, 340);
    lv_arc_set_bg_angles(arc3_, 10, 350);
    lv_obj_set_style_arc_color(arc3_, RING_BORDER, 0);
    lv_obj_set_style_arc_width(arc3_, std::max(2, static_cast<int>(3 * s)), 0);
    lv_obj_set_style_arc_opa(arc3_, LV_OPA_50, 0);
    lv_obj_set_size(arc3_, static_cast<lv_coord_t>(size * 0.80f), static_cast<lv_coord_t>(size * 0.80f));
    lv_obj_set_pos(arc3_, cx - static_cast<lv_coord_t>(size * 0.40f), cy - static_cast<lv_coord_t>(size * 0.40f));

    // Tick marks around arc3 (simulated with small dots)
    for (int i = 0; i < 12; ++i) {
      ticks_[i] = lv_obj_create(c.parent);
      lv_obj_remove_style_all(ticks_[i]);
      lv_obj_remove_flag(ticks_[i], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
      lv_obj_set_style_radius(ticks_[i], LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(ticks_[i], RING_DIM, 0);
      lv_obj_set_style_bg_opa(ticks_[i], LV_OPA_40, 0);
      const int td = std::max(2, static_cast<int>(3 * s));
      lv_obj_set_size(ticks_[i], td, td);
      // Position around the ring
      float angle = (i * 30.0f) * 3.14159f / 180.0f;
      int tx = cx + static_cast<int>((size * 0.40f) * cos(angle)) - td / 2;
      int ty = cy + static_cast<int>((size * 0.40f) * sin(angle)) - td / 2;
      lv_obj_set_pos(ticks_[i], tx, ty);
    }

    // Inner ring - ARC 2
    arc2_ = lv_arc_create(c.parent);
    lv_obj_remove_flag(arc2_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_arc_set_range(arc2_, 0, 360);
    lv_arc_set_value(arc2_, 300);
    lv_arc_set_bg_angles(arc2_, 45, 315);
    lv_obj_set_style_arc_color(arc2_, CYAN, 0);
    lv_obj_set_style_arc_width(arc2_, std::max(2, static_cast<int>(2.5f * s)), 0);
    lv_obj_set_style_arc_opa(arc2_, LV_OPA_60, 0);
    lv_obj_set_size(arc2_, static_cast<lv_coord_t>(size * 0.65f), static_cast<lv_coord_t>(size * 0.65f));
    lv_obj_set_pos(arc2_, cx - static_cast<lv_coord_t>(size * 0.325f), cy - static_cast<lv_coord_t>(size * 0.325f));

    // Hexagonal dots on arc2
    for (int i = 0; i < 6; ++i) {
      hex_dots_[i] = lv_obj_create(c.parent);
      lv_obj_remove_style_all(hex_dots_[i]);
      lv_obj_remove_flag(hex_dots_[i], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
      lv_obj_set_style_radius(hex_dots_[i], LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(hex_dots_[i], CYAN, 0);
      lv_obj_set_style_bg_opa(hex_dots_[i], LV_OPA_80, 0);
      const int hd = std::max(3, static_cast<int>(4 * s));
      lv_obj_set_size(hex_dots_[i], hd, hd);
      float angle = (i * 60.0f + 45.0f) * 3.14159f / 180.0f;
      int hx = cx + static_cast<int>((size * 0.325f) * cos(angle)) - hd / 2;
      int hy = cy + static_cast<int>((size * 0.325f) * sin(angle)) - hd / 2;
      lv_obj_set_pos(hex_dots_[i], hx, hy);
    }

    // Innermost ring - ARC 1 (close to core)
    arc1_ = lv_arc_create(c.parent);
    lv_obj_remove_flag(arc1_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_arc_set_range(arc1_, 0, 360);
    lv_arc_set_value(arc1_, 270);
    lv_arc_set_bg_angles(arc1_, 0, 270);
    lv_obj_set_style_arc_color(arc1_, CYAN, 0);
    lv_obj_set_style_arc_width(arc1_, std::max(2, static_cast<int>(2 * s)), 0);
    lv_obj_set_style_arc_opa(arc1_, LV_OPA_70, 0);
    lv_obj_set_size(arc1_, static_cast<lv_coord_t>(size * 0.50f), static_cast<lv_coord_t>(size * 0.50f));
    lv_obj_set_pos(arc1_, cx - static_cast<lv_coord_t>(size * 0.25f), cy - static_cast<lv_coord_t>(size * 0.25f));

    // Central core
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

    // Core highlight
    highlight_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(highlight_);
    lv_obj_remove_flag(highlight_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(highlight_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(highlight_, CORE_INNER, 0);
    lv_obj_set_style_opa(highlight_, LV_OPA_60, 0);
    const int hl_d = static_cast<int>(size * 0.12f);
    lv_obj_set_size(highlight_, hl_d, hl_d);
    lv_obj_set_pos(highlight_, cx - hl_d / 2 - static_cast<int>(4 * s), cy - hl_d / 2 - static_cast<int>(10 * s));

    // J.A.R.V.I.S. text — centred in the core
    tessera::Font title_font = (size >= 200) ? tessera::Font::HEADLINE :
                               (size >= 100) ? tessera::Font::TITLE :
                               tessera::Font::BODY;
    title_ = ui::label(c.parent, title_font, theme::INK);
    lv_obj_set_width(title_, width_);
    lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title_, CYAN, 0);
    ui::set_text(title_, "J.A.R.V.I.S.");
    // Centre vertically and horizontally
    const int text_y = cy - ui::line_height(title_font) / 2;
    lv_obj_set_pos(title_, 0, text_y);

    // Status dot (small, above the reactor)
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

    // Voice visualiser bars (hidden by default)
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

    state_ = VoiceState::IDLE;
    apply_state(state_);
  }

  void on_state(JsonObjectConst data) override {
    if (data["state"].is<const char*>()) {
      const char *state_text = data["state"];
      VoiceState new_state = parse_state(state_text);
      if (new_state != state_) {
        state_ = new_state;
        apply_state(state_);
      }
    }
  }

  void on_tick(uint32_t epoch) override {
    if (!epoch) return;

    // Reactor breathing animation
    const float phase = static_cast<float>(epoch % 3) / 3.0f * 6.2831853f;
    const float breathe = 0.93f + 0.07f * sin(phase);

    // Pulse core size
    const int core_base = std::min(width_, height_);
    const float s = core_base / 240.0f;
    const int core_d = static_cast<int>(core_base * 0.35f * breathe);
    const int cx = width_ / 2;
    const int cy = height_ / 2;
    lv_obj_set_size(core_, core_d, core_d);
    lv_obj_set_pos(core_, cx - core_d / 2, cy - core_d / 2);

    // Pulse arc opacities
    lv_opa_t arc_opa = static_cast<lv_opa_t>(LV_OPA_50 + LV_OPA_20 * sin(phase));
    lv_obj_set_style_arc_opa(arc1_, arc_opa, 0);
    lv_obj_set_style_arc_opa(arc2_, static_cast<lv_opa_t>(LV_OPA_40 + LV_OPA_20 * sin(phase + 1.0f)), 0);
    lv_obj_set_style_arc_opa(arc3_, static_cast<lv_opa_t>(LV_OPA_30 + LV_OPA_20 * sin(phase + 2.0f)), 0);

    // Animate voice bars when speaking
    if (state_ == VoiceState::SPEAKING) {
      for (int i = 0; i < 5; ++i) {
        int h = 2 + static_cast<int>(12 * sin(static_cast<float>(epoch) * 0.5f + i * 1.2f));
        h = std::max(2, h);
        lv_obj_set_height(bars_[i], h);
        lv_obj_set_y(bars_[i], lv_obj_get_y(bars_[i]) + (lv_obj_get_height(bars_[i]) - h) / 2);
      }
    }

    // Status dot blink for error
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
    lv_color_t arc_col = RING_BORDER;
    lv_color_t core_col = CORE_MID;
    lv_color_t status_col = OFF;
    lv_opa_t core_opa = LV_OPA_80;

    switch (state) {
      case VoiceState::IDLE:
        arc_col = RING_BORDER;
        core_col = CORE_MID;
        status_col = CYAN_DIM;
        core_opa = LV_OPA_80;
        break;
      case VoiceState::LISTENING:
        arc_col = GREEN;
        core_col = GREEN;
        status_col = GREEN;
        core_opa = LV_OPA_100;
        break;
      case VoiceState::PROCESSING:
        arc_col = AMBER;
        core_col = AMBER;
        status_col = AMBER;
        core_opa = LV_OPA_90;
        break;
      case VoiceState::SPEAKING:
        arc_col = CYAN;
        core_col = CORE_INNER;
        status_col = CYAN;
        core_opa = LV_OPA_100;
        break;
      case VoiceState::ERROR:
        arc_col = RED;
        core_col = RED;
        status_col = RED;
        core_opa = LV_OPA_70;
        break;
      case VoiceState::WAKE:
        arc_col = GREEN;
        core_col = CORE_INNER;
        status_col = GREEN;
        core_opa = LV_OPA_100;
        break;
      case VoiceState::SLEEP:
        arc_col = OFF;
        core_col = OFF;
        status_col = PURPLE;
        core_opa = LV_OPA_30;
        break;
      case VoiceState::UNKNOWN:
      default:
        arc_col = RING_DIM;
        core_col = OFF;
        status_col = OFF;
        core_opa = LV_OPA_50;
        break;
    }

    // Apply colours to arcs
    lv_obj_set_style_arc_color(arc1_, arc_col, 0);
    lv_obj_set_style_arc_color(arc2_, arc_col, 0);
    lv_obj_set_style_arc_color(arc3_, arc_col, 0);
    lv_obj_set_style_arc_color(arc4_, arc_col, 0);

    // Core
    lv_obj_set_style_shadow_color(core_, arc_col, 0);
    lv_obj_set_style_bg_color(core_, core_col, 0);
    lv_obj_set_style_bg_color(highlight_, core_col, 0);
    lv_obj_set_style_bg_color(status_, status_col, 0);
    lv_obj_set_style_shadow_color(status_, status_col, 0);
    lv_obj_set_style_bg_opa(core_, core_opa, 0);

    // Hex dots
    for (auto *dot : hex_dots_) {
      lv_obj_set_style_bg_color(dot, arc_col, 0);
    }

    // Show/hide voice bars
    for (auto *bar : bars_) {
      lv_obj_set_style_opa(bar, (state == VoiceState::SPEAKING) ? LV_OPA_80 : LV_OPA_TRANSP, 0);
    }

    // Keep text bluish
    lv_obj_set_style_text_color(title_, CYAN, 0);
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0;
  VoiceState state_ = VoiceState::UNKNOWN;
  lv_obj_t *bg_{};
  lv_obj_t *arc1_{};
  lv_obj_t *arc2_{};
  lv_obj_t *arc3_{};
  lv_obj_t *arc4_{};
  lv_obj_t *ticks_[12]{};
  lv_obj_t *hex_dots_[6]{};
  lv_obj_t *core_{};
  lv_obj_t *highlight_{};
  lv_obj_t *title_{};
  lv_obj_t *status_{};
  lv_obj_t *bars_[5]{};
};

void JarvisReactor::setup() {
  add_tile("avatar", [this]() { return new ReactorTile(this); });
}

}  // namespace esphome::jarvis_reactor
