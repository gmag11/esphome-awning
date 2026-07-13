# ESPHome Awning Cover Component

A position-aware awning cover platform for [ESPHome](https://esphome.io/) with non-linear angle tracking and calibration sequences. Designed for motorized articulated-arm awnings where motor travel and opening angle are not linearly related.

## Features

- **Non-linear position-to-angle mapping** — uses a lookup table to convert linear motor movement into the real opening angle, so the reported position (0–100%) matches what you actually see.
- **Time-based position tracking** — no position sensor needed. The component estimates position from motor run time.
- **Calibration sequences** — define multi-step relay pulses to enter the motor's calibration mode. Supports reset, upper-limit, and lower-limit sequences.
- **`awning.calibrate` action** — trigger calibration from automations, buttons, or Home Assistant.
- **Built-in endstop support** — configure whether the motor has its own endstops (`has_built_in_endstop`).

## Installation

Add the external component to your ESPHome YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/gmag11/esphome-awning.git
      ref: main
    components: [ awning ]
```

## Quick Start

```yaml
output:
  - platform: gpio
    pin: GPIO14
    id: up_motor
  - platform: gpio
    pin: GPIO12
    id: down_motor

cover:
  - platform: awning
    name: "Patio Awning"
    open_duration: 30s
    close_duration: 30s
    open_action:
      - output.turn_on: up_motor
      - output.turn_off: down_motor
    close_action:
      - output.turn_on: down_motor
      - output.turn_off: up_motor
    stop_action:
      - output.turn_off: up_motor
      - output.turn_off: down_motor
```

## Configuration Variables

| Variable | Required | Type | Default | Description |
|---|---|---|---|---|
| `open_duration` | **yes** | time | — | Time to fully open from closed position. |
| `close_duration` | **yes** | time | — | Time to fully close from open position. |
| `open_action` | **yes** | action | — | Action executed when opening starts (e.g. energise up relay). |
| `close_action` | **yes** | action | — | Action executed when closing starts (e.g. energise down relay). |
| `stop_action` | **yes** | action | — | Action executed when stopping (e.g. de-energise both relays). |
| `has_built_in_endstop` | no | boolean | `false` | If `true`, `stop_action` is not called when reaching open/close limits. |
| `assumed_state` | no | boolean | `true` | If `true`, Home Assistant shows both OPEN and CLOSE buttons regardless of current state. |
| `calibration` | no | — | — | Calibration sub-configuration (see below). |

### Calibration

| Variable | Required | Type | Default | Description |
|---|---|---|---|---|
| `enabled` | no | boolean | `true` | Enable calibration features. |
| `reset_sequence` | no | list | `[]` | Pulse sequence for resetting calibration. Each item: `up`, `down`, or `stop`. |
| `up_sequence` | no | list | `[]` | Pulse sequence for upper-limit calibration. |
| `down_sequence` | no | list | `[]` | Pulse sequence for lower-limit calibration. |
| `pulse_pause` | no | time | `250ms` | Pause between each pulse in a sequence. |
| `final_pause` | no | time | `2500ms` | Final pause after the full sequence completes. |

## Calibration

### How sequences work

Each calibration sequence is a list of relay actions. The component alternates between executing an action and pausing (`pulse_pause`), then applies a `final_pause` at the end.

For example, `reset_sequence: [up, up, down, up, up, down]` produces:
```text
up → pause 250ms → stop → pause 250ms → up → pause 250ms → stop → ...
```

### Triggering calibration

Use the `awning.calibrate` action with one of three modes:

```yaml
button:
  - platform: template
    name: "Reset Calibration"
    on_press:
      - awning.calibrate:
          id: my_awning
          action: reset   # 'reset', 'up', or 'down'
```

## Full Example

A complete configuration with physical up/down buttons, calibration, and Home Assistant integration:

```yaml
esphome:
  name: cover

external_components:
  - source:
      type: git
      url: https://github.com/gmag11/esphome-awning.git
      ref: main
    components: [ awning ]

esp8266:
  board: esp8285
  framework:
    version: recommended

# API for Home Assistant
api:
  encryption:
    key: !secret api_encryption_key

ota:
  - platform: esphome
    password: !secret ota_password

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

# Motor relay outputs
output:
  - platform: gpio
    pin: GPIO14
    id: up_motor
  - platform: gpio
    pin: GPIO12
    id: down_motor

# Physical buttons (momentary switches)
binary_sensor:
  - platform: gpio
    pin:
      number: GPIO5
      mode:
        input: true
        pullup: true
      inverted: true
    name: "Push Up"
    id: push_up
    on_press:
      then:
        - cover.open: cover
    on_release:
      then:
        - cover.stop: cover

  - platform: gpio
    pin:
      number: GPIO4
      mode:
        input: true
        pullup: true
      inverted: true
    name: "Push Down"
    id: push_down
    on_press:
      then:
        - cover.close: cover
    on_release:
      then:
        - cover.stop: cover

# Status LED
light:
  - platform: status_led
    name: "Status LED"
    pin:
      inverted: true
      number: GPIO3

# Awning cover
cover:
  - platform: awning
    name: "Salon Awning"
    id: cover
    device_class: awning
    open_duration: 30s
    close_duration: 30s
    assumed_state: true
    has_built_in_endstop: false
    open_action:
      - output.turn_on: up_motor
      - output.turn_off: down_motor
    close_action:
      - output.turn_on: down_motor
      - output.turn_off: up_motor
    stop_action:
      - output.turn_off: up_motor
      - output.turn_off: down_motor
    calibration:
      enabled: true
      reset_sequence:
        - up
        - up
        - down
        - up
        - up
        - down
      up_sequence:
        - up
        - up
        - up
      down_sequence:
        - down
        - down
        - down
      pulse_pause: 250ms
      final_pause: 2500ms

# Calibration buttons (visible in Home Assistant)
button:
  - platform: template
    name: "Reset Calibration"
    on_press:
      - awning.calibrate:
          id: cover
          action: reset

  - platform: template
    name: "Top Limit"
    on_press:
      - awning.calibrate:
          id: cover
          action: up

  - platform: template
    name: "Bottom Limit"
    on_press:
      - awning.calibrate:
          id: cover
          action: down
```

## How Position Tracking Works

The component uses a 101-entry lookup table that maps the **linear motor position** (0–100%) to the **real opening angle** (0–100%). This accounts for the non-linear geometry of articulated-arm awnings, where the first few seconds of motor movement produce a large angular change but the last few seconds produce very little.

- `open_duration` and `close_duration` define the total motor run time.
- During movement the component interpolates position based on elapsed time.
- Position is published every second while moving, and every 30 seconds when idle.

## License

MIT
