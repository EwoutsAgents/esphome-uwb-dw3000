#pragma once

#include <vector>
#include <cmath>

namespace esphome {
namespace uwb_dw3000 {

struct Point {
  float x;
  float y;
};

struct AnchorPosition {
  uint8_t id;
  float x;
  float y;
  float z;
};

class TrilaterationLS {
 public:
  TrilaterationLS() = default;
  explicit TrilaterationLS(float tag_z) : tag_z_(tag_z) {}

  void set_tag_height(float tag_z) { this->tag_z_ = tag_z; }
  void set_anchors(const std::vector<AnchorPosition> &anchors) { this->anchors_ = anchors; }

  Point compute(const std::vector<float> &distances) const {
    Point result{0.0f, 0.0f};
    if (this->anchors_.size() < 3 || distances.size() != this->anchors_.size()) {
      return result;
    }

    float x = 0.0f;
    float y = 0.0f;
    constexpr float learning_rate = 0.01f;
    constexpr int max_iterations = 1000;

    for (int iter = 0; iter < max_iterations; iter++) {
      float grad_x = 0.0f;
      float grad_y = 0.0f;

      for (size_t i = 0; i < this->anchors_.size(); i++) {
        const float dx = x - this->anchors_[i].x;
        const float dy = y - this->anchors_[i].y;
        const float dz = this->tag_z_ - this->anchors_[i].z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float error = dist - distances[i];

        if (dist > 1e-6f) {
          grad_x += (error * dx) / dist;
          grad_y += (error * dy) / dist;
        }
      }

      x -= learning_rate * grad_x;
      y -= learning_rate * grad_y;
    }

    result.x = x;
    result.y = y;
    return result;
  }

 protected:
  std::vector<AnchorPosition> anchors_;
  float tag_z_{0.0f};
};

}  // namespace uwb_dw3000
}  // namespace esphome
