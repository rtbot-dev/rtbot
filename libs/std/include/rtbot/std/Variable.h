#ifndef VARIABLE_H
#define VARIABLE_H

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/Operator.h"
#include "rtbot/StateSerializer.h"

namespace rtbot {

class Variable : public Operator {
 
 public:
  Variable(std::string id, double default_value = 0.0) : Operator(std::move(id)), default_value_(default_value) {
    add_data_port<NumberData>();     // For value updates
    add_control_port<NumberData>();  // For queries
    add_output_port<NumberData>();   // For responses    
  } 

  std::string type_name() const override { return "Variable"; }

  double get_default_value() const { return default_value_; }

  bool equals(const Variable& other) const {
    return (StateSerializer::hash_double(default_value_) == StateSerializer::hash_double(other.default_value_) && Operator::equals(other));
  }

  bool operator==(const Variable& other) const {
    return equals(other);
  }

  bool operator!=(const Variable& other) const {
    return !(*this == other);
  }

 protected:
  void process_data(bool debug=false) override {

    if (!get_data_queue(0).empty() && !get_control_queue(0).empty())
    process_pending_queries();
    
  }

  void process_control(bool debug=false) override {
    
    if (!get_data_queue(0).empty() && !get_control_queue(0).empty())
    process_pending_queries();
  }

 private:
  double default_value_;
  
  void process_pending_queries() {

    auto& data_queue = get_data_queue(0);
    auto& control_queue = get_control_queue(0);
    auto& output_queue = get_output_queue(0);

    if (data_queue.empty() || control_queue.empty()) return;

    while (!control_queue.empty()) {
      const auto* query = dynamic_cast<const Message<NumberData>*>(control_queue.front().get());
      if (!query) {
        throw std::runtime_error("Invalid control message type in Variable");
      }

      const timestamp_t query_time = query->time;

      
      if (data_queue.empty()) break;
      
      bool found = false;
      double result_value = 0.0;
      timestamp_t match_on = 0;
      timestamp_t result_time = 0;

      if (data_queue.size() == 1) {
        const auto* msg = dynamic_cast<const Message<NumberData>*>(data_queue.front().get());
        if (msg->time == query_time) {
          result_value = msg->data.value;
          result_time = query_time;
          match_on = query_time;
          found = true;
        }
      } else {
        auto prev = data_queue.begin();
        auto next = std::next(prev);
        while (next != data_queue.end()) {
          const auto* prev_msg = dynamic_cast<const Message<NumberData>*>((*prev).get());
          const auto* next_msg = dynamic_cast<const Message<NumberData>*>((*next).get());
          if (!prev_msg || !next_msg)
            throw std::runtime_error("Invalid data message type in Variable");

          if (query_time == next_msg->time) {
            result_value = next_msg->data.value;
            result_time = query_time;
            match_on = query_time;
            found = true;
            break;
          } else if (query_time >=prev_msg->time && query_time < next_msg->time) {
            result_value = prev_msg->data.value;
            result_time = query_time;
            match_on = prev_msg->time;
            found = true;
            break;
          }

          ++prev;
          ++next;
        }
      }

      // Stop if query time is before the first or after the last
      const auto* first_msg = dynamic_cast<const Message<NumberData>*>(data_queue.front().get());
      const auto* last_msg =  dynamic_cast<const Message<NumberData>*>(data_queue.back().get());

      if (!found) {
        if (query_time < first_msg->time) {
          output_queue.push_back(create_message<NumberData>(query_time, NumberData{default_value_}));
          control_queue.pop_front();          
        }
        else if (query_time > last_msg->time) {
          while (data_queue.size() > 1) {
              data_queue.pop_front();
          }
          break;
        }
      } else {
        output_queue.push_back(create_message<NumberData>(result_time, NumberData{result_value}));
        control_queue.pop_front();        
        while (!data_queue.empty()) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(data_queue.front().get());
          if (msg->time < match_on) {
            data_queue.pop_front();
          } else {
            break;
          }
        }
      }

    }
  }
};

// Factory function
inline std::unique_ptr<Variable> make_variable(std::string id, double default_value = 0.0) {
  return std::make_unique<Variable>(std::move(id), default_value);
}

}  // namespace rtbot

#endif  // VARIABLE_H