#include "jarvis_plugin.h"
#include "jarvis_model.h"

namespace esphome::jarvis_avatar {

namespace ui = tessera::ui;
using namespace tessera;

// ---- Plugin setup ----

void JarvisAvatarPlugin::setup() {
  // Register with Tessera's smart display system
  // The plugin manifest is read from the directory beside __init__.py
}

void JarvisAvatarPlugin::loop() {
  // Most work is event-driven; periodic polling happens in on_tile_tick
}

// ---- Tile lifecycle ----

void JarvisAvatarPlugin::on_tile_create(TileHandle tile) {
  auto *avatar = tile->alloc_user_data<JarvisAvatar>();
  if (!avatar) return;

  const auto bounds = tile->bounds();
  const int size = std::min(bounds.w, bounds.h);
  const int x = bounds.x + (bounds.w - size) / 2;
  const int y = bounds.y + (bounds.h - size) / 2;

  avatar->create(tile->parent(), x, y, size);

  // If demo mode is on, start with WAKE animation
  if (demo_switch_ && demo_switch_->state) {
    avatar->wake();
    apply_state(tile, VoiceState::WAKE);
  } else {
    apply_state(tile, VoiceState::UNKNOWN);
  }

  last_poll_ = 0;
  last_answer_ = 0;
}

void JarvisAvatarPlugin::on_tile_destroy(TileHandle tile) {
  // LVGL objects are cleaned up by Tessera; our avatar destructor is trivial
  tile->free_user_data();
}

// ---- Periodic updates ----

void JarvisAvatarPlugin::on_tile_tick(TileHandle tile, uint32_t epoch, bool quiet) {
  auto *avatar = tile->user_data<JarvisAvatar>();
  if (!avatar || !avatar->made()) return;

  // Run idle animations (blink, pulse, etc.)
  avatar->on_tick(epoch, quiet);

  // Poll Home Assistant for voice state
  const bool demo = demo_switch_ && demo_switch_->state;
  if (demo) {
    // Demo mode: cycle through states automatically
    const VoiceState demo_s = demo_state(epoch);
    if (demo_s != last_state_) {
      apply_state(tile, demo_s);
    }
    return;
  }

  // Real mode: poll the configured entity
  const uint32_t poll_sec = poll_number_ ? static_cast<uint32_t>(poll_number_->state) : config::POLL_SECONDS;
  const uint32_t interval = std::max(config::POLL_MIN_SECONDS, std::min(poll_sec, config::POLL_MAX_SECONDS));

  if (epoch - last_poll_ >= interval) {
    ask_home_assistant(tile);
    last_poll_ = epoch;
  }

  // Offline detection: if no answer for too long
  if (epoch - last_answer_ > config::OFFLINE_AFTER_SECONDS && last_state_ != VoiceState::UNKNOWN) {
    apply_state(tile, VoiceState::UNKNOWN);
  }
}

// ---- Home Assistant interaction ----

void JarvisAvatarPlugin::ask_home_assistant(TileHandle tile) {
  // Get the entity configured for this tile
  const char *entity = tile->option_string("entity");
  if (!entity || !*entity) {
    // No entity configured: show unknown
    apply_state(tile, VoiceState::UNKNOWN);
    return;
  }

  // Request state from Home Assistant via Tessera's API
  // This uses the smart_display plugin's built-in HA communication
  tile->ask_home_assistant(entity, [this, tile](const char *state_text) {
    last_answer_ = tile->epoch();
    VoiceState state = parse_voice_state(state_text);
    apply_state(tile, state);
  });
}

void JarvisAvatarPlugin::apply_state(TileHandle tile, VoiceState state) {
  auto *avatar = tile->user_data<JarvisAvatar>();
  if (!avatar) return;

  avatar->set_state(state);
  last_state_ = state;

  // Update the screen's "state text" entity if configured
  // This lets other systems read the current visual state
  if (state_text_) {
    state_text_->set_value(voice_state_name(state));
  }
}

// ---- Touch interaction ----

bool JarvisAvatarPlugin::on_tile_tap(TileHandle tile, int x, int y) {
  auto *avatar = tile->user_data<JarvisAvatar>();
  if (!avatar || !avatar->made()) return false;

  // Check if tap is on the avatar face
  if (!avatar->hit(x, y)) return false;

  // Check if tappable option is enabled
  const bool tappable = tile->option_bool("tappable", true);
  if (!tappable) return false;

  // Trigger wake animation
  avatar->wake();

  // Fire a Home Assistant event so automations can react
  tile->fire_home_assistant_event("jarvis_avatar_tapped", {
    {"tile_id", std::to_string(tile->id()).c_str()},
    {"state", voice_state_name(last_state_)}
  });

  return true;  // consumed the tap
}

// ---- Theme changes ----

void JarvisAvatarPlugin::on_theme() {
  // Tessera calls this when light/dark mode switches
  // We iterate all tiles and update their avatar colours
  for_each_tile([](TileHandle tile) {
    auto *avatar = tile->user_data<JarvisAvatar>();
    if (avatar) avatar->on_theme();
  });
}

}  // namespace esphome::jarvis_avatar
