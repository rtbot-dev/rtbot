#ifndef KEYED_PIPELINE_H
#define KEYED_PIPELINE_H

#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

struct SubGraph {
  std::map<std::string, std::shared_ptr<Operator>> operators;
  std::shared_ptr<Operator> entry;
  std::shared_ptr<Operator> output;
  std::shared_ptr<Collector> collector;
};

class KeyedPipeline : public Operator {
 public:
  using SubGraphFactory = std::function<SubGraph()>;
  using NewKeyCallback = std::function<void(double)>;

  // Old constructor: key is read from input vector at key_index.
  // Output = [key, prototype_output...] (key prepended).
  KeyedPipeline(std::string id, int key_index, SubGraphFactory factory)
      : Operator(std::move(id)), key_index_(key_index), factory_(std::move(factory)) {
    if (key_index < 0) {
      throw std::runtime_error("KeyedPipeline key_index must be non-negative");
    }
    add_data_port<VectorNumberData>();
    add_output_port<VectorNumberData>();
  }

  // New constructor: key is computed internally as a polynomial hash over
  // selected input columns.  The hash coefficients (PRIME^(N-1), ..., 1)
  // are computed automatically — callers only specify WHICH columns.
  // Output = prototype_output directly (no key prepend, no VectorProject needed).
  KeyedPipeline(std::string id, std::vector<int> key_column_indices,
                SubGraphFactory factory)
      : Operator(std::move(id)),
        key_index_(-1),
        factory_(std::move(factory)),
        key_column_indices_(std::move(key_column_indices)) {
    if (key_column_indices_.empty()) {
      throw std::runtime_error(
          "KeyedPipeline key_column_indices must be non-empty");
    }
    for (auto idx : key_column_indices_) {
      if (idx < 0) {
        throw std::runtime_error(
            "KeyedPipeline key_column_indices entries must be non-negative");
      }
    }
    // Pre-compute polynomial hash coefficients: PRIME^(N-1), ..., PRIME^0
    static constexpr double PRIME = 1000003.0;
    key_coefficients_.resize(key_column_indices_.size());
    double coeff = 1.0;
    for (int i = static_cast<int>(key_column_indices_.size()) - 1; i >= 0; --i) {
      key_coefficients_[i] = coeff;
      coeff *= PRIME;
    }
    add_data_port<VectorNumberData>();
    add_output_port<VectorNumberData>();
  }

  std::string type_name() const override { return "KeyedPipeline"; }

  int get_key_index() const { return key_index_; }
  const std::vector<int>& get_key_column_indices() const { return key_column_indices_; }
  bool has_computed_key() const { return !key_column_indices_.empty(); }
  size_t num_keys() const { return sub_graphs_.size(); }

  void set_new_key_callback(NewKeyCallback cb) { new_key_callback_ = std::move(cb); }

  void reset() override {
    Operator::reset();
    sub_graphs_.clear();
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
      double key = std::stod(key_str);
      auto& sg = get_or_create_subgraph_(key);
      for (auto& [op_id, op] : sg.operators) {
        op->restore_data_from_json(key_ops.at(op_id));
      }
    }
  }

  bool equals(const KeyedPipeline& other) const {
    if (key_index_ != other.key_index_) return false;
    if (key_column_indices_ != other.key_column_indices_) return false;
    // key_coefficients_ are derived from key_column_indices_ — no need to compare
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
    while (!input_queue.empty()) {
      const auto* msg = static_cast<const Message<VectorNumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in KeyedPipeline");
      }

      auto time = msg->time;
      double key;

      if (has_computed_key()) {
        // Computed key mode: key = polynomial hash over selected columns
        key = 0.0;
        for (size_t i = 0; i < key_column_indices_.size(); ++i) {
          if (static_cast<size_t>(key_column_indices_[i]) >= msg->data.values->size()) {
            throw std::runtime_error("KeyedPipeline key_column_indices entry out of bounds");
          }
          key += key_coefficients_[i] * (*msg->data.values)[key_column_indices_[i]];
        }
      } else {
        // Classic mode: key is read directly from input vector
        if (static_cast<size_t>(key_index_) >= msg->data.values->size()) {
          throw std::runtime_error("KeyedPipeline key_index out of bounds");
        }
        key = (*msg->data.values)[key_index_];
      }

      auto& sg = get_or_create_subgraph_(key);

      // Clear collector before processing
      sg.collector->reset();

      // Feed the full input vector to the entry operator
      sg.entry->receive_data(input_queue.front()->clone(), 0);
      sg.entry->execute(debug);

      // Collect from collector's data queue, prepend key
      auto& sg_output = sg.collector->get_data_queue(0);
      for (auto& out_msg : sg_output) {
        if (has_computed_key()) {
          // Computed key mode: pass through prototype output as-is (no key prepend)
          auto* vec_out = dynamic_cast<Message<VectorNumberData>*>(out_msg.get());
          if (vec_out) vec_out->time = time;
          emit_output(0, std::move(out_msg), debug);
        } else {
          // Classic mode: prepend key to output
          VectorNumberData result;
          result.values->push_back(key);

          if (out_msg->type() == std::type_index(typeid(VectorNumberData))) {
            const auto* vec_msg = static_cast<const Message<VectorNumberData>*>(out_msg.get());
            result.values->insert(result.values->end(), vec_msg->data.values->begin(),
                                 vec_msg->data.values->end());
          } else if (out_msg->type() == std::type_index(typeid(NumberData))) {
            const auto* num_msg = static_cast<const Message<NumberData>*>(out_msg.get());
            result.values->push_back(num_msg->data.value);
          }

          emit_output(0, create_message<VectorNumberData>(time, std::move(result)), debug);
        }
      }

      input_queue.pop_front();
    }
  }

 private:
  SubGraph& get_or_create_subgraph_(double key) {
    auto it = sub_graphs_.find(key);
    if (it != sub_graphs_.end()) return it->second;

    sub_graphs_[key] = factory_();
    auto& sg = sub_graphs_[key];

    // Attach a collector to capture the output operator's results
    std::vector<std::string> col_types;
    for (size_t i = 0; i < sg.output->num_output_ports(); i++) {
      col_types.push_back(PortType::type_index_to_string(sg.output->get_output_port_type(i)));
    }
    sg.collector = std::make_shared<Collector>(
        sg.output->id() + "_collector", col_types);
    sg.output->connect(sg.collector, 0, 0);

    if (new_key_callback_) {
      new_key_callback_(key);
    }
    return sg;
  }

  int key_index_;
  SubGraphFactory factory_;
  NewKeyCallback new_key_callback_;
  std::map<double, SubGraph> sub_graphs_;
  std::vector<int> key_column_indices_;
  std::vector<double> key_coefficients_;
};

inline std::shared_ptr<KeyedPipeline> make_keyed_pipeline(std::string id, int key_index,
                                                            KeyedPipeline::SubGraphFactory factory) {
  return std::make_shared<KeyedPipeline>(std::move(id), key_index, std::move(factory));
}

inline std::shared_ptr<KeyedPipeline> make_keyed_pipeline(std::string id,
                                                            std::vector<int> key_column_indices,
                                                            KeyedPipeline::SubGraphFactory factory) {
  return std::make_shared<KeyedPipeline>(std::move(id), std::move(key_column_indices),
                                          std::move(factory));
}

}  // namespace rtbot

#endif  // KEYED_PIPELINE_H
