#ifndef KEYED_PIPELINE_H
#define KEYED_PIPELINE_H

#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

struct SubGraph {
  std::map<std::string, std::shared_ptr<Operator>> operators;
  std::shared_ptr<Operator> entry;
  std::shared_ptr<Operator> output;
};

class KeyedPipeline : public Operator {
 public:
  using SubGraphFactory = std::function<SubGraph()>;
  using NewKeyCallback = std::function<void(uint64_t)>;

  KeyedPipeline(std::string id, SubGraphFactory factory)
      : Operator(std::move(id)), factory_(std::move(factory)) {
    add_data_port<VectorNumberData>();
    add_output_port<VectorNumberData>();
  }

  std::string type_name() const override { return "KeyedPipeline"; }
  size_t num_keys() const { return sub_graphs_.size(); }

  void set_new_key_callback(NewKeyCallback cb) { new_key_callback_ = std::move(cb); }

  void reset() override {
    Operator::reset();
    sub_graphs_.clear();
  }

  void clear_all_output_ports() override {
    Operator::clear_all_output_ports();
    for (auto& [_, sg] : sub_graphs_) {
      for (auto& [__, op] : sg.operators) {
        op->clear_all_output_ports();
      }
    }
  }

  nlohmann::json collect() override {
    nlohmann::json result = {
      {"name", type_name()},
      {"bytes", bytes_to_base64(Operator::collect_bytes())}
    };

    // Nested content: each key maps to its sub-graph operators
    nlohmann::json content;
    for (const auto& [key, sg] : sub_graphs_) {
      nlohmann::json key_ops;
      for (const auto& [op_id, op] : sg.operators) {
        key_ops[op_id] = op->collect();
      }
      content[std::to_string(key)] = key_ops;
    }
    result["content"] = content;

    return result;
  }

  void restore_data_from_json(const nlohmann::json& j) override {
    // Restore base Operator state
    Bytes bytes = base64_to_bytes(j.at("bytes").get<std::string>());
    auto it = bytes.cbegin();
    Operator::restore(it);

    // Restore sub-graphs from "content"
    const auto& content = j.at("content");
    sub_graphs_.clear();
    for (auto& [key_str, key_ops] : content.items()) {
      uint64_t key = std::stoull(key_str);
      sub_graphs_[key] = factory_();
      auto& sg = sub_graphs_[key];
      for (auto& [op_id, op] : sg.operators) {
        op->restore_data_from_json(key_ops.at(op_id));
      }
    }
  }

  bool equals(const KeyedPipeline& other) const {
    if (sub_graphs_.size() != other.sub_graphs_.size()) return false;

    for (const auto& [key, sg] : sub_graphs_) {
      auto it = other.sub_graphs_.find(key);
      if (it == other.sub_graphs_.end()) return false;
      const auto& other_sg = it->second;
      if (sg.operators.size() != other_sg.operators.size()) return false;
      for (const auto& [op_id, op] : sg.operators) {
        auto other_op_it = other_sg.operators.find(op_id);
        if (other_op_it == other_sg.operators.end()) return false;
        if (*op != *other_op_it->second) return false;
      }
    }

    return Operator::equals(other);
  }

  bool operator==(const KeyedPipeline& other) const { return equals(other); }
  bool operator!=(const KeyedPipeline& other) const { return !(*this == other); }

 protected:
  void process_data(bool debug = false) override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<VectorNumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in KeyedPipeline");
      }

      auto time = msg->time;
      uint64_t key = msg->id;

      // Get or create sub-graph for this key
      auto it = sub_graphs_.find(key);
      if (it == sub_graphs_.end()) {
        sub_graphs_[key] = factory_();
        if (new_key_callback_) {
          new_key_callback_(key);
        }
        it = sub_graphs_.find(key);
      }

      auto& sg = it->second;

      // Clear output of the sub-graph's output operator before processing
      sg.output->clear_all_output_ports();

      // Feed the input vector to the entry operator
      sg.entry->receive_data(input_queue.front()->clone(), 0);
      sg.entry->execute(debug);

      // Collect from output operator, set id = key on output messages
      auto& sg_output = sg.output->get_output_queue(0);
      for (const auto& out_msg : sg_output) {
        VectorNumberData result;

        if (out_msg->type() == std::type_index(typeid(VectorNumberData))) {
          const auto* vec_msg = dynamic_cast<const Message<VectorNumberData>*>(out_msg.get());
          result.values = vec_msg->data.values;
        } else if (out_msg->type() == std::type_index(typeid(NumberData))) {
          const auto* num_msg = dynamic_cast<const Message<NumberData>*>(out_msg.get());
          result.values.push_back(num_msg->data.value);
        }

        output_queue.push_back(create_message<VectorNumberData>(time, key, std::move(result)));
      }

      input_queue.pop_front();
    }
  }

 private:
  SubGraphFactory factory_;
  NewKeyCallback new_key_callback_;
  std::map<uint64_t, SubGraph> sub_graphs_;
};

inline std::shared_ptr<KeyedPipeline> make_keyed_pipeline(std::string id,
                                                           KeyedPipeline::SubGraphFactory factory) {
  return std::make_shared<KeyedPipeline>(std::move(id), std::move(factory));
}

}  // namespace rtbot

#endif  // KEYED_PIPELINE_H
