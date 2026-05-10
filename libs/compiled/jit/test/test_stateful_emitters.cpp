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

#include "rtbot/compiled/MovingAverageStage.h"
#include "rtbot/compiled/ResamplerHermiteStage.h"
#include "rtbot/compiled/StdDevStage.h"
#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"
#include "rtbot/compiled/jit/emit/Resampler.h"
#include "rtbot/compiled/jit/emit/StdDev.h"
#include "libs/compiled/jit_spike/JitContext.h"

using namespace rtbot::jit;
using namespace rtbot::jit::emit;

static inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

// ---------------------------------------------------------------------------
// Helper: verify IR and report fatal error on failure.
// ---------------------------------------------------------------------------
static void verify_fn(llvm::Function* fn) {
  std::string err;
  llvm::raw_string_ostream errs(err);
  if (llvm::verifyFunction(*fn, &errs)) {
    errs.flush();
    llvm::report_fatal_error(llvm::StringRef("IR verification failed: " + err));
  }
}

// ---------------------------------------------------------------------------
// SCENARIO 1 — MovingAverage parity
// ---------------------------------------------------------------------------
SCENARIO("emit_moving_average matches AOT MovingAverageStage<14> bit-exactly",
         "[stateful][ma][parity]") {
  constexpr std::size_t W = 14;
  constexpr std::size_t N = 500;

  std::mt19937_64 rng(0xCAFE);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);
  std::vector<double> values(N);
  for (auto& x : values) x = dist(rng);

  // --- AOT reference -------------------------------------------------------
  rtbot::compiled::MovingAverageStage<W> aot;
  std::vector<std::pair<int64_t, double>> aot_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t out_t = 0;
    double out_v = 0.0;
    if (aot.process(static_cast<int64_t>(i + 1), values[i], out_t, out_v))
      aot_out.push_back({out_t, out_v});
  }

  // --- JIT path ------------------------------------------------------------
  // Build function: bool ma_test(double* state, int64_t t, double v,
  //                              int64_t* out_t, double* out_v)
  auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
  auto mod      = std::make_unique<llvm::Module>("ma_test_mod", *llvm_ctx);

  llvm::Type* f64  = llvm::Type::getDoubleTy(*llvm_ctx);
  llvm::Type* i64  = llvm::Type::getInt64Ty(*llvm_ctx);
  llvm::Type* i8   = llvm::Type::getInt8Ty(*llvm_ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);
  llvm::Type* i64p = llvm::PointerType::getUnqual(i64);

  llvm::FunctionType* fn_ty = llvm::FunctionType::get(
      i8, {f64p, i64, f64, i64p, f64p}, false);
  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, "ma_test", mod.get());

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

  // state_offset = 0
  StatefulOutput out = emit_moving_average(ec, /*state_offset=*/0, W, arg_t, arg_v);

  // After emit_moving_average, builder is in the merge block.
  // Branch on emit_flag to write outputs.
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

  static rtbot::JitContext jit_ma;
  jit_ma.compile_module(std::move(mod), std::move(llvm_ctx));
  auto ma_fn = jit_ma.lookup<bool (*)(double*, int64_t, double, int64_t*, double*)>("ma_test");
  REQUIRE(ma_fn != nullptr);

  std::vector<double> state(W + 3, 0.0);
  std::vector<std::pair<int64_t, double>> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t ot = 0;
    double ov = 0.0;
    if (ma_fn(state.data(), static_cast<int64_t>(i + 1), values[i], &ot, &ov))
      jit_out.push_back({ot, ov});
  }

  // --- Parity check --------------------------------------------------------
  REQUIRE(aot_out.size() == jit_out.size());
  for (std::size_t i = 0; i < aot_out.size(); ++i) {
    INFO("i=" << i << " aot_t=" << aot_out[i].first << " jit_t=" << jit_out[i].first
              << " aot_v=" << aot_out[i].second << " jit_v=" << jit_out[i].second);
    REQUIRE(aot_out[i].first == jit_out[i].first);
    REQUIRE(dbits(aot_out[i].second) == dbits(jit_out[i].second));
  }
}

