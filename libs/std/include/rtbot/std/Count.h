#ifndef COUNT_H
#define COUNT_H

#include "rtbot/Operator.h"

namespace rtbot {

/**
 * Counts incoming messages regardless of their type.
 * Has one input port and one output port that emits NumberData.
 */
template <typename T>
class Count : public Operator {
 public:
  explicit Count(std::string id) : Operator(std::move(id)), count_(0) {
    add_data_port<T>();             // Accept any type on input
    add_output_port<NumberData>();  // Output count as number
  }

  std::string type_name() const override { return "Count"; }

  // Serialize count_ since it's our only state
  Bytes collect() override {
    Bytes bytes = Operator::collect();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&count_),
                 reinterpret_cast<const uint8_t*>(&count_) + sizeof(count_));
    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);
    count_ = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(count_);
  }

 protected:
  void process_data() override {
    auto& input = get_data_queue(0);
    auto& output = get_output_queue(0);

    while (!input.empty()) {
      const auto& msg = input.front();
      count_++;
      output.push_back(create_message<NumberData>(msg->time, NumberData{static_cast<double>(count_)}));
      input.pop_front();
    }
  }

 private:
  size_t count_{0};
};

}  // namespace rtbot

#endif  // COUNT_H