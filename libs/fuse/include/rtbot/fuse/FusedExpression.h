#ifndef FUSED_EXPRESSION_H
#define FUSED_EXPRESSION_H

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"
#include "rtbot/fuse/FusedScalarEval.h"
#include "rtbot/std/VectorCompose.h"

namespace rtbot {

// Opcode constants are defined in rtbot/fuse/FusedOps.h so that lower-level
// headers (FusedBytecode.h, FusedScalarEval.h) can reference them without a
// circular include on this header.

class FusedExpression : public VectorCompose {
 public:
  // num_ports: number of scalar NUMBER input ports (same as VectorCompose)
  // num_outputs: number of output columns in the emitted VectorNumberData
  // bytecode: flat double array encoding M expression trees in postfix notation
  // constants: flat double array of compile-time constants referenced by bytecode
  // state_init: initial values for persistent state slots (empty = pure expressions)
  FusedExpression(std::string id, size_t num_ports, size_t num_outputs,
                  std::vector<double> bytecode, std::vector<double> constants,
                  std::vector<double> state_init = {},
                  size_t max_size_per_port = MAX_SIZE_PER_PORT)
      : VectorCompose(std::move(id), num_ports, max_size_per_port),
        num_outputs_(num_outputs),
        bytecode_(std::move(bytecode)),
        constants_(std::move(constants)),
        state_init_(std::move(state_init)),
        state_(state_init_),
        packed_(rtbot::fuse::encode_legacy(bytecode_)) {
    if (num_outputs_ < 1) {
      throw std::runtime_error(
          "FusedExpression requires at least 1 output expression");
    }
    // Validate bytecode has exactly num_outputs END markers.
    // Must walk opcodes respecting argument structure — INPUT, CONST, and
    // stateful opcodes consume the next double as an argument, so those
    // positions must not be counted as opcodes.
    size_t end_count = 0;
    size_t pc = 0;
    while (pc < bytecode_.size()) {
      int opcode = static_cast<int>(bytecode_[pc++]);
      if (opcode == static_cast<int>(fused_op::INPUT) ||
          opcode == static_cast<int>(fused_op::CONST) ||
          opcode == static_cast<int>(fused_op::CUMSUM) ||
          opcode == static_cast<int>(fused_op::COUNT) ||
          opcode == static_cast<int>(fused_op::MAX_AGG) ||
          opcode == static_cast<int>(fused_op::MIN_AGG) ||
          opcode == static_cast<int>(fused_op::STATE_LOAD)) {
        ++pc;  // skip argument
      } else if (opcode == static_cast<int>(fused_op::END)) {
        ++end_count;
      }
    }
    if (end_count != num_outputs_) {
      throw std::runtime_error(
          "FusedExpression bytecode has " + std::to_string(end_count) +
          " END markers but num_outputs is " + std::to_string(num_outputs_));
    }
  }

  std::string type_name() const override { return "FusedExpression"; }

  size_t get_num_outputs() const { return num_outputs_; }
  const std::vector<double>& get_bytecode() const { return bytecode_; }
  const std::vector<double>& get_constants() const { return constants_; }
  const std::vector<double>& get_state_init() const { return state_init_; }

  void reset() override {
    VectorCompose::reset();
    state_ = state_init_;
  }

  Bytes collect_bytes() override {
    Bytes bytes = VectorCompose::collect_bytes();
    size_t n = state_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&n),
                 reinterpret_cast<const uint8_t*>(&n) + sizeof(n));
    for (double v : state_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&v),
                   reinterpret_cast<const uint8_t*>(&v) + sizeof(v));
    }
    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    VectorCompose::restore(it);
    size_t n;
    std::memcpy(&n, &(*it), sizeof(n));
    it += sizeof(n);
    state_.resize(n);
    for (size_t i = 0; i < n; i++) {
      std::memcpy(&state_[i], &(*it), sizeof(double));
      it += sizeof(double);
    }
  }

 protected:
  void process_data(bool debug = false) override {
    const size_t np = get_num_ports();
    const rtbot::fuse::Instruction* ins = packed_.data();
    const size_t ins_size = packed_.size();
    const double* consts = constants_.data();

    while (true) {
      // Use the built-in sync mechanism (inherited from Join via VectorCompose)
      bool is_any_empty = false;
      bool is_sync = sync_data_inputs();
      for (size_t i = 0; i < np; i++) {
        if (get_data_queue(i).empty()) {
          is_any_empty = true;
          break;
        }
      }
      if (!is_sync && is_any_empty) return;
      if (!is_sync) continue;

      // Read synced scalar values from input ports — stack-allocated
      timestamp_t time = 0;
      double inputs[64];
      for (size_t i = 0; i < np; i++) {
        const auto* msg = static_cast<const Message<NumberData>*>(
            get_data_queue(i).front().get());
        time = msg->time;
        inputs[i] = msg->data.value;
      }

      // Pop all input port fronts
      for (size_t i = 0; i < np; i++) {
        get_data_queue(i).pop_front();
      }

      // Allocate output vector once — evaluate_one writes directly into it.
      auto out_vec = std::make_shared<std::vector<double>>(num_outputs_);
      rtbot::fuse::evaluate_one(ins, ins_size, consts, inputs, state_.data(),
                                 out_vec->data(), num_outputs_);

      get_output_queue(0).push_back(create_message<VectorNumberData>(
          time, VectorNumberData(std::move(out_vec))));
    }
  }

 private:
  size_t num_outputs_;
  std::vector<double> bytecode_;
  std::vector<double> constants_;
  std::vector<double> state_init_;
  std::vector<double> state_;
  std::vector<rtbot::fuse::Instruction> packed_;
};

inline std::shared_ptr<FusedExpression> make_fused_expression(
    std::string id, size_t num_ports, size_t num_outputs,
    std::vector<double> bytecode, std::vector<double> constants,
    std::vector<double> state_init = {}) {
  return std::make_shared<FusedExpression>(std::move(id), num_ports,
                                            num_outputs, std::move(bytecode),
                                            std::move(constants),
                                            std::move(state_init));
}

}  // namespace rtbot

#endif  // FUSED_EXPRESSION_H
