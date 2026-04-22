# ESPHome UWB DW3000

ESPHome external component for a DW3000-based UWB tag using ESP-IDF on ESP32.

This repo starts with the `tag` side of the original reference firmware and adapts it into an ESPHome external component structure.

## Status

Current state:
- initial external component structure added
- `tag` ranging logic ported into a dedicated ESPHome component
- DW3000 third-party driver vendored into the component
- basic ESPHome entities exposed for distance, CIR power, and computed position

This is a first pass and will likely need board-specific pin/config tuning plus real hardware validation.

## Example usage

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/EwoutsAgents/esphome-uwb-dw3000
      ref: main
    components: [uwb_dw3000]
    refresh: 0s

esphome:
  name: uwb-tag
  platformio_options:
    build_flags:
      - -std=gnu++17

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:

spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

uwb_dw3000:
  id: my_uwb
  role: tag
  ss_pin: GPIO4
  irq_pin: GPIO34
  rst_pin: GPIO27
  tag_id: 0x45
  update_interval: 200ms
  anchors:
    - id: 0x01
      x: 0.06
      y: 4.61
      z: 1.83
    - id: 0x02
      x: 2.09
      y: 4.37
      z: 1.74
    - id: 0x03
      x: 2.20
      y: 0.06
      z: 1.83
  tag_height: 0.78

sensor:
  - platform: uwb_dw3000
    uwb_dw3000_id: my_uwb
    distance:
      name: "UWB Distance A1"
      anchor_id: 0x01

  - platform: uwb_dw3000
    uwb_dw3000_id: my_uwb
    distance_filtered:
      name: "UWB Distance A1 Filtered"
      anchor_id: 0x01

  - platform: uwb_dw3000
    uwb_dw3000_id: my_uwb
    x_raw:
      name: "UWB X Raw"

  - platform: uwb_dw3000
    uwb_dw3000_id: my_uwb
    y_raw:
      name: "UWB Y Raw"

  - platform: uwb_dw3000
    uwb_dw3000_id: my_uwb
    x_filtered:
      name: "UWB X Filtered"

  - platform: uwb_dw3000
    uwb_dw3000_id: my_uwb
    y_filtered:
      name: "UWB Y Filtered"

  - platform: uwb_dw3000
    uwb_dw3000_id: my_uwb
    fp_power:
      name: "UWB First Path Power"

  - platform: uwb_dw3000
    uwb_dw3000_id: my_uwb
    rx_power:
      name: "UWB RX Power"
```

## Notes

- Designed around the Makerfabs DW3000-style pin defaults from the reference firmware.
- The original code used Arduino-oriented DW3000 sources; this repo currently vendors them inside the component and compiles them through ESPHome/PlatformIO.
- `anchor` mode is not yet ported.
