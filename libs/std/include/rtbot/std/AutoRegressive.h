#ifndef AUTOREGRESSIVE_H
#define AUTOREGRESSIVE_H

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

struct AutoRegressiveFeatures {
  static constexpr bool TRACK_SUM = false;
  static constexpr bool TRACK_MEAN = false;
  static constexpr bool TRACK_VARIANCE = false;
};

class AutoRegressive : public Buffer<NumberData, AutoRegressiveFeatures> {
 public:
  AutoRegressive(std::string id, const std::vector<double>& coefficients)
      : Buffer<NumberData, AutoRegressiveFeatures>(std::move(id), coefficients.size()), coefficients_(coefficients) {
    if (coefficients.empty()) {
      throw std::runtime_error("AutoRegressive requires at least one coefficient");
    }
  }

  const std::vector<double>& get_coefficients() const { return coefficients_; }
  std::string type_name() const override { return "AutoRegressive"; }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData>* msg) override {
    if (!this->buffer_full()) {
      return {};
    }

    double value = 0.0;
    const auto& buf = this->buffer();
    for (size_t i = 0; i < coefficients_.size(); ++i) {
      value += coefficients_[i] * buf[coefficients_.size() - 1 - i]->data.value;
    }

    std::vector<std::unique_ptr<Message<NumberData>>> v;
    v.push_back(create_message<NumberData>(msg->time, NumberData{value}));
    return v;
  }

 private:
  std::vector<double> coefficients_;
};

inline std::unique_ptr<AutoRegressive> make_auto_regressive(std::string id, const std::vector<double>& coefficients) {
  return std::make_unique<AutoRegressive>(std::move(id), coefficients);
}

}  // namespace rtbot

#endif  // AUTOREGRESSIVE_H