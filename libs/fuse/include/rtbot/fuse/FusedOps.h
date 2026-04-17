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

}  // namespace rtbot::fused_op

#endif  // RTBOT_FUSE_OPS_H
