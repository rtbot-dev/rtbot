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
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/Aggregate.h"
#include "rtbot/compiled/jit/emit/StateLoad.h"
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
// Build and JIT a 2-output function with signature:
//   void sl_test(double* state, double v, double* out0, double* out1)
//
// Emits:
//   out0 = cumsum(state_offset=0, v)         // writes state[0]=sum, state[1]=comp
//   out1 = state_load(state_offset=0) * 2.0  // reads state[0] (the updated sum)
// ---------------------------------------------------------------------------
using SlFnT = void (*)(double*, double, double*, double*);

SlFnT build_sl_fn(const char* fn_name, rtbot::JitContext& jit) {
  auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
  auto mod      = std::make_unique<llvm::Module>(fn_name, *llvm_ctx);

  llvm::Type* f64  = llvm::Type::getDoubleTy(*llvm_ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);
  llvm::Type* voidTy = llvm::Type::getVoidTy(*llvm_ctx);

  llvm::FunctionType* fn_ty =
      llvm::FunctionType::get(voidTy, {f64p, f64, f64p, f64p}, false);
  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, fn_name, mod.get());

  auto ai = fn->arg_begin();
  llvm::Argument* arg_state = &*ai++;
  llvm::Argument* arg_v     = &*ai++;
  llvm::Argument* arg_out0  = &*ai++;
  llvm::Argument* arg_out1  = &*ai++;

  llvm::BasicBlock* bb_entry = llvm::BasicBlock::Create(*llvm_ctx, "entry", fn);
  llvm::IRBuilder<> b(bb_entry);
  b.setFastMathFlags(llvm::FastMathFlags{});

  IrEmissionContext ec(*llvm_ctx, *mod, b, arg_state);

  // Output 0: cumsum writes state[0] (sum) and state[1] (comp), returns sum.
  llvm::Value* cs_val = emit_cumsum(ec, /*state_offset=*/0, arg_v);
  b.CreateStore(cs_val, arg_out0);

  // Output 1: read state[0] (the just-updated sum) and multiply by 2.0.
  llvm::Value* sl_val = emit_state_load(ec, /*state_offset=*/0);
  llvm::Value* two    = llvm::ConstantFP::get(f64, 2.0);
  llvm::Value* out1_v = b.CreateFMul(sl_val, two, "sl_x2");
  b.CreateStore(out1_v, arg_out1);

  b.CreateRetVoid();

  verify_fn(fn);
  jit.compile_module(std::move(mod), std::move(llvm_ctx));
  return jit.lookup<SlFnT>(fn_name);
}

// Deterministic input sequence used by both FE and JIT sides.
inline double input_value(int i) {
  // Simple deterministic sequence: mix a fixed-seed rng offline by position.
  // We pre-generate via a local mt19937_64 seeded once and cached.
  static std::vector<double> cache = []() {
    std::mt19937_64 rng(0x514EB);
    std::uniform_real_distribution<double> dist(-1e3, 1e3);
    std::vector<double> v(200);
    for (auto& x : v) x = dist(rng);
    return v;
  }();
  return cache[static_cast<std::size_t>(i)];
}

}  // namespace

// ===========================================================================
// SCENARIO — parity vs FE bytecode using CUMSUM + STATE_LOAD
// ===========================================================================
SCENARIO("emit_state_load matches FE STATE_LOAD bit-exactly", "[stateful][state_load][parity]") {
  constexpr std::size_t N = 200;

  // -------------------------------------------------------------------------
  // FE side: 2-output program:
  //   output 0: INPUT 0 -> CUMSUM 0 -> END      (writes state[0]=sum, state[1]=comp)
  //   output 1: STATE_LOAD 0 -> CONST 0 -> MUL -> END  (reads state[0], multiplies by 2.0)
  // -------------------------------------------------------------------------
  std::vector<rtbot::fuse::Instruction> packed = {
      // output 0
      {static_cast<uint8_t>(fused_op::INPUT),    0, 0},
      {static_cast<uint8_t>(fused_op::CUMSUM),   0, 0},  // arg=0: state offset
      {static_cast<uint8_t>(fused_op::END),       0, 0},
      // output 1
      {static_cast<uint8_t>(fused_op::STATE_LOAD), 0, 0},  // arg=0: reads state[0]
      {static_cast<uint8_t>(fused_op::CONST),      0, 0},  // arg=0: constants[0] = 2.0
      {static_cast<uint8_t>(fused_op::MUL),        0, 0},
      {static_cast<uint8_t>(fused_op::END),        0, 0},
  };
  std::vector<double> constants = {2.0};

  auto layout = rtbot::fuse::compute_state_layout(packed, {});
  std::vector<double> fe_state = layout.initial_values;
  // CUMSUM requires 2 slots (sum + comp) starting at offset 0.
  if (fe_state.size() < 2) fe_state.resize(2, 0.0);

  std::vector<std::pair<double, double>> fe_out;
  fe_out.reserve(N);
  for (int i = 0; i < static_cast<int>(N); ++i) {
    double scratch[2] = {0.0, 0.0};
    double inputs[1]  = {input_value(i)};
    rtbot::fuse::evaluate_one(
        packed.data(), packed.size(),
        constants.data(),
        /*aux_args=*/nullptr,
        /*coefficients=*/nullptr,
        inputs, fe_state.data(), scratch, 2);
    fe_out.push_back({scratch[0], scratch[1]});
  }

  // -------------------------------------------------------------------------
  // JIT side: build the equivalent IR function and drive it.
  // -------------------------------------------------------------------------
  static rtbot::JitContext jit;
  auto sl_fn = build_sl_fn("sl_test", jit);
  REQUIRE(sl_fn != nullptr);

  // State: 2 doubles (sum=0.0, comp=0.0) matching CUMSUM layout at offset 0.
  std::vector<double> jit_state(2, 0.0);
  std::vector<std::pair<double, double>> jit_out;
  jit_out.reserve(N);
  for (std::size_t i = 0; i < N; ++i) {
    double v0 = 0.0, v1 = 0.0;
    sl_fn(jit_state.data(), input_value(static_cast<int>(i)), &v0, &v1);
    jit_out.push_back({v0, v1});
  }

  // -------------------------------------------------------------------------
  // Parity check: all 200 ticks x 2 outputs = 400 values, all bit-exact.
  // -------------------------------------------------------------------------
  REQUIRE(fe_out.size() == N);
  REQUIRE(jit_out.size() == N);

  for (std::size_t i = 0; i < N; ++i) {
    INFO("tick=" << i
         << " fe0=" << fe_out[i].first  << " jit0=" << jit_out[i].first
         << " fe1=" << fe_out[i].second << " jit1=" << jit_out[i].second);
    REQUIRE(dbits(fe_out[i].first)  == dbits(jit_out[i].first));
    REQUIRE(dbits(fe_out[i].second) == dbits(jit_out[i].second));
  }
}
