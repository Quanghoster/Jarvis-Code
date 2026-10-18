#include "jarvis_reactor.h"
#include <cmath>
#include <cstdint>
#include <string>

namespace esphome::jarvis_reactor {

namespace ui = tessera::ui;

// ---- Colours matching the View Assist dashboard reactor ----
inline const lv_color_t CYAN = lv_color_hex(0x00D4FF);
inline const lv_color_t CYAN_DIM = lv_color_hex(0x006680);
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

// ---- The tile: reactor avatar ----
class ReactorTile : public tessera::Tile {
 public:
  explicit ReactorTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    const int cx = width_ / 2;
    const int cy = height_ / 2;
    // The reactor sits in a square, centred, using the smaller dimension
    const int reactor_size = std::min(width_, height_);
    const float s = reactor_size / 120.0f;  // scale factor based on 120px base

    // Dark glass background (like dashboard panels)
    bg_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(bg_);
    lv_obj_remove_flag(bg_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(bg_, 12, 0);
    lv_obj_set_style_bg_color(bg_, DARK_BG, 0);
    lv_obj_set_style_bg_opa(bg_, LV_OPA_40, 0);
    lv_obj_set_style_border_color(bg_, GLASS_BORDER, 0);
    lv_obj_set_style_border_width(bg_, 1, 0);
    lv_obj_set_style_border_opa(bg_, LV_OPA_30, 0);
    lv_obj_set_size(bg_, width_, height_);
    lv_obj_set_pos(bg_, 0, 0);

    // Top accent line (like dashboard panels)
    accent_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(accent_);
    lv_obj_remove_flag(accent_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_bg_color(accent_, CYAN, 0);
    lv_obj_set_style_bg_opa(accent_, LV_OPA_60, 0);
    lv_obj_set_size(accent_, width_, 1);
    lv_obj_set_pos(accent_, 0, 0);

    // Ambient glow overlay (large soft glow behind reactor)
    glow_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(glow_);
    lv_obj_remove_flag(glow_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(glow_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(glow_, CYAN, 0);
    lv_obj_set_style_bg_opa(glow_, LV_OPA_10, 0);
    const int glow_d = static_cast<int>(reactor_size * 1.2f);
    lv_obj_set_size(glow_, glow_d, glow_d);
    lv_obj_set_pos(glow_, cx - glow_d / 2, cy - glow_d / 2 - static_cast<int>(10 * s));
    lv_obj_set_style_shadow_color(glow_, CYAN, 0);
    lv_obj_set_style_shadow_width(glow_, static_cast<lv_coord_t>(40 * s), 0);
    lv_obj_set_style_shadow_opa(glow_, LV_OPA_30, 0);

    // Outer ring (main glow ring like dashboard)
    outer_ring_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(outer_ring_);
    lv_obj_remove_flag(outer_ring_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(outer_ring_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(outer_ring_, RING_BORDER, 0);
    lv_obj_set_style_border_width(outer_ring_, std::max(2, static_cast<int>(3 * s)), 0);
    lv_obj_set_style_border_opa(outer_ring_, LV_OPA_60, 0);
    lv_obj_set_style_shadow_color(outer_ring_, CYAN, 0);
    lv_obj_set_style_shadow_width(outer_ring_, static_cast<lv_coord_t>(20 * s), 0);
    lv_obj_set_style_shadow_opa(outer_ring_, LV_OPA_50, 0);
    const int outer_d = static_cast<int>(reactor_size * 0.85f);
    lv_obj_set_size(outer_ring_, outer_d, outer_d);
    lv_obj_set_pos(outer_ring_, cx - outer_d / 2, cy - outer_d / 2 - static_cast<int>(10 * s));

    // Middle ring (decorative, thinner)
    mid_ring_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(mid_ring_);
    lv_obj_remove_flag(mid_ring_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(mid_ring_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(mid_ring_, RING_DIM, 0);
    lv_obj_set_style_border_width(mid_ring_, std::max(1, static_cast<int>(2 * s)), 0);
    lv_obj_set_style_border_opa(mid_ring_, LV_OPA_40, 0);
    const int mid_d = static_cast<int>(reactor_size * 0.70f);
    lv_obj_set_size(mid_ring_, mid_d, mid_d);
    lv_obj_set_pos(mid_ring_, cx - mid_d / 2, cy - mid_d / 2 - static_cast<int>(10 * s));

    // Inner ring
    inner_ring_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(inner_ring_);
    lv_obj_remove_flag(inner_ring_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(inner_ring_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(inner_ring_, RING_BORDER, 0);
    lv_obj_set_style_border_width(inner_ring_, std::max(1, static_cast<int>(1.5f * s)), 0);
    lv_obj_set_style_border_opa(inner_ring_, LV_OPA_30, 0);
    const int inner_d = static_cast<int>(reactor_size * 0.55f);
    lv_obj_set_size(inner_ring_, inner_d, inner_d);
    lv_obj_set_pos(inner_ring_, cx - inner_d / 2, cy - inner_d / 2 - static_cast<int>(10 * s));

    // Central core (the reactor core)
    core_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(core_);
    lv_obj_remove_flag(core_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(core_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(core_, CORE_MID, 0);
    lv_obj_set_style_shadow_color(core_, CYAN, 0);
    lv_obj_set_style_shadow_width(core_, static_cast<lv_coord_t>(25 * s), 0);
    lv_obj_set_style_shadow_opa(core_, LV_OPA_60, 0);
    const int core_d = static_cast<int>(reactor_size * 0.40f);
    lv_obj_set_size(core_, core_d, core_d);
    lv_obj_set_pos(core_, cx - core_d / 2, cy - core_d / 2 - static_cast<int>(10 * s));

    // Core highlight (bright spot)
    highlight_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(highlight_);
    lv_obj_remove_flag(highlight_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(highlight_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(highlight_, CORE_INNER, 0);
    lv_obj_set_style_opa(highlight_, LV_OPA_70, 0);
    const int hl_d = static_cast<int>(reactor_size * 0.15f);
    lv_obj_set_size(highlight_, hl_d, hl_d);
    lv_obj_set_pos(highlight_, cx - hl_d / 2 - static_cast<int>(5 * s), cy - hl_d / 2 - static_cast<int>(15 * s));

    // J.A.R.V.I.S. text label (like dashboard title)
    // Use the largest available font that fits
    tessera::Font title_font = (reactor_size >= 200) ? tessera::Font::HEADLINE :
                               (reactor_size >= 100) ? tessera::Font::TITLE :
                               tessera::Font::BODY;
    title_ = ui::label(c.parent, title_font, theme::INK);
    lv_obj_set_width(title_, width_);
    lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);
    ui::set_text(title_, "J.A.R.V.I.S.");
    // Position below the reactor
    const int title_y = cy + static_cast<int>(reactor_size * 0.42f);
    lv_obj_set_pos(title_, 0, title_y);

    // Subtitle / status text
    subtitle_ = ui::label(c.parent, tessera::Font::BODY, theme::MUTED);
    lv_obj_set_width(subtitle_, width_);
    lv_obj_set_style_text_align(subtitle_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(subtitle_, 0, title_y + ui::line_height(title_font) + static_cast<int>(4 * s));

    // Status dot (small indicator)
    status_ = lv_obj_create(c.parent);
    lv_obj_remove_style_all(status_);
    lv_obj_remove_flag(status_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_radius(status_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_, OFF, 0);
    lv_obj_set_style_shadow_color(status_, OFF, 0);
    lv_obj_set_style_shadow_width(status_, static_cast<lv_coord_t>(8 * s), 0);
    const int sd_d = std::max(4, static_cast<int>(8 * s));
    lv_obj_set_size(status_, sd_d, sd_d);
    lv_obj_set_pos(status_, cx - sd_d / 2, title_y - static_cast<int>(12 * s));

    // Voice visualiser bars (hidden by default, shown when speaking)
    const int bar_w = std::max(2, static_cast<int>(4 * s));
    const int bar_gap = std::max(3, static_cast<int>(6 * s));
    for (int i = 0; i < 5; ++i) {
      bars_[i] = lv_obj_create(c.parent);
      lv_obj_remove_style_all(bars_[i]);
      lv_obj_remove_flag(bars_[i], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
      lv_obj_set_style_bg_color(bars_[i], CYAN, 0);
      lv_obj_set_style_radius(bars_[i], bar_w / 2, 0);
      lv_obj_set_size(bars_[i], bar_w, 2);
      lv_obj_set_pos(bars_[i],
                      cx - (2 * bar_gap + 2 * bar_w) + i * (bar_w + bar_gap),
                      title_y - static_cast<int>(8 * s));
      lv_obj_set_style_opa(bars_[i], LV_OPA_TRANSP, 0);
    }

    state_ = VoiceState::IDLE;
    apply_state(state_);
    update_subtitle();
  }

  void on_state(JsonObjectConst data) override {
    if (data["state"].is<const char*>()) {
      const char *state_text = data["state"];
      VoiceState new_state = parse_state(state_text);
      if (new_state != state_) {
        state_ = new_state;
        apply_state(state_);
        update_subtitle();
      }
    }
  }

  void on_tick(uint32_t epoch) override {
    if (!epoch) return;

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
    ui::set_color(title_, theme::INK);
    ui::set_color(subtitle_, theme::MUTED);
  }

 private:
  void update_subtitle() {
    const char *text = "";
    switch (state_) {
      case VoiceState::IDLE: text = "System Online"; break;
      case VoiceState::LISTENING: text = "Listening..."; break;
      case VoiceState::PROCESSING: text = "Processing..."; break;
      case VoiceState::SPEAKING: text = "Speaking..."; break;
      case VoiceState::ERROR: text = "Error"; break;
      case VoiceState::WAKE: text = "Wake"; break;
      case VoiceState::SLEEP: text = "Sleep"; break;
      case VoiceState::UNKNOWN: text = "Unknown"; break;
    }
    ui::set_text(subtitle_, text);
  }

  void apply_state(VoiceState state) {
    lv_color_t ring_col = RING_BORDER;
    lv_color_t core_col = CORE_MID;
    lv_color_t status_col = OFF;
    lv_opa_t core_opa = LV_OPA_80;
    lv_opa_t glow_opa = LV_OPA_10;

    switch (state) {
      case VoiceState::IDLE:
        ring_col = RING_BORDER;
        core_col = CORE_MID;
        status_col = CYAN_DIM;
        core_opa = LV_OPA_80;
        glow_opa = LV_OPA_10;
        break;
      case VoiceState::LISTENING:
        ring_col = GREEN;
        core_col = GREEN;
        status_col = GREEN;
        core_opa = LV_OPA_100;
        glow_opa = LV_OPA_30;
        break;
      case VoiceState::PROCESSING:
        ring_col = AMBER;
        core_col = AMBER;
        status_col = AMBER;
        core_opa = LV_OPA_90;
        glow_opa = LV_OPA_20;
        break;
      case VoiceState::SPEAKING:
        ring_col = CYAN;
        core_col = CORE_INNER;
        status_col = CYAN;
        core_opa = LV_OPA_100;
        glow_opa = LV_OPA_40;
        break;
      case VoiceState::ERROR:
        ring_col = RED;
        core_col = RED;
        status_col = RED;
        core_opa = LV_OPA_70;
        glow_opa = LV_OPA_20;
        break;
      case VoiceState::WAKE:
        ring_col = GREEN;
        core_col = CORE_INNER;
        status_col = GREEN;
        core_opa = LV_OPA_100;
        glow_opa = LV_OPA_50;
        break;
      case VoiceState::SLEEP:
        ring_col = OFF;
        core_col = OFF;
        status_col = PURPLE;
        core_opa = LV_OPA_30;
        glow_opa = LV_OPA_10;
        break;
      case VoiceState::UNKNOWN:
      default:
        ring_col = RING_DIM;
        core_col = OFF;
        status_col = OFF;
        core_opa = LV_OPA_50;
        glow_opa = LV_OPA_10;
        break;
    }

    // Apply colours
    lv_obj_set_style_border_color(outer_ring_, ring_col, 0);
    lv_obj_set_style_shadow_color(outer_ring_, ring_col, 0);
    lv_obj_set_style_shadow_color(core_, core_col, 0);
    lv_obj_set_style_bg_color(core_, core_col, 0);
    lv_obj_set_style_bg_color(highlight_, core_col, 0);
    lv_obj_set_style_bg_color(status_, status_col, 0);
    lv_obj_set_style_shadow_color(status_, status_col, 0);
    lv_obj_set_style_bg_opa(core_, core_opa, 0);
    lv_obj_set_style_bg_opa(glow_, glow_opa, 0);

    // Show/hide voice bars
    for (auto *bar : bars_) {
      lv_obj_set_style_opa(bar, (state == VoiceState::SPEAKING) ? LV_OPA_80 : LV_OPA_TRANSP, 0);
    }
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0;
  VoiceState state_ = VoiceState::UNKNOWN;
  lv_obj_t *bg_{};
  lv_obj_t *accent_{};
  lv_obj_t *glow_{};
  lv_obj_t *outer_ring_{};
  lv_obj_t *mid_ring_{};
  lv_obj_t *inner_ring_{};
  lv_obj_t *core_{};
  lv_obj_t *highlight_{};
  lv_obj_t *title_{};
  lv_obj_t *subtitle_{};
  lv_obj_t *status_{};
  lv_obj_t *bars_[5]{};
};

void JarvisReactor::setup() {
  // Register the tile with the manifest id
  add_tile("avatar", [this]() { return new ReactorTile(this); });
}

}  // namespace esphome::jarvis_reactor
