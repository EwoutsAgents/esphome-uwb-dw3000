#pragma once

#include <cstdint>

namespace esphome {
namespace uwb_dw3000 {

struct TagDriverPins {
  uint8_t ss_pin;
  uint8_t irq_pin;
  uint8_t rst_pin;
};

struct TagDriverCirMetrics {
  float fp_power_db;
  float rx_power_db;
  float cir_ratio_db;
  float nlos_power_db;
  uint8_t status;
  uint8_t dgc_decision;
};

void uwb_tag_driver_set_pins(const TagDriverPins &pins);
void uwb_tag_driver_set_tag_id(uint8_t tag_id);
void uwb_tag_driver_set_rx_after_tx_delay_uus(uint16_t delay_uus);
bool uwb_tag_driver_init();
float uwb_tag_driver_range(uint8_t anchor_id, TagDriverCirMetrics *metrics);

}  // namespace uwb_dw3000
}  // namespace esphome
