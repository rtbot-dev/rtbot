#include <catch2/catch.hpp>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <random>
#include <vector>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/Aggregate.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"
#include "rtbot/fuse/FusedScalarEval.h"
#include "rtbot/fuse/FusedStateLayout.h"
#include "libs/compiled/jit_spike/JitContext.h"

using namespace rtbot::jit;
using namespace rtbot::jit::emit;
namespace fused_op = rtbot::fused_op;

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

static void verify_fn(llvm::Function* fn) {
  std::string err;
  llvm::raw_string_ostream errs(err);
  if (llvm::verifyFunction(*fn, &errs)) {
    errs.flush();
    llvm::report_fatal_error(llvm::StringRef("IR verification failed: " + err));
  }
}

// ---------------------------------------------------------------------------
// Build and JIT a function with signature:
//   double agg_fn(double* state, double v)
// for aggregate emitters that take an input value (CumSum, MaxAgg, MinAgg).
// ---------------------------------------------------------------------------
using AggFnT = double (*)(double*, double);

AggFnT build_agg_fn_with_v(
    const char* fn_name,
    std::function<llvm::Value*(IrEmissionContext&, std::size_t, llvm::Value*)> emitter,
    rtbot::JitContext& jit) {
  auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
  auto mod      = std::make_unique<llvm::Module>(fn_name, *llvm_ctx);

  llvm::Type* f64  = llvm::Type::getDoubleTy(*llvm_ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);

  llvm::FunctionType* fn_ty = llvm::FunctionType::get(f64, {f64p, f64}, false);
  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, fn_name, mod.get());

  auto ai = fn->arg_begin();
  llvm::Argument* arg_state = &*ai++;
  llvm::Argument* arg_v     = &*ai++;

  llvm::BasicBlock* bb_entry = llvm::BasicBlock::Create(*llvm_ctx, "entry", fn);
  llvm::IRBuilder<> b(bb_entry);
  b.setFastMathFlags(llvm::FastMathFlags{});

  IrEmissionContext ec(*llvm_ctx, *mod, b, arg_state);

  llvm::Value* result = emitter(ec, /*state_offset=*/0, arg_v);
  b.CreateRet(result);

  verify_fn(fn);
  jit.compile_module(std::move(mod), std::move(llvm_ctx));
  return jit.lookup<AggFnT>(fn_name);
}

// ---------------------------------------------------------------------------
// Build and JIT a Count function: double count_fn(double* state)
// (no input value needed — count increments on every call).
// ---------------------------------------------------------------------------
using CountFnT = double (*)(double*);

CountFnT build_count_fn(const char* fn_name, rtbot::JitContext& jit) {
  auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
  auto mod      = std::make_unique<llvm::Module>(fn_name, *llvm_ctx);

  llvm::Type* f64  = llvm::Type::getDoubleTy(*llvm_ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);

  llvm::FunctionType* fn_ty = llvm::FunctionType::get(f64, {f64p}, false);
  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, fn_name, mod.get());

  llvm::Argument* arg_state = &*fn->arg_begin();

  llvm::BasicBlock* bb_entry = llvm::BasicBlock::Create(*llvm_ctx, "entry", fn);
  llvm::IRBuilder<> b(bb_entry);
  b.setFastMathFlags(llvm::FastMathFlags{});

  IrEmissionContext ec(*llvm_ctx, *mod, b, arg_state);

  llvm::Value* result = emit_count(ec, /*state_offset=*/0);
  b.CreateRet(result);

  verify_fn(fn);
  jit.compile_module(std::move(mod), std::move(llvm_ctx));
  return jit.lookup<CountFnT>(fn_name);
}

// ---------------------------------------------------------------------------
// Build packed FE program for a stateful unary aggregate: INPUT 0, <op 0>, END.
// State offset is passed as 0.
// ---------------------------------------------------------------------------
std::pair<std::vector<rtbot::fuse::Instruction>, std::vector<double>>
build_agg_fe_program(std::uint8_t fe_opcode) {
  std::vector<double> bc = {
      fused_op::INPUT, 0.0,
      static_cast<double>(fe_opcode), 0.0,  // arg=0: state offset
      fused_op::END,
  };
  auto pack   = rtbot::fuse::pack_bytecode(bc);
  auto layout = rtbot::fuse::compute_state_layout(pack.packed, pack.aux_args);
  // Merge state_init from pack (seed values like -inf/+inf) with layout.
  // pack.state_init already contains the correct seed values for all opcodes.
  return {pack.packed, pack.state_init};
}

