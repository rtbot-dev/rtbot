#include "fused_parity/opcode_spec.h"

namespace rtbot::fused_parity {

using namespace rtbot::fused_op;

const std::vector<OpcodeSpec>& stateless_non_transcendental_specs() {
  static const std::vector<OpcodeSpec> v = {
      {"ADD",   ADD,   2, false, false, false},
      {"SUB",   SUB,   2, false, false, false},
      {"MUL",   MUL,   2, false, false, false},
      {"DIV",   DIV,   2, false, false, false},
      {"NEG",   NEG,   1, false, false, false},
      {"ABS",   ABS,   1, false, false, false},
      {"SIGN",  SIGN,  1, false, false, false},
      {"FLOOR", FLOOR, 1, false, false, false},
      {"CEIL",  CEIL,  1, false, false, false},
      {"ROUND", ROUND, 1, false, false, false},
      {"GT",    GT,    2, false, false, false},
      {"GTE",   GTE,   2, false, false, false},
      {"LT",    LT,    2, false, false, false},
      {"LTE",   LTE,   2, false, false, false},
      {"EQ",    EQ,    2, false, false, false},
      {"NEQ",   NEQ,   2, false, false, false},
      {"AND",   AND,   2, false, false, false},
      {"OR",    OR,    2, false, false, false},
      {"NOT",   NOT,   1, false, false, false},
  };
  return v;
}

const std::vector<OpcodeSpec>& stateless_transcendental_specs() {
  static const std::vector<OpcodeSpec> v = {
      {"SQRT",  SQRT,  1, false, true, false, 0.0},
      {"LOG",   LOG,   1, false, true, false, 0.0},
      {"LOG10", LOG10, 1, false, true, false, 0.0},
      {"EXP",   EXP,   1, false, true, false},
      {"SIN",   SIN,   1, false, true, false},
      {"COS",   COS,   1, false, true, false},
      {"TAN",   TAN,   1, false, true, false},
      {"POW",   POW,   2, false, true, false, 0.0},
  };
  return v;
}

const std::vector<OpcodeSpec>& stateful_specs() {
  static const std::vector<OpcodeSpec> v = {
      {"CUMSUM",     CUMSUM,     1, true, false, true},
      {"COUNT",      COUNT,      0, true, false, true},
      {"MAX_AGG",    MAX_AGG,    1, true, false, true},
      {"MIN_AGG",    MIN_AGG,    1, true, false, true},
      {"STATE_LOAD", STATE_LOAD, 0, true, false, true},
  };
  return v;
}

}  // namespace rtbot::fused_parity
