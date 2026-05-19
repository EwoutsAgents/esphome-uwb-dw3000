# ESPHome UWB DW3000

ESPHome external component for a DW3000-based UWB tag on ESP32 using ESP-IDF.

This repository adapts the working DW3000 reference tag firmware into an ESPHome-native component.

## Current status

### Working
- External component loads and compiles with ESPHome.
- DW3000 init, SPI wiring, and ESP-IDF integration are in place.
- TX start/TX complete handling is hardened.
- Failed TX/RX attempts force the DW3000 out of a bad state and clear status bits.
- Per-anchor retries and invalid-value suppression are implemented.
- Diagnostics now distinguish TX start failure, TX timeout, and RX timeout classes.

### Current blocker
Ranging can still fail at runtime with:
- `outcome=rx_preamble_timeout status=0x00200000`

`rx_preamble_timeout` means the tag transmitted and opened RX, but no response preamble was detected. This usually points to RX timing, PHY/profile mismatch, or an anchor not accepting/responding to the poll.

## Reference-aligned profile

The component is currently aligned with the known-working reference on the key PHY settings:
- channel 5
- preamble length 128
- PAC 8
- TX/RX preamble code 9
- SFD type 4z 8-symbol
- data rate 6.8 Mbps
- PHR mode standard
- PHR rate standard
- STS mode 1
- STS length 128
- reference STS key/IV
- antenna delays `16385`

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
  board: esp-wrover-kit
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
  cs_pin: GPIO4
  irq_pin: GPIO34
  rst_pin: GPIO27
  tag_id: 0x45
  update_interval: 200ms
  anchors:
    - id: 0x01
      x: 0.06
      y: 4.61
      z: 1.83
    - id: 0x05
      x: 2.09
      y: 4.37
      z: 1.74
    - id: 0x04
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
    distance:
      name: "UWB Distance A5"
      anchor_id: 0x05

  - platform: uwb_dw3000
    uwb_dw3000_id: my_uwb
    distance:
      name: "UWB Distance A4"
      anchor_id: 0x04

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

button:
  - platform: restart
    name: "UWB Tag Restart"
```

The example anchor IDs match the known-working reference firmware. If your anchors use different IDs, update both the `anchors:` list and the matching distance sensor `anchor_id` values.

## Practical debug notes

- If you see `tx_start_failed` or `tx_timeout`, focus on local DW3000 TX state, SPI, reset, or timing.
- If you see `rx_preamble_timeout`, the tag transmitted but did not detect a response preamble.
- If you see `rx_frame_timeout`, a preamble was detected but no valid frame completed in time.
- If you see `rx_sfd_timeout`, the receiver did not complete SFD detection.
- If all anchors fail the same way, focus on tag/anchor profile compatibility before trilateration.

## Notes

- The example is based on Makerfabs DW3000-style pin defaults.
- `anchor` mode is not yet ported into this ESPHome component.
