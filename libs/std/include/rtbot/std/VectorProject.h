#ifndef VECTOR_PROJECT_H
#define VECTOR_PROJECT_H

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class VectorProject : public Operator {
 public:
  VectorProject(std::string id, std::vector<int> indices) : Operator(std::move(id)), indices_(std::move(indices)) {
    if (indices_.empty()) {
      throw std::runtime_error("VectorProject indices must not be empty");
    }
    for (auto idx : indices_) {
      if (idx < 0) {
        throw std::runtime_error("VectorProject indices must be non-negative");
      }
    }
    add_data_port<VectorNumberData>();
    add_output_port<VectorNumberData>();
  }

  std::string type_name() const override { return "VectorProject"; }
  const std::vector<int>& get_indices() const { return indices_; }

  bool equals(const VectorProject& other) const {
    return indices_ == other.indices_ && Operator::equals(other);
  }

  bool operator==(const VectorProject& other) const { return equals(other); }
  bool operator!=(const VectorProject& other) const { return !(*this == other); }

 protected:
  void process_data(bool debug = false) override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = static_cast<const Message<VectorNumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in VectorProject");
      }

      VectorNumberData result;
      result.values->reserve(indices_.size());
      for (auto idx : indices_) {
        if (static_cast<size_t>(idx) >= msg->data.values->size()) {
          throw std::runtime_error("VectorProject index " + std::to_string(idx) +
                                   " out of bounds for vector of size " + std::to_string(msg->data.values->size()));
        }
        result.values->push_back((*msg->data.values)[idx]);
      }

      output_queue.push_back(create_message<VectorNumberData>(msg->time, result));
      input_queue.pop_front();
    }
  }

 private:
  std::vector<int> indices_;
};

inline std::shared_ptr<VectorProject> make_vector_project(std::string id, std::vector<int> indices) {
  return std::make_shared<VectorProject>(std::move(id), std::move(indices));
}

}  // namespace rtbot

#endif  // VECTOR_PROJECT_H
