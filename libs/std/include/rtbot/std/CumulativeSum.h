// CumulativeSum.h
#ifndef CUMULATIVE_SUM_H
#define CUMULATIVE_SUM_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class CumulativeSum : public Operator {
 public:
  CumulativeSum(std::string id) : Operator(std::move(id)), sum_(0.0) {
    // Add single input and output port for NumberData
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  void reset() override {
    Operator::reset();
    sum_ = 0.0;  // Reset running sum
  }

  std::string type_name() const override { return "CumulativeSum"; }

  // Access current sum
  double get_sum() const { return sum_; }

  // State serialization
  Bytes collect() override {
    Bytes bytes = Operator::collect();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&sum_),
                 reinterpret_cast<const uint8_t*>(&sum_) + sizeof(sum_));
    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);
    sum_ = *reinterpret_cast<const double*>(&(*it));
    it += sizeof(double);
  }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in CumulativeSum");
      }

      // Update sum and create output message
      sum_ += msg->data.value;
      output_queue.push_back(create_message<NumberData>(msg->time, NumberData{sum_}));

      input_queue.pop_front();
    }
  }

 private:
  double sum_;  // Running sum
};

// Factory function for CumulativeSum
inline std::shared_ptr<Operator> make_cumulative_sum(std::string id) { return std::make_shared<CumulativeSum>(id); }

}  // namespace rtbot

#endif  // CUMULATIVE_SUM_H