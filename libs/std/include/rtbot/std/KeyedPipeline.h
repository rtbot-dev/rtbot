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
  using NewKeyCallback = std::function<void(double)>;

  KeyedPipeline(std::string id, int key_index, SubGraphFactory factory)
      : Operator(std::move(id)), key_index_(key_index), factory_(std::move(factory)) {
    if (key_index < 0) {
      throw std::runtime_error("KeyedPipeline key_index must be non-negative");
    }
    add_data_port<VectorNumberData>();
    add_output_port<VectorNumberData>();
  }

  std::string type_name() const override { return "KeyedPipeline"; }

  int get_key_index() const { return key_index_; }
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

  Bytes collect() override {
    Bytes bytes = Operator::collect();

    // Number of keys
    size_t nk = sub_graphs_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&nk),
                 reinterpret_cast<const uint8_t*>(&nk) + sizeof(nk));

    // For each key (sorted by key value — deterministic order)
    for (const auto& [key, sg] : sub_graphs_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&key),
                   reinterpret_cast<const uint8_t*>(&key) + sizeof(key));

      // Serialize each operator's state (sorted by id — deterministic order)
      for (const auto& [op_id, op] : sg.operators) {
        Bytes op_bytes = op->collect();
        bytes.insert(bytes.end(), op_bytes.begin(), op_bytes.end());
      }
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    size_t nk;
    std::memcpy(&nk, &(*it), sizeof(nk));
    it += sizeof(nk);

    sub_graphs_.clear();
    for (size_t i = 0; i < nk; i++) {
      double key;
      std::memcpy(&key, &(*it), sizeof(key));
      it += sizeof(key);

      sub_graphs_[key] = factory_();
      auto& sg = sub_graphs_[key];
      for (auto& [op_id, op] : sg.operators) {
        op->restore(it);
      }
    }
  }

  bool equals(const KeyedPipeline& other) const {
    if (key_index_ != other.key_index_) return false;
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

      if (static_cast<size_t>(key_index_) >= msg->data.values.size()) {
        throw std::runtime_error("KeyedPipeline key_index out of bounds");
      }

      auto time = msg->time;
      double key = msg->data.values[key_index_];

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

      // Feed the full input vector to the entry operator
      sg.entry->receive_data(input_queue.front()->clone(), 0);
      sg.entry->execute(debug);

      // Collect from output operator, prepend key
      auto& sg_output = sg.output->get_output_queue(0);
      for (const auto& out_msg : sg_output) {
        VectorNumberData result;
        result.values.push_back(key);

        if (out_msg->type() == std::type_index(typeid(VectorNumberData))) {
          const auto* vec_msg = dynamic_cast<const Message<VectorNumberData>*>(out_msg.get());
          result.values.insert(result.values.end(), vec_msg->data.values.begin(),
                               vec_msg->data.values.end());
        } else if (out_msg->type() == std::type_index(typeid(NumberData))) {
          const auto* num_msg = dynamic_cast<const Message<NumberData>*>(out_msg.get());
          result.values.push_back(num_msg->data.value);
        }

        output_queue.push_back(create_message<VectorNumberData>(time, std::move(result)));
      }

      input_queue.pop_front();
    }
  }

 private:
  int key_index_;
  SubGraphFactory factory_;
  NewKeyCallback new_key_callback_;
  std::map<double, SubGraph> sub_graphs_;
};

inline std::shared_ptr<KeyedPipeline> make_keyed_pipeline(std::string id, int key_index,
                                                           KeyedPipeline::SubGraphFactory factory) {
  return std::make_shared<KeyedPipeline>(std::move(id), key_index, std::move(factory));
}

}  // namespace rtbot

#endif  // KEYED_PIPELINE_H
