#pragma once
// JARVIS Avatar plugin for Tessera screens.
// A reactor-style avatar that shows the voice assistant pipeline state.

#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::jarvis_avatar {

class JarvisAvatar : public Component, public tessera::Plugin {
 public:
  void setup() override;
};

}  // namespace esphome::jarvis_avatar