// ---------------------------------------------------------------------------
// Build packed FE program for COUNT (no input needed in the opcode, but
// evaluate_one still needs an inputs[] array): INPUT 0, COUNT 0, END.
// The INPUT 0 is consumed by evaluate_one but COUNT ignores the stack.
// ---------------------------------------------------------------------------
std::pair<std::vector<rtbot::fuse::Instruction>, std::vector<double>>
build_count_fe_program() {
  std::vector<double> bc = {
      // COUNT does not pop from the stack, but we push a dummy input so that
      // sp is consistent (evaluate_one asserts nothing, so it is harmless).
      static_cast<double>(fused_op::COUNT), 0.0,  // arg=0: state offset
      fused_op::END,
  };
  auto pack   = rtbot::fuse::pack_bytecode(bc);
  auto layout = rtbot::fuse::compute_state_layout(pack.packed, pack.aux_args);
  return {pack.packed, pack.state_init};
}

// Run one FE tick with a single input value; returns the value written to
// out_ptr[0].
double fe_eval_one(const std::vector<rtbot::fuse::Instruction>& ins,
                   std::vector<double>& state,
                   double input) {
  double out = 0.0;
  double inputs[1] = {input};
  rtbot::fuse::evaluate_one(ins.data(), ins.size(),
                            /*constants=*/nullptr,
                            /*aux_args=*/nullptr,
                            /*coefficients=*/nullptr,
                            inputs, state.data(), &out, 1);
  return out;
}

// Overload for programs that don't need an input (e.g. COUNT-only).
double fe_eval_no_input(const std::vector<rtbot::fuse::Instruction>& ins,
                        std::vector<double>& state) {
  double out = 0.0;
  rtbot::fuse::evaluate_one(ins.data(), ins.size(),
                            /*constants=*/nullptr,
                            /*aux_args=*/nullptr,
                            /*coefficients=*/nullptr,
                            /*inputs=*/nullptr, state.data(), &out, 1);
  return out;
}

}  // namespace

// ===========================================================================
// SCENARIO 1 — CumSum parity
// ===========================================================================
SCENARIO("emit_cumsum matches FE CUMSUM bit-exactly", "[aggregate][cumsum][parity]") {
  constexpr std::size_t N = 200;

  std::mt19937_64 rng(0xC5);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);
  std::vector<double> values(N);
  for (auto& x : values) x = dist(rng);

  // --- FE reference --------------------------------------------------------
  auto [fe_ins, fe_state_init] =
      build_agg_fe_program(static_cast<std::uint8_t>(fused_op::CUMSUM));
  std::vector<double> fe_state = fe_state_init;
  std::vector<double> fe_out;
  for (std::size_t i = 0; i < N; ++i) {
    fe_out.push_back(fe_eval_one(fe_ins, fe_state, values[i]));
  }

  // --- JIT path ------------------------------------------------------------
  static rtbot::JitContext jit_cs;
  auto cs_fn = build_agg_fn_with_v(
      "cs_test",
      [](IrEmissionContext& ec, std::size_t off, llvm::Value* v) {
        return emit_cumsum(ec, off, v);
      },
      jit_cs);
  REQUIRE(cs_fn != nullptr);

  // JIT state: 2 doubles (sum=0, comp=0).
  std::vector<double> jit_state(2, 0.0);
  std::vector<double> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    jit_out.push_back(cs_fn(jit_state.data(), values[i]));
  }

  // --- Parity check --------------------------------------------------------
  REQUIRE(fe_out.size() == jit_out.size());
  for (std::size_t i = 0; i < N; ++i) {
    INFO("i=" << i << " fe=" << fe_out[i] << " jit=" << jit_out[i]);
    REQUIRE(dbits(fe_out[i]) == dbits(jit_out[i]));
  }
}

// ===========================================================================
// SCENARIO 2 — Count parity
// ===========================================================================
SCENARIO("emit_count matches FE COUNT bit-exactly", "[aggregate][count][parity]") {
  constexpr std::size_t N = 200;

  // --- FE reference --------------------------------------------------------
  auto [fe_ins, fe_state_init] = build_count_fe_program();
  std::vector<double> fe_state = fe_state_init;
  std::vector<double> fe_out;
  for (std::size_t i = 0; i < N; ++i) {
    fe_out.push_back(fe_eval_no_input(fe_ins, fe_state));
  }

  // --- JIT path ------------------------------------------------------------
  static rtbot::JitContext jit_cnt;
  auto cnt_fn = build_count_fn("cnt_test", jit_cnt);
  REQUIRE(cnt_fn != nullptr);

  // JIT state: 1 double (count=0).
  std::vector<double> jit_state(1, 0.0);
  std::vector<double> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    jit_out.push_back(cnt_fn(jit_state.data()));
  }

  // --- Parity check --------------------------------------------------------
  REQUIRE(fe_out.size() == jit_out.size());
  for (std::size_t i = 0; i < N; ++i) {
    INFO("i=" << i << " fe=" << fe_out[i] << " jit=" << jit_out[i]);
    REQUIRE(dbits(fe_out[i]) == dbits(jit_out[i]));
    // Sanity: output must equal i+1.
    REQUIRE(jit_out[i] == static_cast<double>(i + 1));
  }
}

