#include <catch2/catch.hpp>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/Transcendental.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"
#include "rtbot/fuse/FusedScalarEval.h"
#include "rtbot/fuse/FusedStateLayout.h"
#include "libs/compiled/jit_spike/JitContext.h"

using namespace rtbot::jit;
using namespace rtbot::jit::emit;
namespace fused_op = rtbot::fused_op;

namespace {

// Bit-cast helper — interprets the double's object representation as uint64_t.
inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

// ---------------------------------------------------------------------------
// Shared JIT instance. All tests add modules to the same LLJIT dylib, so each
// function name must be globally unique — the helpers below accept fn_name for
// that purpose.
// ---------------------------------------------------------------------------
static rtbot::JitContext& global_jit() {
  static rtbot::JitContext jit;
  return jit;
}

// ---------------------------------------------------------------------------
// Build a JIT-compiled double(*)(double) function calling emit_fn(ec, arg).
// Returns a callable function pointer; the function is registered under
// fn_name in the global JIT dylib (caller must pick a unique name).
// ---------------------------------------------------------------------------
template <class UnaryEmitFn>
double (*build_unary_jit_fn(const char* fn_name, UnaryEmitFn emit_fn))(double) {
  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>(fn_name, *ctx);
  llvm::Type* dbl = llvm::Type::getDoubleTy(*ctx);
  llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl}, false);
  llvm::Function* fn =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fn_name, mod.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
  llvm::IRBuilder<> b(bb);
  b.setFastMathFlags(llvm::FastMathFlags{});
  IrEmissionContext ec(*ctx, *mod, b, /*state_ptr=*/nullptr);
  llvm::Value* result = emit_fn(ec, fn->getArg(0));
  b.CreateRet(result);
  global_jit().compile_module(std::move(mod), std::move(ctx));
  return global_jit().lookup<double (*)(double)>(fn_name);
}

// ---------------------------------------------------------------------------
// Build a packed FE program for a unary opcode: INPUT 0, <op>, END.
// Returns (packed instructions, initial state vector).
// ---------------------------------------------------------------------------
std::pair<std::vector<rtbot::fuse::Instruction>, std::vector<double>>
build_unary_fe_program(std::uint8_t fe_opcode) {
  std::vector<double> bc = {
      fused_op::INPUT, 0.0,
      static_cast<double>(fe_opcode),
      fused_op::END,
  };
  auto pack = rtbot::fuse::pack_bytecode(bc);
  auto layout = rtbot::fuse::compute_state_layout(pack.packed, pack.aux_args);
  return {pack.packed, layout.initial_values};
}

// ---------------------------------------------------------------------------
// Run one FE unary evaluation.
// ---------------------------------------------------------------------------
double fe_eval_unary(
    const std::vector<rtbot::fuse::Instruction>& ins,
    std::vector<double>& state,
    double input) {
  double scratch = 0.0;
  double inputs[1] = {input};
  rtbot::fuse::evaluate_one(ins.data(), ins.size(),
                            /*constants=*/nullptr,
                            /*aux_args=*/nullptr,
                            /*coefficients=*/nullptr,
                            inputs, state.data(), &scratch, 1);
  return scratch;
}

// ---------------------------------------------------------------------------
// Parity helper: test a unary emitter against the FE interpreter over 50
// deterministic inputs drawn from [lo, hi).
// ---------------------------------------------------------------------------
template <class UnaryEmitFn>
void test_unary_parity(
    std::uint8_t fe_opcode,
    UnaryEmitFn emit_fn,
    const char* fn_name,
    std::pair<double, double> input_range,
    std::uint64_t seed = 0xACED) {
  auto [lo, hi] = input_range;

  // JIT side
  auto jit_fn = build_unary_jit_fn(fn_name, emit_fn);
  REQUIRE(jit_fn != nullptr);

  // FE side
  auto [packed, state_init] = build_unary_fe_program(fe_opcode);

  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(lo, hi);

  for (int i = 0; i < 50; ++i) {
    double v = dist(rng);
    std::vector<double> state = state_init;  // fresh state per call (stateless ops)
    double fe_result = fe_eval_unary(packed, state, v);
    double jit_result = jit_fn(v);
    INFO("opcode=" << static_cast<int>(fe_opcode) << " i=" << i << " v=" << v
                   << " fe=" << fe_result << " jit=" << jit_result);
    REQUIRE(dbits(jit_result) == dbits(fe_result));
  }
}

// ---------------------------------------------------------------------------
// Build a JIT-compiled double(*)(double, double) function for a binary emitter.
// ---------------------------------------------------------------------------
template <class BinaryEmitFn>
double (*build_binary_jit_fn(const char* fn_name, BinaryEmitFn emit_fn))(double, double) {
  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>(fn_name, *ctx);
  llvm::Type* dbl = llvm::Type::getDoubleTy(*ctx);
  llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl, dbl}, false);
  llvm::Function* fn =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fn_name, mod.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
  llvm::IRBuilder<> b(bb);
  b.setFastMathFlags(llvm::FastMathFlags{});
  IrEmissionContext ec(*ctx, *mod, b, /*state_ptr=*/nullptr);
  llvm::Value* result = emit_fn(ec, fn->getArg(0), fn->getArg(1));
  b.CreateRet(result);
  global_jit().compile_module(std::move(mod), std::move(ctx));
  return global_jit().lookup<double (*)(double, double)>(fn_name);
}

