#ifndef RTBOT_FUSED_PARITY_LIVE_DRIVERS_H
#define RTBOT_FUSED_PARITY_LIVE_DRIVERS_H

#include <cstdint>
#include <memory>
#include <vector>

#include "rtbot/fuse/FusedExpression.h"
#include "rtbot/fuse/FusedExpressionVector.h"

namespace rtbot::fused_parity {

// Drive the production FusedExpression through its scalar-port interface
// over a synchronized input sequence. Returns the flattened output stream
// (same shape as RefResult::outputs — [msg * num_outputs + k]).
inline std::vector<double> drive_fused_expression(
    const std::vector<double>& bytecode,
    const std::vector<double>& constants,
    const std::vector<std::vector<double>>& inputs_per_message,
    const std::vector<double>& state_init,
    std::size_t num_ports,
    std::size_t num_outputs) {
  auto op = make_fused_expression("fe", num_ports, num_outputs, bytecode,
                                    constants, state_init);
  for (std::size_t t = 0; t < inputs_per_message.size(); ++t) {
    for (std::size_t p = 0; p < num_ports; ++p) {
      op->receive_data(create_message<NumberData>(
                           static_cast<std::int64_t>(t + 1),
                           NumberData{inputs_per_message[t][p]}),
                       p);
    }
    op->execute();
  }
  auto& q = op->get_output_queue(0);
  std::vector<double> flat;
  flat.reserve(q.size() * num_outputs);
  for (std::size_t t = 0; t < q.size(); ++t) {
    const auto* m = static_cast<const Message<VectorNumberData>*>(q[t].get());
    for (double v : *m->data.values) flat.push_back(v);
  }
  return flat;
}

// Drive the production FusedExpressionVector through its single
// VectorNumberData port. Input messages carry a vector of scalars — one
// message per timestamp.
inline std::vector<double> drive_fused_expression_vector(
    const std::vector<double>& bytecode,
    const std::vector<double>& constants,
    const std::vector<std::vector<double>>& inputs_per_message,
    const std::vector<double>& state_init,
    std::size_t num_outputs) {
  auto op = make_fused_expression_vector("fev", num_outputs, bytecode,
                                           constants, state_init);
  for (std::size_t t = 0; t < inputs_per_message.size(); ++t) {
    auto v = std::make_shared<std::vector<double>>(inputs_per_message[t]);
    op->receive_data(create_message<VectorNumberData>(
                         static_cast<std::int64_t>(t + 1),
                         VectorNumberData(std::move(v))),
                     0);
    op->execute();
  }
  auto& q = op->get_output_queue(0);
  std::vector<double> flat;
  flat.reserve(q.size() * num_outputs);
  for (std::size_t t = 0; t < q.size(); ++t) {
    const auto* m = static_cast<const Message<VectorNumberData>*>(q[t].get());
    for (double v : *m->data.values) flat.push_back(v);
  }
  return flat;
}

}  // namespace rtbot::fused_parity

#endif  // RTBOT_FUSED_PARITY_LIVE_DRIVERS_H