// ---------------------------------------------------------------------------
// SCENARIO 2 — StdDev parity
// ---------------------------------------------------------------------------
SCENARIO("emit_stddev matches AOT StdDevStage<14> bit-exactly",
         "[stateful][stddev][parity]") {
  constexpr std::size_t W = 14;
  constexpr std::size_t N = 500;

  std::mt19937_64 rng(0xCAFE);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);
  std::vector<double> values(N);
  for (auto& x : values) x = dist(rng);

  // --- AOT reference -------------------------------------------------------
  rtbot::compiled::StdDevStage<W> aot;
  std::vector<std::pair<int64_t, double>> aot_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t out_t = 0;
    double out_v = 0.0;
    if (aot.process(static_cast<int64_t>(i + 1), values[i], out_t, out_v))
      aot_out.push_back({out_t, out_v});
  }

  // --- JIT path ------------------------------------------------------------
  auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
  auto mod      = std::make_unique<llvm::Module>("sd_test_mod", *llvm_ctx);

  llvm::Type* f64  = llvm::Type::getDoubleTy(*llvm_ctx);
  llvm::Type* i64  = llvm::Type::getInt64Ty(*llvm_ctx);
  llvm::Type* i8   = llvm::Type::getInt8Ty(*llvm_ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);
  llvm::Type* i64p = llvm::PointerType::getUnqual(i64);

  llvm::FunctionType* fn_ty = llvm::FunctionType::get(
      i8, {f64p, i64, f64, i64p, f64p}, false);
  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, "sd_test", mod.get());

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

  StatefulOutput out = emit_stddev(ec, /*state_offset=*/0, W, arg_t, arg_v);

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

  static rtbot::JitContext jit_sd;
  jit_sd.compile_module(std::move(mod), std::move(llvm_ctx));
  auto sd_fn = jit_sd.lookup<bool (*)(double*, int64_t, double, int64_t*, double*)>("sd_test");
  REQUIRE(sd_fn != nullptr);

  std::vector<double> state(W + 3, 0.0);
  std::vector<std::pair<int64_t, double>> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t ot = 0;
    double ov = 0.0;
    if (sd_fn(state.data(), static_cast<int64_t>(i + 1), values[i], &ot, &ov))
      jit_out.push_back({ot, ov});
  }

  // --- Parity check --------------------------------------------------------
  REQUIRE(aot_out.size() == jit_out.size());
  for (std::size_t i = 0; i < aot_out.size(); ++i) {
    INFO("i=" << i << " aot_t=" << aot_out[i].first << " jit_t=" << jit_out[i].first
              << " aot_v=" << aot_out[i].second << " jit_v=" << jit_out[i].second);
    REQUIRE(aot_out[i].first == jit_out[i].first);
    REQUIRE(dbits(aot_out[i].second) == dbits(jit_out[i].second));
  }
}

