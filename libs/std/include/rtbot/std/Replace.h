#ifndef REPLACE_H
#define REPLACE_H

#include <functional>
#include <memory>
#include <string>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class Replace : public Operator {
 public:
  Replace(std::string id) : Operator(std::move(id)) {
    // Add single input and output port for numeric data
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  virtual ~Replace() = default;

  // Pure virtual method that derived classes must implement
  virtual double replace(double value) const = 0;

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in Replace");
      }

      if (!std::isnan(msg->data.value) && std::isfinite(msg->data.value)) {
        double replacement = replace(msg->data.value);
        output_queue.push_back(create_message<NumberData>(msg->time, NumberData{replacement}));
      }

      input_queue.pop_front();
    }
  }
};

// Concrete implementations for various filter operations

class LessThanOrEqualToReplace : public Replace {
 public:
  LessThanOrEqualToReplace(std::string id, double threshold, double replaceBy)
      : Replace(std::move(id)), threshold_(threshold), replaceBy_(replaceBy) {}
  std::string type_name() const override { return "LessThanOrEqualToReplace"; }

  double get_threshold() const { return threshold_; }
  double get_replace_by() const { return replaceBy_; }
  double replace(double x) const override { return (x <= threshold_) ? replaceBy_ : x; }

 private:
  double threshold_;
  double replaceBy_;
};

// Factory functions
inline std::shared_ptr<LessThanOrEqualToReplace> make_less_than_or_equal_to_replace(std::string id, double threshold,
                                                                                    double replaceBy) {
  return std::make_shared<LessThanOrEqualToReplace>(std::move(id), threshold, replaceBy);
}

}  // namespace rtbot

#endif  // REPLACE_H