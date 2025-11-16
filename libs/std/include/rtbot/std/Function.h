#ifndef FUNCTION_H
#define FUNCTION_H

#include <algorithm>
#include <cmath>
#include <vector>

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

enum class InterpolationType { LINEAR, HERMITE };

class Function : public Operator {
 public:
  Function(std::string id, std::vector<std::pair<double, double>> points,
           InterpolationType type = InterpolationType::LINEAR)
      : Operator(std::move(id)) {
    if (points.size() < 2) {
      throw std::runtime_error("At least 2 points are required for interpolation");
    }

    std::sort(points.begin(), points.end());
    points_ = std::move(points);
    type_ = type;

    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "Function"; }

  const std::vector<std::pair<double, double>>& get_points() const { return points_; }
  InterpolationType get_interpolation_type() const { return type_; }

  bool equals(const Function& other) const {
    return (points_ == other.points_ && type_ == other.type_ && Operator::equals(other));
  }

  bool operator==(const Function& other) const {
    return equals(other);
  }

  bool operator!=(const Function& other) const {
      return !(*this == other);
  }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in Function");
      }

      double x = msg->data.value;
      double y = interpolate(x);

      output_queue.push_back(create_message<NumberData>(msg->time, NumberData{y}));
      input_queue.pop_front();
    }
  }

 private:
  double interpolate(double x) const {
    size_t i = 0;
    while (i < points_.size() - 1 && points_[i + 1].first <= x) {
      i++;
    }

    if (type_ == InterpolationType::LINEAR) {
      if (i == 0 && x < points_[0].first) {
        return linear_interpolate(points_[0].first, points_[0].second, points_[1].first, points_[1].second, x);
      } else if (i == points_.size() - 1) {
        return linear_interpolate(points_[i - 1].first, points_[i - 1].second, points_[i].first, points_[i].second, x);
      }
      return linear_interpolate(points_[i].first, points_[i].second, points_[i + 1].first, points_[i + 1].second, x);
    } else {
      if (i == 0 && x < points_[0].first) {
        return hermite_interpolate(points_[0].second, points_[1].second, get_tangent(0), get_tangent(1),
                                   (x - points_[0].first) / (points_[1].first - points_[0].first));
      } else if (i >= points_.size() - 2) {
        size_t last = points_.size() - 1;
        return hermite_interpolate(points_[last - 1].second, points_[last].second, get_tangent(last - 1),
                                   get_tangent(last),
                                   (x - points_[last - 1].first) / (points_[last].first - points_[last - 1].first));
      }
      return hermite_interpolate(points_[i].second, points_[i + 1].second, get_tangent(i), get_tangent(i + 1),
                                 (x - points_[i].first) / (points_[i + 1].first - points_[i].first));
    }
  }

  static double linear_interpolate(double x1, double y1, double x2, double y2, double x) {
    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
  }

  static double hermite_interpolate(double y0, double y1, double m0, double m1, double mu) {
    double mu2 = mu * mu;
    double mu3 = mu2 * mu;
    double h00 = 2 * mu3 - 3 * mu2 + 1;
    double h10 = mu3 - 2 * mu2 + mu;
    double h01 = -2 * mu3 + 3 * mu2;
    double h11 = mu3 - mu2;

    return h00 * y0 + h10 * m0 + h01 * y1 + h11 * m1;
  }

  double get_tangent(size_t i) const {
    if (i == 0) {
      return (points_[1].second - points_[0].second) / (points_[1].first - points_[0].first);
    } else if (i == points_.size() - 1) {
      return (points_[i].second - points_[i - 1].second) / (points_[i].first - points_[i - 1].first);
    }
    return (points_[i + 1].second - points_[i - 1].second) / (points_[i + 1].first - points_[i - 1].first);
  }

  std::vector<std::pair<double, double>> points_;
  InterpolationType type_;
};

inline std::shared_ptr<Operator> make_function(const std::string& id,
                                               const std::vector<std::pair<double, double>>& points,
                                               InterpolationType type = InterpolationType::LINEAR) {
  return std::make_shared<Function>(id, points, type);
}

}  // namespace rtbot

#endif  // FUNCTION_H