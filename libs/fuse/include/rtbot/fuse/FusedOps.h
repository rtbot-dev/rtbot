#ifndef RTBOT_FUSE_OPS_H
#define RTBOT_FUSE_OPS_H

// Bytecode opcodes for FusedExpression RPN evaluation.
//
// Defined in their own small header so that infrastructure headers
// (FusedBytecode.h, FusedScalarEval.h) can depend on the opcode constants
// without pulling in the full FusedExpression / FusedExpressionVector
// operator class definitions (which would create circular includes).

namespace rtbot::fused_op {

constexpr double INPUT = 0;
constexpr double CONST = 1;
constexpr double ADD = 2;
constexpr double SUB = 3;
constexpr double MUL = 4;
constexpr double DIV = 5;
constexpr double POW = 6;
constexpr double ABS = 7;
constexpr double SQRT = 8;
constexpr double LOG = 9;
constexpr double LOG10 = 10;
constexpr double EXP = 11;
constexpr double SIN = 12;
constexpr double COS = 13;
constexpr double TAN = 14;
constexpr double SIGN = 15;
constexpr double FLOOR = 16;
constexpr double CEIL = 17;
constexpr double ROUND = 18;
constexpr double NEG = 19;
constexpr double END = 20;
constexpr double CUMSUM = 21;
constexpr double COUNT = 22;
constexpr double MAX_AGG = 23;
constexpr double MIN_AGG = 24;
constexpr double STATE_LOAD = 25;
constexpr double GT = 26;
constexpr double GTE = 27;
constexpr double LT = 28;
constexpr double LTE = 29;
constexpr double EQ = 30;
constexpr double NEQ = 31;
constexpr double AND = 32;
constexpr double OR = 33;
constexpr double NOT = 34;

// Tier-1 windowed/stateful opcodes (Phase 3). These collapse the standalone
// MovingAverage / MovingSum / StandardDeviation / Difference / WindowMinMax /
// FIR / IIR operators into FusedExpression bytecode so whole chains can fuse.
// Each carries side-table AuxArgs for window size, coefficient offsets, etc.
constexpr double MA_UPDATE = 35;     // running mean over window
constexpr double MSUM_UPDATE = 36;   // running sum over window
constexpr double STD_UPDATE = 37;    // running standard deviation over window
constexpr double DIFF = 38;          // x[t] - x[t-1]
constexpr double SIGN_CHANGE = 39;   // sign(x[t] - x[t-1])
constexpr double WIN_MIN = 40;       // min over sliding window
constexpr double WIN_MAX = 41;       // max over sliding window
constexpr double FIR_UPDATE = 42;    // FIR dot-product with fixed coefficients
constexpr double IIR_UPDATE = 43;    // IIR recurrence with fixed coefficients

// Emission gate. Pops the top of stack; if zero, sets the emit flag to
// false so the surrounding FE/FEV suppresses this message's output. Used to
// absorb WHERE predicates into a projection's bytecode — the predicate
// sub-expression is evaluated with comparison/logical opcodes, then GATE
// replaces the standalone Demultiplexer that would have dropped the tuple.
constexpr double GATE = 44;

}  // namespace rtbot::fused_op

#endif  // RTBOT_FUSE_OPS_H
