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
#include <vector>

#include "rtbot/compiled/DiffStage.h"
#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/Diff.h"
#include "libs/compiled/jit_spike/JitContext.h"

using namespace rtbot::jit;
using namespace rtbot::jit::emit;

namespace {

static inline std::uint64_t dbits(double v) {
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

// Build a JIT function with signature:
//   bool diff_test(double* state, int64_t t, double v, int64_t* out_t, double* out_v)
// state_size = 4 doubles. use_oldest_time controls the timestamp selection.
auto build_diff_fn(const char* fn_name, bool use_oldest_time) {
  auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
  auto mod      = std::make_unique<llvm::Module>(fn_name, *llvm_ctx);

  llvm::Type* f64  = llvm::Type::getDoubleTy(*llvm_ctx);
  llvm::Type* i64  = llvm::Type::getInt64Ty(*llvm_ctx);
  llvm::Type* i8   = llvm::Type::getInt8Ty(*llvm_ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);
  llvm::Type* i64p = llvm::PointerType::getUnqual(i64);

  llvm::FunctionType* fn_ty = llvm::FunctionType::get(
      i8, {f64p, i64, f64, i64p, f64p}, false);
  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, fn_name, mod.get());

  auto ai = fn->arg_begin();
  llvm::Argument* arg_state = &*ai++;
  llvm::Argument* arg_t     = &*ai++;
  llvm::Argument* arg_v     = &*ai++;
  llvm::Argument* arg_out_t = &*ai++;
  llvm::Argument* arg_out_v = &*ai++;

  llvm::BasicBlock* bb_entry = llvm::BasicBlock::Create(*llvm_ctx, "entry", fn);
  llvm::IRBuilder<> b(bb_entry);
  b.setFastMathFlags(llvm::FastMathFlags{});

  IrEmissionContext ec(*llvm_ctx, *mod, b, arg_state);

  StatefulOutput out = emit_diff(ec, /*state_offset=*/0, use_oldest_time, arg_t, arg_v);

  // After emit_diff the builder is in df_merge. Branch on emit_flag.
  llvm::Function* cur_fn = b.GetInsertBlock()->getParent();
  llvm::BasicBlock* bb_store = llvm::BasicBlock::Create(*llvm_ctx, "store", cur_fn);
  llvm::BasicBlock* bb_ret_f = llvm::BasicBlock::Create(*llvm_ctx, "ret_f",  cur_fn);
  llvm::BasicBlock* bb_ret_t = llvm::BasicBlock::Create(*llvm_ctx, "ret_t",  cur_fn);

  b.CreateCondBr(out.emit_flag, bb_store, bb_ret_f);

  b.SetInsertPoint(bb_store);
  b.CreateStore(out.out_t, arg_out_t);
  b.CreateStore(out.out_v, arg_out_v);
  b.CreateBr(bb_ret_t);

  b.SetInsertPoint(bb_ret_t);
  b.CreateRet(llvm::ConstantInt::get(i8, 1));

  b.SetInsertPoint(bb_ret_f);
  b.CreateRet(llvm::ConstantInt::get(i8, 0));

  verify_fn(fn);

  static rtbot::JitContext jit_true;
  static rtbot::JitContext jit_false;
  rtbot::JitContext& jit = use_oldest_time ? jit_true : jit_false;
  jit.compile_module(std::move(mod), std::move(llvm_ctx));
  return jit.lookup<bool (*)(double*, int64_t, double, int64_t*, double*)>(fn_name);
}

}  // namespace

// ---------------------------------------------------------------------------
// SCENARIO 1 — use_oldest_time=true (200 samples, seed 0xD1FF)
// ---------------------------------------------------------------------------
SCENARIO("emit_diff matches AOT DiffStage with use_oldest_time=true",
         "[stateful][diff][parity]") {
  constexpr std::size_t N = 200;

  std::mt19937_64 rng(0xD1FF);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);
  std::vector<double> values(N);
  for (auto& x : values) x = dist(rng);

  // --- AOT reference -------------------------------------------------------
  rtbot::compiled::DiffStage aot{true};
  std::vector<std::pair<int64_t, double>> aot_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t t_out = 0;
    double  v_out = 0.0;
    if (aot.process(static_cast<int64_t>(i + 1), values[i], t_out, v_out))
      aot_out.push_back({t_out, v_out});
  }

  // --- JIT path ------------------------------------------------------------
  auto diff_fn = build_diff_fn("diff_test_true", /*use_oldest_time=*/true);
  REQUIRE(diff_fn != nullptr);

  std::vector<double> state(4, 0.0);
  std::vector<std::pair<int64_t, double>> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t ot = 0;
    double  ov = 0.0;
    if (diff_fn(state.data(), static_cast<int64_t>(i + 1), values[i], &ot, &ov))
      jit_out.push_back({ot, ov});
  }

  // --- Parity check --------------------------------------------------------
  REQUIRE(aot_out.size() == jit_out.size());
  for (std::size_t i = 0; i < aot_out.size(); ++i) {
    INFO("i=" << i
         << " aot_t=" << aot_out[i].first  << " jit_t=" << jit_out[i].first
         << " aot_v=" << aot_out[i].second << " jit_v=" << jit_out[i].second);
    REQUIRE(aot_out[i].first == jit_out[i].first);
    REQUIRE(dbits(aot_out[i].second) == dbits(jit_out[i].second));
  }
}

// ---------------------------------------------------------------------------
// SCENARIO 2 — use_oldest_time=false (200 samples, seed 0xD1F0)
// ---------------------------------------------------------------------------
SCENARIO("emit_diff matches AOT DiffStage with use_oldest_time=false",
         "[stateful][diff][parity]") {
  constexpr std::size_t N = 200;

  std::mt19937_64 rng(0xD1F0);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);
  std::vector<double> values(N);
  for (auto& x : values) x = dist(rng);

  // --- AOT reference -------------------------------------------------------
  rtbot::compiled::DiffStage aot{false};
  std::vector<std::pair<int64_t, double>> aot_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t t_out = 0;
    double  v_out = 0.0;
    if (aot.process(static_cast<int64_t>(i + 1), values[i], t_out, v_out))
      aot_out.push_back({t_out, v_out});
  }

  // --- JIT path ------------------------------------------------------------
  auto diff_fn = build_diff_fn("diff_test_false", /*use_oldest_time=*/false);
  REQUIRE(diff_fn != nullptr);

  std::vector<double> state(4, 0.0);
  std::vector<std::pair<int64_t, double>> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t ot = 0;
    double  ov = 0.0;
    if (diff_fn(state.data(), static_cast<int64_t>(i + 1), values[i], &ot, &ov))
      jit_out.push_back({ot, ov});
  }

  // --- Parity check --------------------------------------------------------
  REQUIRE(aot_out.size() == jit_out.size());
  for (std::size_t i = 0; i < aot_out.size(); ++i) {
    INFO("i=" << i
         << " aot_t=" << aot_out[i].first  << " jit_t=" << jit_out[i].first
         << " aot_v=" << aot_out[i].second << " jit_v=" << jit_out[i].second);
    REQUIRE(aot_out[i].first == jit_out[i].first);
    REQUIRE(dbits(aot_out[i].second) == dbits(jit_out[i].second));
  }
}
