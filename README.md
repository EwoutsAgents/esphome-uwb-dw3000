# ESPHome UWB DW3000

ESPHome external component for a DW3000-based UWB **tag** on ESP32 (ESP-IDF).

This repository adapts the original reference tag firmware into an ESPHome-native component structure.

## Status (current)

### ✅ What is working
- External component loads and compiles with ESPHome (`2026.4.1`).
- DW3000 init path is integrated in ESPHome and no longer depends on Arduino runtime APIs.
- SPI + pin wiring path is validated on Makerfabs ESP32 UWB DW3000 style wiring.
- Runtime guardrails are in place to avoid watchdog lockups:
  - init timeout/fail-fast path
  - TX/RX timeout handling
  - component failure marking when hardware init fails
- Per-anchor runtime diagnostics are available (e.g. `outcome=ok`, `outcome=rx_timeout`, etc.).
- Example YAML now includes a restart button for quick reboot/testing from Home Assistant.

### ⚠️ Current blocker
Ranging currently times out in runtime logs for tested setups:

- `outcome=rx_timeout status=0x00020000`
- `0x00020000` maps to `SYS_STATUS_RXFTO_BIT_MASK` (RX frame timeout)

This means the tag sends poll frames, but no valid response frame is received in the configured receive window.

## What has been done

- Ported tag logic into `components/uwb_dw3000` as an ESPHome component.
- Flattened and fixed DW3000 source layout/build integration.
- Fixed ESPHome Python schema/import issues in component config.
- Fixed GPIO handling and added graceful failure behavior.
- Cleaned UART const-correctness warnings in DW3000 adapter code.
- Added per-anchor debug logging to classify runtime outcomes.

## What comes next

1. **Anchor compatibility verification**
   - Confirm active anchor ID(s)
   - Confirm anchor firmware is running and responding
2. **PHY/profile alignment check (tag ↔ anchor)**
   - channel, preamble, data rate
   - STS mode/length and STS key/IV expectations
3. **Timing window validation**
   - align poll/response delay and RX timeout constants with working reference values
4. **Optional compatibility path**
   - add an explicit non-STS/compat mode toggle for quick A/B testing
5. **Anchor-mode port (future)**
   - ESPHome anchor component support is still pending

## Example usage

```yaml
esphome:
  name: uwb-tag-try
  friendly_name: UWB_tag_try
  platformio_options:
    build_flags:
      - -std=gnu++17

esp32:
  board: esp-wrover-kit
  framework:
    type: esp-idf

# Enable logging
logger:

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

  # Enable fallback hotspot (captive portal) in case wifi connection fails
  ap:
    ssid: "Uwb-Tag-Try Fallback Hotspot"
    password: "cLvs9zlPJlsI"

captive_portal:


button:
  - platform: restart
    name: "UWB Tag Restart"
    
external_components:
  - source:
      type: git
      url: https://github.com/EwoutsAgents/esphome-uwb-dw3000
      ref: main
    components: [uwb_dw3000]
    refresh: 0s

spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

uwb_dw3000:
  id: my_uwb
  role: tag
  data_rate: 2MHz
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
    - id: 0x02
      x: 2.09
      y: 4.37
      z: 1.74
    - id: 0x03
      x: 2.20
      y: 0.06
      z: 1.83
    - id: 0x04
      x: 2.00
      y: 0.06
      z: 1.00
    - id: 0x05
      x: 2.12
      y: 0.02
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

## Practical debug notes

- If anchor ID is unknown, test one anchor at a time (or temporarily poll multiple IDs) and watch per-anchor outcomes.
- With only one anchor active, distance can work but XY trilateration is not meaningful yet.
- If all anchors show `rx_timeout`, focus on tag/anchor profile compatibility before further SPI tuning.

## Notes

- Designed around Makerfabs DW3000-style pin defaults from the reference firmware.
- `anchor` mode is not yet ported into this ESPHome component.
