#pragma once
// Main JARVIS Avatar plugin component.
// Bridges the avatar face (jarvis_avatar.h) with Home Assistant entity polling
// and the Tessera plugin API.
//
// Adapted from microplant.h / microplant.cpp patterns.

#include "esphome/components/smart_display/plugin_api.h"
#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text/text.h"
#include "jarvis_avatar.h"

namespace esphome::jarvis_avatar {

class JarvisAvatarPlugin : public Component, public tessera::Plugin {
 public:
  void setup() override;
  void loop() override;

  // Tessera plugin API
  void on_tile_create(tessera::TileHandle tile) override;
  void on_tile_destroy(tessera::TileHandle tile) override;
  void on_tile_tick(tessera::TileHandle tile, uint32_t epoch, bool quiet) override;
  bool on_tile_tap(tessera::TileHandle tile, int x, int y) override;
  void on_theme() override;

  void set_demo_switch(switch_::Switch *demo) { demo_switch_ = demo; }
  void set_poll_number(number::Number *poll) { poll_number_ = poll; }
  void set_state_text(text::Text *state) { state_text_ = state; }

 private:
  void ask_home_assistant(tessera::TileHandle tile);
  void apply_state(tessera::TileHandle tile, VoiceState state);
  VoiceState last_state_ = VoiceState::UNKNOWN;
  uint32_t last_poll_ = 0;
  uint32_t last_answer_ = 0;

  switch_::Switch *demo_switch_ = nullptr;
  number::Number *poll_number_ = nullptr;
  text::Text *state_text_ = nullptr;
};

}  // namespace esphome::jarvis_avatar
