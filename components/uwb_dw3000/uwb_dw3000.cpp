#include "uwb_dw3000.h"
#include "tag_driver.h"

#include "esphome/core/log.h"
#include "dw3000_backend.h"

#include <algorithm>
#include <cmath>

namespace esphome {
namespace uwb_dw3000 {

static const char *const TAG = "uwb_dw3000";

namespace {
UwbDw3000Component *g_active_component = nullptr;

int gpio_pin_number_(GPIOPin *pin) {
  if (pin == nullptr || !pin->is_internal())
    return -1;
  return static_cast<InternalGPIOPin *>(pin)->get_pin();
}

void dw3000_spi_begin_() {
  if (g_active_component != nullptr)
    g_active_component->enable();
}

uint8_t dw3000_spi_transfer_(uint8_t data) {
  if (g_active_component != nullptr)
    return g_active_component->transfer_byte(data);
  return 0;
}

void dw3000_spi_end_() {
  if (g_active_component != nullptr)
    g_active_component->disable();
}
}  // namespace

void UwbDw3000Component::add_anchor(uint8_t id, float x, float y, float z) {
  AnchorRuntime runtime;
  runtime.anchor = AnchorPosition{id, x, y, z};

  const size_t index = this->anchors_.size();
  if (index == 0) {
    runtime.filter = KalmanFilter1D(0.01f, 2.0f, 1.0f, 1.00f);
  } else if (index == 1) {
    runtime.filter = KalmanFilter1D(0.02f, 1.5f, 1.0f, 1.25f);
  } else if (index == 2) {
    runtime.filter = KalmanFilter1D(0.03f, 1.0f, 1.0f, 1.50f);
  }

  this->anchors_.push_back(runtime);
}

void UwbDw3000Component::set_distance_sensor(sensor::Sensor *sens, uint8_t anchor_id) {
  for (auto &anchor : this->anchors_) {
    if (anchor.anchor.id == anchor_id) {
      anchor.distance_sensor = sens;
      return;
    }
  }
}

void UwbDw3000Component::set_distance_filtered_sensor(sensor::Sensor *sens, uint8_t anchor_id) {
  for (auto &anchor : this->anchors_) {
    if (anchor.anchor.id == anchor_id) {
      anchor.distance_filtered_sensor = sens;
      return;
    }
  }
}

void UwbDw3000Component::setup() {
  if (this->irq_pin_ == nullptr || this->rst_pin_ == nullptr) {
    ESP_LOGE(TAG, "Pins are not configured");
    this->mark_failed();
    return;
  }

  this->spi_setup();
  this->irq_pin_->setup();
  this->rst_pin_->setup();

  // Hardware reset pulse for boards with a wired DW3000 reset pin (e.g. Makerfabs v1.1).
  this->rst_pin_->digital_write(false);
  delay(2);
  this->rst_pin_->digital_write(true);
  delay(5);

  this->trilat_.set_tag_height(this->tag_height_);

  std::vector<AnchorPosition> anchors;
  anchors.reserve(this->anchors_.size());
  for (const auto &anchor : this->anchors_) {
    anchors.push_back(anchor.anchor);
  }
  this->trilat_.set_anchors(anchors);

  g_active_component = this;
  dw3000_set_spi_backend(dw3000_spi_begin_, dw3000_spi_transfer_, dw3000_spi_end_);

  const int cs_pin_no = gpio_pin_number_(this->cs_);
  const int irq_pin_no = gpio_pin_number_(this->irq_pin_);
  const int rst_pin_no = gpio_pin_number_(this->rst_pin_);

  if (cs_pin_no < 0 || irq_pin_no < 0 || rst_pin_no < 0) {
    ESP_LOGE(TAG, "Invalid internal pin selection for cs/irq/rst");
    this->mark_failed();
    return;
  }

  uwb_tag_driver_set_pins(TagDriverPins{
      static_cast<uint8_t>(cs_pin_no),
      static_cast<uint8_t>(irq_pin_no),
      static_cast<uint8_t>(rst_pin_no),
  });
  uwb_tag_driver_set_tag_id(this->tag_id_);
  if (!uwb_tag_driver_init()) {
    ESP_LOGE(TAG, "DW3000 init failed (check SPI pins, power, and wiring)");
    this->mark_failed();
    return;
  }
}

void UwbDw3000Component::update() {
  if (this->anchors_.empty())
    return;

  std::vector<float> distances_raw;
  std::vector<float> distances_filtered;
  distances_raw.reserve(this->anchors_.size());
  distances_filtered.reserve(this->anchors_.size());

  for (auto &anchor : this->anchors_) {
    TagDriverCirMetrics metrics{};
    float distance = uwb_tag_driver_range(anchor.anchor.id, &metrics);
    anchor.distance_raw = distance;

    if (!std::isnan(distance) && std::isfinite(distance)) {
      anchor.distance_filtered = anchor.filter.update(distance);
    }

    distances_raw.push_back(anchor.distance_raw);
    distances_filtered.push_back(anchor.distance_filtered);

    this->fp_power_ = metrics.fp_power_db;
    this->rx_power_ = metrics.rx_power_db;
    this->cir_ratio_ = metrics.cir_ratio_db;
    this->nlos_power_ = metrics.nlos_power_db;
    this->cir_status_ = metrics.status;
  }

  bool raw_ok = std::all_of(distances_raw.begin(), distances_raw.end(),
                            [](float value) { return std::isfinite(value); });
  bool filtered_ok = std::all_of(distances_filtered.begin(), distances_filtered.end(),
                                 [](float value) { return std::isfinite(value); });

  if (raw_ok)
    this->raw_position_ = this->trilat_.compute(distances_raw);
  if (filtered_ok)
    this->filtered_position_ = this->trilat_.compute(distances_filtered);

  this->publish_measurements_();
}

void UwbDw3000Component::publish_measurements_() {
  for (auto &anchor : this->anchors_) {
    if (anchor.distance_sensor != nullptr && std::isfinite(anchor.distance_raw)) {
      anchor.distance_sensor->publish_state(this->truncate_(anchor.distance_raw, 3));
    }
    if (anchor.distance_filtered_sensor != nullptr && std::isfinite(anchor.distance_filtered)) {
      anchor.distance_filtered_sensor->publish_state(this->truncate_(anchor.distance_filtered, 3));
    }
  }

  if (this->x_raw_sensor_ != nullptr && std::isfinite(this->raw_position_.x))
    this->x_raw_sensor_->publish_state(this->truncate_(this->raw_position_.x, 3));
  if (this->y_raw_sensor_ != nullptr && std::isfinite(this->raw_position_.y))
    this->y_raw_sensor_->publish_state(this->truncate_(this->raw_position_.y, 3));
  if (this->x_filtered_sensor_ != nullptr && std::isfinite(this->filtered_position_.x))
    this->x_filtered_sensor_->publish_state(this->truncate_(this->filtered_position_.x, 3));
  if (this->y_filtered_sensor_ != nullptr && std::isfinite(this->filtered_position_.y))
    this->y_filtered_sensor_->publish_state(this->truncate_(this->filtered_position_.y, 3));
  if (this->fp_power_sensor_ != nullptr && std::isfinite(this->fp_power_))
    this->fp_power_sensor_->publish_state(this->truncate_(this->fp_power_, 1));
  if (this->rx_power_sensor_ != nullptr && std::isfinite(this->rx_power_))
    this->rx_power_sensor_->publish_state(this->truncate_(this->rx_power_, 1));
  if (this->cir_ratio_sensor_ != nullptr && std::isfinite(this->cir_ratio_))
    this->cir_ratio_sensor_->publish_state(this->truncate_(this->cir_ratio_, 1));
  if (this->nlos_power_sensor_ != nullptr && std::isfinite(this->nlos_power_))
    this->nlos_power_sensor_->publish_state(this->truncate_(this->nlos_power_, 1));
  if (this->status_sensor_ != nullptr && std::isfinite(this->cir_status_))
    this->status_sensor_->publish_state(this->cir_status_);
}

float UwbDw3000Component::truncate_(float value, int decimals) const {
  const float multiplier = powf(10.0f, decimals);
  return roundf(value * multiplier) / multiplier;
}

float UwbDw3000Component::get_setup_priority() const { return setup_priority::DATA; }

void UwbDw3000Component::dump_config() {
  ESP_LOGCONFIG(TAG, "UWB DW3000:");
  ESP_LOGCONFIG(TAG, "  Tag ID: 0x%02X", this->tag_id_);
  ESP_LOGCONFIG(TAG, "  Anchors: %u", static_cast<unsigned>(this->anchors_.size()));
}

}  // namespace uwb_dw3000
}  // namespace esphome
