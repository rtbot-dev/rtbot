#ifndef RTBOT_FUSED_PARITY_OPCODE_SPEC_H
#define RTBOT_FUSED_PARITY_OPCODE_SPEC_H

#include <limits>
#include <string>
#include <vector>

#include "rtbot/fuse/FusedExpression.h"

namespace rtbot::fused_parity {

struct OpcodeSpec {
  std::string name;
  double opcode;           // value from rtbot::fused_op namespace
  int arity;               // number of stack inputs popped (0, 1, or 2)
  bool has_inline_arg;     // INPUT/CONST/stateful opcodes consume next double
  bool is_transcendental;  // uses libm transcendentals (sin/cos/log/exp/pow)
  bool is_stateful;        // reads or writes persistent state slots

  // Optional input domain restrictions used by fuzzers to avoid trivially
  // divergent behavior (NaN vs NaN comparison, domain errors).
  double min_input_a = -std::numeric_limits<double>::infinity();
  double min_input_b = -std::numeric_limits<double>::infinity();
};

const std::vector<OpcodeSpec>& stateless_non_transcendental_specs();
const std::vector<OpcodeSpec>& stateless_transcendental_specs();
const std::vector<OpcodeSpec>& stateful_specs();

}  // namespace rtbot::fused_parity

#endif  // RTBOT_FUSED_PARITY_OPCODE_SPEC_H