// ---------------------------------------------------------------------------
// SCENARIO 3 — ResamplerHermite multi-emit parity
// ---------------------------------------------------------------------------
//
// JIT function signature:
//   void resamp_test(double* state, int64_t t, double v,
//                   int64_t* out_ts, double* out_vs, int64_t* out_count)
//
// The IR callback stores each (out_t, out_v) pair into the output arrays at
// [*out_count] and increments *out_count. int64_t is used for the count
// pointer so we can use i64 arithmetic in the IR without needing size_t.
//
// Input: 200 samples with spacing=3 (irregular enough to trigger multi-emit
// at dt=1). This is the same pattern used to stress multi-emit in the spike.
SCENARIO("emit_resampler_hermite matches AOT ResamplerHermiteStage bit-exactly",
         "[stateful][resampler][parity]") {
  constexpr std::size_t N       = 200;
  constexpr std::int64_t DT     = 1;
  constexpr std::int64_t STEP   = 3;  // input spacing — forces multi-emit
  constexpr std::size_t MAX_OUT = N * STEP + 10;

  std::mt19937_64 rng(0xCAFE);
  std::uniform_real_distribution<double> dist(-1e2, 1e2);
  std::vector<double> values(N);
  for (auto& x : values) x = dist(rng);

  // --- AOT reference -------------------------------------------------------
  rtbot::compiled::ResamplerHermiteStage aot(DT);
  std::vector<std::pair<int64_t, double>> aot_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t t_in = static_cast<int64_t>((i + 1) * STEP);
    aot.process(t_in, values[i],
                [&](int64_t et, double ev) { aot_out.push_back({et, ev}); });
  }

  // --- JIT path ------------------------------------------------------------
  // Build: void resamp_test(double* state, int64_t t, double v,
  //                         int64_t* out_ts, double* out_vs, int64_t* out_count)
  auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
  auto mod      = std::make_unique<llvm::Module>("rs_test_mod", *llvm_ctx);

  llvm::Type* f64  = llvm::Type::getDoubleTy(*llvm_ctx);
  llvm::Type* i64  = llvm::Type::getInt64Ty(*llvm_ctx);
  llvm::Type* voidT = llvm::Type::getVoidTy(*llvm_ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);
  llvm::Type* i64p = llvm::PointerType::getUnqual(i64);

  llvm::FunctionType* fn_ty = llvm::FunctionType::get(
      voidT, {f64p, i64, f64, i64p, f64p, i64p}, false);
  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, "resamp_test", mod.get());

  auto ai = fn->arg_begin();
  llvm::Argument* arg_state     = &*ai++;
  llvm::Argument* arg_t         = &*ai++;
  llvm::Argument* arg_v         = &*ai++;
  llvm::Argument* arg_out_ts    = &*ai++;
  llvm::Argument* arg_out_vs    = &*ai++;
  llvm::Argument* arg_out_count = &*ai++;

  llvm::BasicBlock* bb_entry = llvm::BasicBlock::Create(*llvm_ctx, "entry", fn);
  llvm::IRBuilder<> b(bb_entry);
  b.setFastMathFlags(llvm::FastMathFlags{});

  IrEmissionContext ec(*llvm_ctx, *mod, b, arg_state);

  // state_offset = 0, 11 doubles total.
  emit_resampler_hermite(
      ec, /*state_offset=*/0, DT, arg_t, arg_v,
      [&](llvm::Value* out_t, llvm::Value* out_v) {
        // Load current count.
        llvm::Value* cnt = b.CreateLoad(i64, arg_out_count, "rs_cnt_cb");
        // out_ts[cnt] = out_t
        llvm::Value* ts_ptr = b.CreateGEP(i64, arg_out_ts, cnt, "rs_ts_ptr");
        b.CreateStore(out_t, ts_ptr);
        // out_vs[cnt] = out_v
        llvm::Value* vs_ptr = b.CreateGEP(f64, arg_out_vs, cnt, "rs_vs_ptr");
        b.CreateStore(out_v, vs_ptr);
        // ++count
        llvm::Value* cnt1 = b.CreateAdd(cnt, llvm::ConstantInt::get(i64, 1), "rs_cnt1");
        b.CreateStore(cnt1, arg_out_count);
      });

  // After loop_exit, just return void.
  b.CreateRetVoid();

  verify_fn(fn);

  static rtbot::JitContext jit_rs;
  jit_rs.compile_module(std::move(mod), std::move(llvm_ctx));
  using RsFn = void (*)(double*, int64_t, double, int64_t*, double*, int64_t*);
  auto rs_fn = jit_rs.lookup<RsFn>("resamp_test");
  REQUIRE(rs_fn != nullptr);

  // State: 11 doubles (all zero).
  std::vector<double> state(11, 0.0);
  std::vector<int64_t> out_ts(MAX_OUT, 0);
  std::vector<double>  out_vs(MAX_OUT, 0.0);

  std::vector<std::pair<int64_t, double>> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t t_in = static_cast<int64_t>((i + 1) * STEP);
    int64_t cnt = 0;
    rs_fn(state.data(), t_in, values[i], out_ts.data(), out_vs.data(), &cnt);
    for (int64_t k = 0; k < cnt; ++k)
      jit_out.push_back({out_ts[k], out_vs[k]});
  }

  // --- Parity check --------------------------------------------------------
  REQUIRE(aot_out.size() == jit_out.size());
  for (std::size_t i = 0; i < aot_out.size(); ++i) {
    INFO("i=" << i << " aot_t=" << aot_out[i].first << " jit_t=" << jit_out[i].first
              << " aot_v=" << aot_out[i].second << " jit_v=" << jit_out[i].second);
    REQUIRE(aot_out[i].first == jit_out[i].first);
    REQUIRE(dbits(aot_out[i].second) == dbits(jit_out[i].second));
  }
}
