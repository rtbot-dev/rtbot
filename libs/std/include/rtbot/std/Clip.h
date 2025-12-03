#ifndef CLIP_H
#define CLIP_H

#include <algorithm>
#include <optional>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class Clip : public Operator {
 public:
  Clip(std::string id, std::optional<double> lower = std::nullopt, std::optional<double> upper = std::nullopt)
      : Operator(std::move(id)), lower_(lower), upper_(upper) {
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  void reset() override { Operator::reset(); }

  std::string type_name() const override { return "Clip"; }

  std::optional<double> lower() const { return lower_; }
  std::optional<double> upper() const { return upper_; }

  Bytes collect() override {
    Bytes bytes = Operator::collect();

    bool has_lower = lower_.has_value();
    bool has_upper = upper_.has_value();
    bytes.push_back(static_cast<uint8_t>(has_lower));
    if (has_lower) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&(*lower_)),
                   reinterpret_cast<const uint8_t*>(&(*lower_)) + sizeof(double));
    }

    bytes.push_back(static_cast<uint8_t>(has_upper));
    if (has_upper) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&(*upper_)),
                   reinterpret_cast<const uint8_t*>(&(*upper_)) + sizeof(double));
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    bool has_lower = static_cast<bool>(*it++);
    if (has_lower) {
      lower_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
    } else {
      lower_.reset();
    }

    bool has_upper = static_cast<bool>(*it++);
    if (has_upper) {
      upper_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
    } else {
      upper_.reset();
    }
  }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in Clip operator.");
      }

      double value = msg->data.value;
      if (lower_) {
        value = std::max(value, *lower_);
      }
      if (upper_) {
        value = std::min(value, *upper_);
      }

      output_queue.push_back(create_message<NumberData>(msg->time, NumberData{value}));
      input_queue.pop_front();
    }
  }

 private:
  std::optional<double> lower_;
  std::optional<double> upper_;
};

inline std::shared_ptr<Operator> make_clip(const std::string& id, std::optional<double> lower = std::nullopt,
                                          std::optional<double> upper = std::nullopt) {
  return std::make_shared<Clip>(id, lower, upper);
}

}  // namespace rtbot

#endif  // CLIP_H
