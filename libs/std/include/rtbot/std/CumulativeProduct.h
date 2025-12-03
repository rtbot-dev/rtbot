#ifndef CUMULATIVE_PRODUCT_H
#define CUMULATIVE_PRODUCT_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class CumulativeProduct : public Operator {
 public:
  explicit CumulativeProduct(std::string id) : Operator(std::move(id)), product_(1.0) {
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  void reset() override {
    Operator::reset();
    product_ = 1.0;
  }

  std::string type_name() const override { return "CumulativeProduct"; }

  double current_product() const { return product_; }

  Bytes collect() override {
    Bytes bytes = Operator::collect();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&product_),
                 reinterpret_cast<const uint8_t*>(&product_) + sizeof(product_));
    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);
    product_ = *reinterpret_cast<const double*>(&(*it));
    it += sizeof(double);
  }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in CumulativeProduct");
      }

      product_ *= msg->data.value;
      output_queue.push_back(create_message<NumberData>(msg->time, NumberData{product_}));

      input_queue.pop_front();
    }
  }

 private:
  double product_;
};

inline std::shared_ptr<Operator> make_cumulative_product(const std::string& id) {
  return std::make_shared<CumulativeProduct>(id);
}

}  // namespace rtbot

#endif  // CUMULATIVE_PRODUCT_H
