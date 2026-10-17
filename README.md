# JARVIS Avatar for Tessera

An animated avatar tile for [Tessera](https://tessera.one) smart home screens that visualises the Home Assistant voice assistant pipeline state.

## What It Shows

A circular face that reacts in real-time to your voice interactions:

| State | Visual |
|-------|--------|
| **Idle** | Gentle pulse, subtle smile, relaxed eyes |
| **Listening** | Eyes widen, eyebrows raise, fast green pulse |
| **Processing** | Eyes narrow slightly, three amber dots bounce |
| **Speaking** | Mouth opens and closes, blue glow |
| **Error** | Furrowed brows, frown, question mark, red tint |
| **Wake** | Brief flourish, grin, bright green |
| **Sleep** | Dimmed, closed eyes, "z" glyph |

## Installation

Requires **Tessera dev branch** (plugin API 0.10) as of now.

1. In Tessera, open **Plugins → Add with a link**
2. Paste: `https://github.com/YOUR_USERNAME/tessera-jarvis-avatar`
3. Add **JARVIS Avatar** to your screen; the screen builds once
4. In **Layout**, place the Avatar tile from the library's Plugins group

## Setup

### 1. Create a Voice State Helper in Home Assistant

Go to **Settings → Devices & Services → Helpers → Text** and create an `input_text` helper.

Name it something like `input_text.jarvis_voice_state`.

### 2. Update the Helper via Automations

Create an automation that sets the helper based on voice pipeline events:

```yaml
alias: "JARVIS Voice State Tracker"
trigger:
  - platform: event
    event_type: assist_pipeline_run
    event_data:
      pipeline_stage: start
    id: listening
  - platform: event
    event_type: assist_pipeline_run
    event_data:
      pipeline_stage: stt
    id: processing
  - platform: event
    event_type: assist_pipeline_run
    event_data:
      pipeline_stage: tts
    id: speaking
  - platform: event
    event_type: assist_pipeline_run
    event_data:
      pipeline_stage: end
    id: idle
  - platform: event
    event_type: assist_pipeline_run
    event_data:
      error: true
    id: error
action:
  - service: input_text.set_value
    target:
      entity_id: input_text.jarvis_voice_state
    data:
      value: >
        {% if trigger.id == 'listening' %}listening
        {% elif trigger.id == 'processing' %}processing
        {% elif trigger.id == 'speaking' %}speaking
        {% elif trigger.id == 'error' %}error
        {% else %}idle{% endif %}
```

### 3. Configure the Tile

In the Tessera layout editor, select the JARVIS Avatar tile and set:

- **Voice state entity**: `input_text.jarvis_voice_state` (or your helper)
- **Avatar style**: Minimal circle, Hexagonal, or Glowing orb
- **Tap to wake**: Enable to play a wake animation on touch

## How It Works

- The tile polls Home Assistant every 2 seconds (configurable) for the voice state helper's value
- The face transitions smoothly between expressions with LVGL animations
- All processing stays local on the screen; no cloud connections

## File Structure

```
components/jarvis_avatar/
├── __init__.py              # ESPHome component registration
├── jarvis_model.h           # Voice state enum, parsing, demo cycling
├── jarvis_avatar.h          # Avatar class interface
├── jarvis_avatar.cpp        # LVGL drawing & animation engine
plugin.yaml                  # Screen-level settings
tessera-plugin.yaml          # Tile options & sizes
translations/en.json         # UI strings
```

## Credit

Architecture heavily inspired by [tessera-Microplant](https://github.com/alexscemo/tessera-Microplant) by alexscemo — a beautifully crafted plant monitoring tile that demonstrates the Tessera plugin API patterns.

## License

MIT