// ===========================================================================
// SCENARIO 3 — MaxAgg parity
// ===========================================================================
SCENARIO("emit_max_agg matches FE MAX_AGG bit-exactly", "[aggregate][maxagg][parity]") {
  constexpr std::size_t N = 200;

  // Inputs spanning both negative and positive to exercise the comparison
  // in both directions.
  std::mt19937_64 rng(0xA666);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);
  std::vector<double> values(N);
  for (auto& x : values) x = dist(rng);

  // --- FE reference --------------------------------------------------------
  auto [fe_ins, fe_state_init] =
      build_agg_fe_program(static_cast<std::uint8_t>(fused_op::MAX_AGG));
  std::vector<double> fe_state = fe_state_init;
  std::vector<double> fe_out;
  for (std::size_t i = 0; i < N; ++i) {
    fe_out.push_back(fe_eval_one(fe_ins, fe_state, values[i]));
  }

  // --- JIT path ------------------------------------------------------------
  static rtbot::JitContext jit_max;
  auto max_fn = build_agg_fn_with_v(
      "max_test",
      [](IrEmissionContext& ec, std::size_t off, llvm::Value* v) {
        return emit_max_agg(ec, off, v);
      },
      jit_max);
  REQUIRE(max_fn != nullptr);

  // JIT state: 1 double, seeded to -inf (as state_init_overrides would do at
  // the full JitCompiler level; here we initialise manually).
  std::vector<double> jit_state(1, -std::numeric_limits<double>::infinity());
  std::vector<double> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    jit_out.push_back(max_fn(jit_state.data(), values[i]));
  }

  // --- Parity check --------------------------------------------------------
  REQUIRE(fe_out.size() == jit_out.size());
  for (std::size_t i = 0; i < N; ++i) {
    INFO("i=" << i << " v=" << values[i] << " fe=" << fe_out[i]
              << " jit=" << jit_out[i]);
    REQUIRE(dbits(fe_out[i]) == dbits(jit_out[i]));
  }
}

// ===========================================================================
// SCENARIO 4 — MinAgg parity
// ===========================================================================
SCENARIO("emit_min_agg matches FE MIN_AGG bit-exactly", "[aggregate][minagg][parity]") {
  constexpr std::size_t N = 200;

  // Inputs spanning both negative and positive.
  std::mt19937_64 rng(0xA777);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);
  std::vector<double> values(N);
  for (auto& x : values) x = dist(rng);

  // --- FE reference --------------------------------------------------------
  auto [fe_ins, fe_state_init] =
      build_agg_fe_program(static_cast<std::uint8_t>(fused_op::MIN_AGG));
  std::vector<double> fe_state = fe_state_init;
  std::vector<double> fe_out;
  for (std::size_t i = 0; i < N; ++i) {
    fe_out.push_back(fe_eval_one(fe_ins, fe_state, values[i]));
  }

  // --- JIT path ------------------------------------------------------------
  static rtbot::JitContext jit_min;
  auto min_fn = build_agg_fn_with_v(
      "min_test",
      [](IrEmissionContext& ec, std::size_t off, llvm::Value* v) {
        return emit_min_agg(ec, off, v);
      },
      jit_min);
  REQUIRE(min_fn != nullptr);

  // JIT state: 1 double, seeded to +inf.
  std::vector<double> jit_state(1, std::numeric_limits<double>::infinity());
  std::vector<double> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    jit_out.push_back(min_fn(jit_state.data(), values[i]));
  }

  // --- Parity check --------------------------------------------------------
  REQUIRE(fe_out.size() == jit_out.size());
  for (std::size_t i = 0; i < N; ++i) {
    INFO("i=" << i << " v=" << values[i] << " fe=" << fe_out[i]
              << " jit=" << jit_out[i]);
    REQUIRE(dbits(fe_out[i]) == dbits(jit_out[i]));
  }
}
