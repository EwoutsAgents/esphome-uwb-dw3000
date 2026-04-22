#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "trilateration_least_squares.h"
#include "kalman_filter_1d.h"

#include <cmath>
#include <vector>

namespace esphome {
namespace uwb_dw3000 {

class UwbDw3000Component
    : public PollingComponent,
      public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                            spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_8MHZ> {
 public:
  void set_irq_pin(GPIOPin *pin) { this->irq_pin_ = pin; }
  void set_rst_pin(GPIOPin *pin) { this->rst_pin_ = pin; }
  void set_tag_id(uint8_t tag_id) { this->tag_id_ = tag_id; }
  void set_tag_height(float tag_height) { this->tag_height_ = tag_height; }
  void add_anchor(uint8_t id, float x, float y, float z);

  void set_distance_sensor(sensor::Sensor *sens, uint8_t anchor_id);
  void set_distance_filtered_sensor(sensor::Sensor *sens, uint8_t anchor_id);
  void set_x_raw_sensor(sensor::Sensor *sens) { this->x_raw_sensor_ = sens; }
  void set_y_raw_sensor(sensor::Sensor *sens) { this->y_raw_sensor_ = sens; }
  void set_x_filtered_sensor(sensor::Sensor *sens) { this->x_filtered_sensor_ = sens; }
  void set_y_filtered_sensor(sensor::Sensor *sens) { this->y_filtered_sensor_ = sens; }
  void set_fp_power_sensor(sensor::Sensor *sens) { this->fp_power_sensor_ = sens; }
  void set_rx_power_sensor(sensor::Sensor *sens) { this->rx_power_sensor_ = sens; }
  void set_cir_ratio_sensor(sensor::Sensor *sens) { this->cir_ratio_sensor_ = sens; }
  void set_nlos_power_sensor(sensor::Sensor *sens) { this->nlos_power_sensor_ = sens; }
  void set_status_sensor(sensor::Sensor *sens) { this->status_sensor_ = sens; }

  void setup() override;
  void update() override;
  float get_setup_priority() const override;
  void dump_config() override;

 protected:
  struct AnchorRuntime {
    AnchorPosition anchor;
    KalmanFilter1D filter{0.01f, 2.0f, 1.0f, 1.0f};
    float distance_raw{NAN};
    float distance_filtered{NAN};
    sensor::Sensor *distance_sensor{nullptr};
    sensor::Sensor *distance_filtered_sensor{nullptr};
  };

  void publish_measurements_();
  float truncate_(float value, int decimals) const;

  GPIOPin *irq_pin_{nullptr};
  GPIOPin *rst_pin_{nullptr};
  uint8_t tag_id_{0x45};
  float tag_height_{0.78f};
  std::vector<AnchorRuntime> anchors_;
  TrilaterationLS trilat_;

  Point raw_position_{0.0f, 0.0f};
  Point filtered_position_{0.0f, 0.0f};
  float fp_power_{NAN};
  float rx_power_{NAN};
  float cir_ratio_{NAN};
  float nlos_power_{NAN};
  float cir_status_{NAN};

  sensor::Sensor *x_raw_sensor_{nullptr};
  sensor::Sensor *y_raw_sensor_{nullptr};
  sensor::Sensor *x_filtered_sensor_{nullptr};
  sensor::Sensor *y_filtered_sensor_{nullptr};
  sensor::Sensor *fp_power_sensor_{nullptr};
  sensor::Sensor *rx_power_sensor_{nullptr};
  sensor::Sensor *cir_ratio_sensor_{nullptr};
  sensor::Sensor *nlos_power_sensor_{nullptr};
  sensor::Sensor *status_sensor_{nullptr};
};

}  // namespace uwb_dw3000
}  // namespace esphome
