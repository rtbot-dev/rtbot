// CumulativeSum.h
#ifndef CUMULATIVE_SUM_H
#define CUMULATIVE_SUM_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class CumulativeSum : public Operator {
 public:
  CumulativeSum(std::string id) : Operator(std::move(id)), sum_(0.0), sum_comp_(0.0) {
    // Add single input and output port for NumberData
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  void reset() override {
    Operator::reset();
    sum_ = 0.0;       // Reset running sum
    sum_comp_ = 0.0;   // Reset Kahan compensation
  }

  std::string type_name() const override { return "CumulativeSum"; }

  // Access current sum
  double get_sum() const { return sum_; }
  
  bool equals(const CumulativeSum& other) const {
    return (StateSerializer::hash_double(sum_) == StateSerializer::hash_double(other.sum_) && Operator::equals(other));
  }
  
  bool operator==(const CumulativeSum& other) const {
    return equals(other);  // still check base class
  }

  bool operator!=(const CumulativeSum& other) const {
    return !(*this == other);
  }

  // State serialization
  Bytes collect_bytes() override {
    Bytes bytes = Operator::collect_bytes();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&sum_),
                 reinterpret_cast<const uint8_t*>(&sum_) + sizeof(sum_));
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&sum_comp_),
                 reinterpret_cast<const uint8_t*>(&sum_comp_) + sizeof(sum_comp_));
    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    // Restore base state first
    Operator::restore(it);

    std::memcpy(&sum_, &(*it), sizeof(sum_));
    it += sizeof(sum_);
    std::memcpy(&sum_comp_, &(*it), sizeof(sum_comp_));
    it += sizeof(sum_comp_);
  }

 protected:
  void process_data(bool debug=false) override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in CumulativeSum");
      }

      // Kahan-compensated addition: keeps drift at O(1·ε) instead of O(N·ε)
      // for this unbounded cumulative sum.
      double y = msg->data.value - sum_comp_;
      double t = sum_ + y;
      sum_comp_ = (t - sum_) - y;
      sum_ = t;
      output_queue.push_back(create_message<NumberData>(msg->time, NumberData{sum_}));

      input_queue.pop_front();
    }
  }

 private:
  double sum_;       // Running sum
  double sum_comp_;  // Kahan compensation term
};

// Factory function for CumulativeSum
inline std::shared_ptr<Operator> make_cumulative_sum(std::string id) { return std::make_shared<CumulativeSum>(id); }

}  // namespace rtbot

#endif  // CUMULATIVE_SUM_H