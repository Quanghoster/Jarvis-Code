# JARVIS Avatar for Tessera

An animated avatar tile for [Tessera](https://tessera.one) smart home screens that visualises the Home Assistant voice assistant pipeline state.

Inspired by the J.A.R.V.I.S. interface from the Iron Man films and the matching View Assist dashboard. The tile now ships in three display variants so you can pick the best look for each screen.

## What It Shows

A reactor-style avatar that reacts in real-time to your voice interactions:

| State | Colour | Animation |
|-------|--------|-----------|
| **Idle** | Cyan | Slow, gentle pulse |
| **Listening** | Green | Faster pulse / ring activity |
| **Processing** | Amber | Rapid pulse or ring motion |
| **Speaking** | White-hot cyan | Voice bars active |
| **Error** | Red | Blinking status dot |
| **Wake** | Bright green | Flare / intense pulse |
| **Sleep** | Purple | Very slow dim pulse |

## Display variants

Choose one per tile in the Tessera layout editor:

| Variant | Best for | How it works |
|---|---|---|
| **Reactor** (default) | Any screen | The original concentric-ring LVGL reactor. Busy, detailed, state-aware. |
| **HUD** | Square screens (e.g. Guition 480×480) or dark dashboards | A cleaner, View-Assist-style face: dark HUD background, subtle scanlines, glowing ring and pulsing core. Matches the look of `jarvis-home.html`. |
| **Image** | Larger or landscape screens (e.g. CrowPanel 1024×600) | Loads a picture (such as the `jarvis-ui.gif` used on the dashboard) and overlays a state-coloured border and status dot. Note: Tessera's picture API treats images as static, so animated GIFs will show their first frame only. |

## Installation

Requires **Tessera dev branch** (plugin API 0.10).

1. In Tessera, open **Plugins → Add with a link**
2. Paste your plugin repository URL
3. Add **JARVIS Avatar** to your screen; the screen builds once
4. In **Layout**, place the Avatar tile from the library's Plugins group

## Setup

### 1. Create a Voice State Helper in Home Assistant

Go to **Settings → Devices & Services → Helpers → Text** and create an `input_text` helper, e.g. `input_text.jarvis_voice_state`.

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
- **Display variant**: `reactor`, `hud`, or `image`
- **Tap to wake**: Enable to play a wake animation on touch

For the **image** variant:

- **Image URL**: the address Tessera should load.
  - For a file in Home Assistant's `www/` folder, use `/local/jarvis-ui.gif`.
  - For a public URL, the host must be listed under the plugin's `permissions.network` in `tessera-plugin.yaml`.
  - Note: private LAN IPs are not supported by `permissions.network`; use a public hostname or Home Assistant `/local/`.
- **Image fit**: `cover` fills the tile; `contain` shows the whole image (recommended for the 16:9 GIF on a square tile).

To use the existing dashboard GIF, copy or symlink it to Home Assistant `www/` if it is not already there:

```bash
cp /config/www/jarvis-ui.gif /config/www/jarvis-ui.gif
```

Then set the tile URL to `/local/jarvis-ui.gif`.

## File Structure

```
components/jarvis_reactor/
├── __init__.py              # ESPHome component registration
├── jarvis_reactor.h         # Plugin class interface
└── jarvis_reactor.cpp       # Avatar tile engine + three painters
tessera-plugin.yaml          # Manifest: tiles, options, pictures
plugin.yaml                  # Screen-level component
translations/en.json         # UI strings
```

## How It Works

- The tile polls Home Assistant every few seconds for the voice state helper's value
- The chosen painter transitions smoothly between states with LVGL animations
- All processing stays local on the screen; no cloud connections

## Credit

Architecture heavily inspired by [tessera-Microplant](https://github.com/alexscemo/tessera-Microplant) by alexscemo.

Reactor visual design inspired by the JARVIS dashboard for View Assist.

## License

MIT
