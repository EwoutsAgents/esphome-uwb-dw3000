#pragma once

namespace esphome {
namespace uwb_dw3000 {

class KalmanFilter1D {
 public:
  KalmanFilter1D(float process_noise, float measurement_noise, float estimation_error,
                 float initial_value = 0.0f)
      : q_(process_noise),
        r_(measurement_noise),
        p_(estimation_error),
        x_(initial_value),
        last_k_(0.0f) {}

  float update(float measurement) {
    this->p_ = this->p_ + this->q_;
    const float k = this->p_ / (this->p_ + this->r_);
    this->last_k_ = k;
    this->x_ = this->x_ + k * (measurement - this->x_);
    this->p_ = (1.0f - k) * this->p_;
    return this->x_;
  }

  float get_estimate() const { return this->x_; }

 protected:
  float q_;
  float r_;
  float p_;
  float x_;
  float last_k_;
};

}  // namespace uwb_dw3000
}  // namespace esphome
