#ifndef FILTER_BY_VALUE_H
#define FILTER_BY_VALUE_H

#include <functional>

#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

template <typename T>
class FilterByValue : public Operator {
 public:
  using FilterFunction = std::function<bool(const T&)>;

  FilterByValue(std::string id, FilterFunction filter) : Operator(std::move(id)), filter_(std::move(filter)) {
    // Add single data input port
    add_data_port<T>();

    // Add single output port
    add_output_port<T>();
  }

  bool equals(const FilterByValue& other) const {
    return Operator::equals(other);
  }

 protected:
  void process_data(bool debug=false) override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<T>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in FilterByValue");
      }

      // Apply filter
      if (filter_(msg->data)) {
        output_queue.push_back(input_queue.front()->clone());
      }

      input_queue.pop_front();
    }
  }

 private:
  FilterFunction filter_;
};

}  // namespace rtbot

#endif  // FILTER_BY_VALUE_H