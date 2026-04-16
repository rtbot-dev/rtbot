#ifndef VECTOR_EXTRACT_H
#define VECTOR_EXTRACT_H

#include <memory>
#include <stdexcept>
#include <string>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class VectorExtract : public Operator {
 public:
  VectorExtract(std::string id, int index) : Operator(std::move(id)), index_(index) {
    if (index < 0) {
      throw std::runtime_error("VectorExtract index must be non-negative");
    }
    add_data_port<VectorNumberData>();
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "VectorExtract"; }
  int get_index() const { return index_; }

  bool equals(const VectorExtract& other) const {
    return index_ == other.index_ && Operator::equals(other);
  }

  bool operator==(const VectorExtract& other) const { return equals(other); }
  bool operator!=(const VectorExtract& other) const { return !(*this == other); }

 protected:
  void process_data(bool debug = false) override {
    auto& input_queue = get_data_queue(0);
    while (!input_queue.empty()) {
      const auto* msg = static_cast<const Message<VectorNumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in VectorExtract");
      }
      if (static_cast<size_t>(index_) >= msg->data.values->size()) {
        throw std::runtime_error("VectorExtract index " + std::to_string(index_) +
                                 " out of bounds for vector of size " + std::to_string(msg->data.values->size()));
      }
      emit_output(0, create_message<NumberData>(msg->time, NumberData{(*msg->data.values)[index_]}), debug);
      input_queue.pop_front();
    }
  }

 private:
  int index_;
};

inline std::shared_ptr<VectorExtract> make_vector_extract(std::string id, int index) {
  return std::make_shared<VectorExtract>(std::move(id), index);
}

}  // namespace rtbot

#endif  // VECTOR_EXTRACT_H