// ---------------------------------------------------------------------------
// Build a packed FE program for a binary opcode: INPUT 0, INPUT 1, <op>, END.
// ---------------------------------------------------------------------------
std::pair<std::vector<rtbot::fuse::Instruction>, std::vector<double>>
build_binary_fe_program(std::uint8_t fe_opcode) {
  std::vector<double> bc = {
      fused_op::INPUT, 0.0,
      fused_op::INPUT, 1.0,
      static_cast<double>(fe_opcode),
      fused_op::END,
  };
  auto pack = rtbot::fuse::pack_bytecode(bc);
  auto layout = rtbot::fuse::compute_state_layout(pack.packed, pack.aux_args);
  return {pack.packed, layout.initial_values};
}

double fe_eval_binary(
    const std::vector<rtbot::fuse::Instruction>& ins,
    std::vector<double>& state,
    double a,
    double b) {
  double scratch = 0.0;
  double inputs[2] = {a, b};
  std::vector<double> empty_state;
  if (state.empty()) empty_state.resize(1, 0.0);
  rtbot::fuse::evaluate_one(ins.data(), ins.size(),
                            /*constants=*/nullptr,
                            /*aux_args=*/nullptr,
                            /*coefficients=*/nullptr,
                            inputs, state.empty() ? empty_state.data() : state.data(),
                            &scratch, 1);
  return scratch;
}

}  // namespace

// ===========================================================================
// Individual SCENARIO blocks — one per emitter
// ===========================================================================

SCENARIO("emit_abs matches FE ABS bit-exactly", "[transcendental][abs][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::ABS),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_abs(ec, v); },
      "test_abs",
      {-1e6, 1e6});
}

SCENARIO("emit_sqrt matches FE SQRT bit-exactly", "[transcendental][sqrt][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::SQRT),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_sqrt(ec, v); },
      "test_sqrt",
      {0.0, 1e6});
}

SCENARIO("emit_log matches FE LOG bit-exactly", "[transcendental][log][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::LOG),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_log(ec, v); },
      "test_log",
      {1e-9, 1e6});
}

SCENARIO("emit_log10 matches FE LOG10 bit-exactly", "[transcendental][log10][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::LOG10),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_log10(ec, v); },
      "test_log10",
      {1e-9, 1e6});
}

SCENARIO("emit_exp matches FE EXP bit-exactly", "[transcendental][exp][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::EXP),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_exp(ec, v); },
      "test_exp",
      {-20.0, 20.0});
}

SCENARIO("emit_sin matches FE SIN bit-exactly", "[transcendental][sin][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::SIN),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_sin(ec, v); },
      "test_sin",
      {-1e4, 1e4});
}

SCENARIO("emit_cos matches FE COS bit-exactly", "[transcendental][cos][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::COS),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_cos(ec, v); },
      "test_cos",
      {-1e4, 1e4});
}

SCENARIO("emit_tan matches FE TAN bit-exactly", "[transcendental][tan][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::TAN),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_tan(ec, v); },
      "test_tan",
      {-1e4, 1e4});
}

SCENARIO("emit_sign matches FE SIGN bit-exactly", "[transcendental][sign][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::SIGN),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_sign(ec, v); },
      "test_sign",
      {-1e6, 1e6});
}

SCENARIO("emit_floor matches FE FLOOR bit-exactly", "[transcendental][floor][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::FLOOR),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_floor(ec, v); },
      "test_floor",
      {-1e6, 1e6});
}

SCENARIO("emit_ceil matches FE CEIL bit-exactly", "[transcendental][ceil][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::CEIL),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_ceil(ec, v); },
      "test_ceil",
      {-1e6, 1e6});
}

SCENARIO("emit_round matches FE ROUND bit-exactly", "[transcendental][round][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::ROUND),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_round(ec, v); },
      "test_round",
      {-1e6, 1e6});
}

SCENARIO("emit_neg matches FE NEG bit-exactly", "[transcendental][neg][parity]") {
  test_unary_parity(
      static_cast<std::uint8_t>(fused_op::NEG),
      [](IrEmissionContext& ec, llvm::Value* v) { return emit_neg(ec, v); },
      "test_neg",
      {-1e6, 1e6});
}

// ---------------------------------------------------------------------------
// POW — binary emitter
// ---------------------------------------------------------------------------
SCENARIO("emit_pow matches FE POW bit-exactly", "[transcendental][pow][parity]") {
  // Positive base to avoid complex/NaN from fractional exponents.
  auto jit_fn = build_binary_jit_fn(
      "test_pow",
      [](IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
        return emit_pow(ec, a, b);
      });
  REQUIRE(jit_fn != nullptr);

  auto [packed, state_init] =
      build_binary_fe_program(static_cast<std::uint8_t>(fused_op::POW));

  std::mt19937_64 rng(0xACED);
  std::uniform_real_distribution<double> base_dist(1e-3, 1e3);
  std::uniform_real_distribution<double> exp_dist(-4.0, 4.0);

  for (int i = 0; i < 50; ++i) {
    double a = base_dist(rng);
    double b = exp_dist(rng);
    std::vector<double> state = state_init;
    double fe_result = fe_eval_binary(packed, state, a, b);
    double jit_result = jit_fn(a, b);
    INFO("i=" << i << " a=" << a << " b=" << b
              << " fe=" << fe_result << " jit=" << jit_result);
    REQUIRE(dbits(jit_result) == dbits(fe_result));
  }
}
