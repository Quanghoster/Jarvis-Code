# JARVIS Avatar for Tessera

An animated reactor avatar tile for [Tessera](https://tessera.one) smart home screens that visualises the Home Assistant voice assistant pipeline state.

Inspired by the J.A.R.V.I.S. arc reactor from the Iron Man films — a glowing circular core with concentric rings that pulse and shift colour based on voice interaction state.

## What It Shows

A reactor-style avatar that reacts in real-time to your voice interactions:

| State | Ring | Core | Pulse Speed | Animation |
|-------|------|------|-------------|-----------|
| **Idle** | Cyan | Medium | 2s slow | Gentle glow |
| **Listening** | Green | Bright | 0.8s fast | Ring counter-rotation |
| **Processing** | Amber | Warm | 0.6s rapid | Spinning ring |
| **Speaking** | Cyan | White-hot | 0.4s quick | Voice bars active |
| **Error** | Red | Dim | 1.2s erratic | Blinking status dot |
| **Wake** | Green | Flash | 0.3s burst | Bright flash |
| **Sleep** | Dark | Dim | 4s very slow | Purple status dot |

The avatar features:
- **Concentric rings**: outer glow ring, middle rotating ring, inner accent ring
- **Pulsing core**: central reactor sphere with radial gradient simulation
- **Ambient glow**: soft background halo matching the state colour
- **Status dot**: small indicator at the bottom showing current state
- **Voice visualiser**: 5 bars appearing when speaking

## Colours

Matching the View Assist dashboard reactor design:
- Primary cyan: `#00D4FF`
- Core inner: `#AADDFF`
- Core outer: `#002244`
- Dim ring: `#0088AA`

## Installation

Requires **Tessera dev branch** (plugin API 0.10) as of now.

1. In Tessera, open **Plugins → Add with a link**
2. Paste: `https:…Code`
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
- **Avatar style**: Reactor (Iron Man), Minimal circle, or Hexagonal
- **Tap to wake**: Enable to play a wake animation on touch

## How It Works

- The tile polls Home Assistant every 2 seconds (configurable) for the voice state helper's value
- The reactor transitions smoothly between states with LVGL animations
- Ring rotation, core pulsing, and glow effects are all hardware-accelerated
- All processing stays local on the screen; no cloud connections

## File Structure

```
components/jarvis_avatar/
├── __init__.py              # ESPHome component registration
├── jarvis_model.h           # Voice state enum, parsing, demo cycling
├── jarvis_avatar.h          # Avatar class interface
├── jarvis_avatar.cpp        # LVGL reactor drawing & animation engine
├── jarvis_plugin.h          # Plugin class header
└── jarvis_plugin.cpp        # HA polling, tile lifecycle, touch handling
plugin.yaml                  # Screen-level settings
tessera-plugin.yaml          # Tile options & sizes
translations/en.json         # UI strings
```

## Credit

Architecture heavily inspired by [tessera-Microplant](https://github.com/alexscemo/tessera-Microplant) by alexscemo — a beautifully crafted plant monitoring tile that demonstrates the Tessera plugin API patterns.

Reactor visual design inspired by the JARVIS dashboard for View Assist.

## License

MIT
