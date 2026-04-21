#ifndef RTBOT_FUSED_OPCODE_SPEC_H
#define RTBOT_FUSED_OPCODE_SPEC_H

#include <array>
#include <cstdint>

#include "rtbot/fuse/FusedExpression.h"

namespace rtbot::fused {

// Opcode classification used by both production code (validators,
// state-layout passes, bytecode printers) and tests (per-opcode differential,
// fuzzer). Single source of truth so categorization cannot drift.

enum class Category {
  StatelessPure,           // arithmetic, unary math, comparisons, booleans
  StatelessTranscendental, // SQRT, LOG, EXP, SIN, COS, TAN, POW, LOG10
  Stateful,                // CUMSUM, COUNT, MAX_AGG, MIN_AGG, STATE_LOAD
  Control,                 // END
};

struct Spec {
  const char* name;
  std::uint8_t opcode;
  std::uint8_t arity;             // stack-pops; 0 for leaves/no-pop ops
  bool has_inline_arg;            // consumes next double in legacy bytecode
  Category category;
};

// Full opcode table, ordered by numeric opcode.
inline constexpr std::array<Spec, 35> kAllSpecs = {{
    {"INPUT",      static_cast<std::uint8_t>(fused_op::INPUT),      0, true,  Category::StatelessPure},
    {"CONST",      static_cast<std::uint8_t>(fused_op::CONST),      0, true,  Category::StatelessPure},
    {"ADD",        static_cast<std::uint8_t>(fused_op::ADD),        2, false, Category::StatelessPure},
    {"SUB",        static_cast<std::uint8_t>(fused_op::SUB),        2, false, Category::StatelessPure},
    {"MUL",        static_cast<std::uint8_t>(fused_op::MUL),        2, false, Category::StatelessPure},
    {"DIV",        static_cast<std::uint8_t>(fused_op::DIV),        2, false, Category::StatelessPure},
    {"POW",        static_cast<std::uint8_t>(fused_op::POW),        2, false, Category::StatelessTranscendental},
    {"ABS",        static_cast<std::uint8_t>(fused_op::ABS),        1, false, Category::StatelessPure},
    {"SQRT",       static_cast<std::uint8_t>(fused_op::SQRT),       1, false, Category::StatelessTranscendental},
    {"LOG",        static_cast<std::uint8_t>(fused_op::LOG),        1, false, Category::StatelessTranscendental},
    {"LOG10",      static_cast<std::uint8_t>(fused_op::LOG10),      1, false, Category::StatelessTranscendental},
    {"EXP",        static_cast<std::uint8_t>(fused_op::EXP),        1, false, Category::StatelessTranscendental},
    {"SIN",        static_cast<std::uint8_t>(fused_op::SIN),        1, false, Category::StatelessTranscendental},
    {"COS",        static_cast<std::uint8_t>(fused_op::COS),        1, false, Category::StatelessTranscendental},
    {"TAN",        static_cast<std::uint8_t>(fused_op::TAN),        1, false, Category::StatelessTranscendental},
    {"SIGN",       static_cast<std::uint8_t>(fused_op::SIGN),       1, false, Category::StatelessPure},
    {"FLOOR",      static_cast<std::uint8_t>(fused_op::FLOOR),      1, false, Category::StatelessPure},
    {"CEIL",       static_cast<std::uint8_t>(fused_op::CEIL),       1, false, Category::StatelessPure},
    {"ROUND",      static_cast<std::uint8_t>(fused_op::ROUND),      1, false, Category::StatelessPure},
    {"NEG",        static_cast<std::uint8_t>(fused_op::NEG),        1, false, Category::StatelessPure},
    {"END",        static_cast<std::uint8_t>(fused_op::END),        0, false, Category::Control},
    {"CUMSUM",     static_cast<std::uint8_t>(fused_op::CUMSUM),     1, true,  Category::Stateful},
    {"COUNT",      static_cast<std::uint8_t>(fused_op::COUNT),      0, true,  Category::Stateful},
    {"MAX_AGG",    static_cast<std::uint8_t>(fused_op::MAX_AGG),    1, true,  Category::Stateful},
    {"MIN_AGG",    static_cast<std::uint8_t>(fused_op::MIN_AGG),    1, true,  Category::Stateful},
    {"STATE_LOAD", static_cast<std::uint8_t>(fused_op::STATE_LOAD), 0, true,  Category::Stateful},
    {"GT",         static_cast<std::uint8_t>(fused_op::GT),         2, false, Category::StatelessPure},
    {"GTE",        static_cast<std::uint8_t>(fused_op::GTE),        2, false, Category::StatelessPure},
    {"LT",         static_cast<std::uint8_t>(fused_op::LT),         2, false, Category::StatelessPure},
    {"LTE",        static_cast<std::uint8_t>(fused_op::LTE),        2, false, Category::StatelessPure},
    {"EQ",         static_cast<std::uint8_t>(fused_op::EQ),         2, false, Category::StatelessPure},
    {"NEQ",        static_cast<std::uint8_t>(fused_op::NEQ),        2, false, Category::StatelessPure},
    {"AND",        static_cast<std::uint8_t>(fused_op::AND),        2, false, Category::StatelessPure},
    {"OR",         static_cast<std::uint8_t>(fused_op::OR),         2, false, Category::StatelessPure},
    {"NOT",        static_cast<std::uint8_t>(fused_op::NOT),        1, false, Category::StatelessPure},
}};

// Fast lookup used by bytecode validators and encoders.
inline constexpr bool has_inline_arg(std::uint8_t op) {
  for (const auto& s : kAllSpecs) {
    if (s.opcode == op) return s.has_inline_arg;
  }
  return false;
}

// Lookup category by opcode; returns Control for unknown opcodes.
inline constexpr Category category_of(std::uint8_t op) {
  for (const auto& s : kAllSpecs) {
    if (s.opcode == op) return s.category;
  }
  return Category::Control;
}

}  // namespace rtbot::fused

#endif  // RTBOT_FUSED_OPCODE_SPEC_H
