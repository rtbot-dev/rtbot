#ifndef PCT_CHANGE_H
#define PCT_CHANGE_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"
#include <cstring>

namespace rtbot {

class PctChange : public Operator {
 public:
  explicit PctChange(std::string id)
      : Operator(std::move(id)), has_prev_(false), prev_value_(0.0) {
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  void reset() override {
    Operator::reset();
    has_prev_ = false;
    prev_value_ = 0.0;
  }

  std::string type_name() const override { return "PctChange"; }

  Bytes collect() override {
    Bytes bytes = Operator::collect();
    auto append = [&](const auto& value) {
      const auto* raw = reinterpret_cast<const uint8_t*>(&value);
      bytes.insert(bytes.end(), raw, raw + sizeof(value));
    };
    append(has_prev_);
    append(prev_value_);
    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);
    std::memcpy(&has_prev_, &(*it), sizeof(has_prev_));
    it += sizeof(has_prev_);
    std::memcpy(&prev_value_, &(*it), sizeof(prev_value_));
    it += sizeof(prev_value_);
  }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in PctChange");
      }

      double value = msg->data.value;
      if (has_prev_ && prev_value_ != 0.0) {
        double pct = (value - prev_value_) / prev_value_;
        output_queue.push_back(create_message<NumberData>(msg->time, NumberData{pct}));
      }

      prev_value_ = value;
      has_prev_ = true;
      input_queue.pop_front();
    }
  }

 private:
  bool has_prev_;
  double prev_value_;
};

inline std::shared_ptr<Operator> make_pct_change(const std::string& id) {
  return std::make_shared<PctChange>(id);
}

}  // namespace rtbot

#endif  // PCT_CHANGE_H
