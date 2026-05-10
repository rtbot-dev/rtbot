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

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "rtbot/compiled/JoinStage.h"
#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/Join.h"
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

// Build a JIT function for Join<2> with signature:
//   int8_t join2_test(double* state, int32_t port, int64_t t, double v,
//                     int64_t* out_t, double* out_v0, double* out_v1)
// The function: push (port, t, v), run try_sync, return sync_flag (as i8),
// and if synced write out_t, out_v0, out_v1.
//
// State layout: N=2, 2*130 = 260 doubles.
auto build_join2_fn(const char* fn_name) {
  auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
  auto mod      = std::make_unique<llvm::Module>(fn_name, *llvm_ctx);

  llvm::Type* f64  = llvm::Type::getDoubleTy(*llvm_ctx);
  llvm::Type* i64  = llvm::Type::getInt64Ty(*llvm_ctx);
  llvm::Type* i32  = llvm::Type::getInt32Ty(*llvm_ctx);
  llvm::Type* i8   = llvm::Type::getInt8Ty(*llvm_ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);
  llvm::Type* i64p = llvm::PointerType::getUnqual(i64);

  // bool join2_test(double* state, int32_t port, int64_t t, double v,
  //                 int64_t* out_t, double* out_v0, double* out_v1)
  llvm::FunctionType* fn_ty = llvm::FunctionType::get(
      i8, {f64p, i32, i64, f64, i64p, f64p, f64p}, false);
  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, fn_name, mod.get());

  auto ai = fn->arg_begin();
  llvm::Argument* arg_state  = &*ai++;
  llvm::Argument* arg_port   = &*ai++;
  llvm::Argument* arg_t      = &*ai++;
  llvm::Argument* arg_v      = &*ai++;
  llvm::Argument* arg_out_t  = &*ai++;
  llvm::Argument* arg_out_v0 = &*ai++;
  llvm::Argument* arg_out_v1 = &*ai++;

  llvm::BasicBlock* bb_entry = llvm::BasicBlock::Create(*llvm_ctx, "entry", fn);
  llvm::IRBuilder<> b(bb_entry);
  b.setFastMathFlags(llvm::FastMathFlags{});

  IrEmissionContext ec(*llvm_ctx, *mod, b, arg_state);

  // We emit push for port 0 and port 1 under a runtime branch.
  // Since N=2 is known at emit time, unroll both port cases.
  constexpr std::size_t N            = 2;
  constexpr std::size_t state_offset = 0;

  llvm::BasicBlock* bb_port0 = llvm::BasicBlock::Create(*llvm_ctx, "push_port0", fn);
  llvm::BasicBlock* bb_port1 = llvm::BasicBlock::Create(*llvm_ctx, "push_port1", fn);
  llvm::BasicBlock* bb_after = llvm::BasicBlock::Create(*llvm_ctx, "push_after",  fn);

  llvm::Value* is_port0 = b.CreateICmpEQ(arg_port, llvm::ConstantInt::get(i32, 0), "is_p0");
  b.CreateCondBr(is_port0, bb_port0, bb_port1);

  b.SetInsertPoint(bb_port0);
  emit_join_push(ec, state_offset, N, 0, arg_t, arg_v);
  b.CreateBr(bb_after);

  b.SetInsertPoint(bb_port1);
  emit_join_push(ec, state_offset, N, 1, arg_t, arg_v);
  b.CreateBr(bb_after);

  b.SetInsertPoint(bb_after);

  // try_sync
  JoinSyncOutput so = emit_join_try_sync(ec, state_offset, N);

  // Branch on sync_flag to write outputs.
  llvm::BasicBlock* bb_store = llvm::BasicBlock::Create(*llvm_ctx, "store", fn);
  llvm::BasicBlock* bb_ret_f = llvm::BasicBlock::Create(*llvm_ctx, "ret_f",  fn);
  llvm::BasicBlock* bb_ret_t = llvm::BasicBlock::Create(*llvm_ctx, "ret_t",  fn);

  b.CreateCondBr(so.sync_flag, bb_store, bb_ret_f);

  b.SetInsertPoint(bb_store);
  b.CreateStore(so.out_t,    arg_out_t);
  b.CreateStore(so.out_vs[0], arg_out_v0);
  b.CreateStore(so.out_vs[1], arg_out_v1);
  b.CreateBr(bb_ret_t);

  b.SetInsertPoint(bb_ret_t);
  b.CreateRet(llvm::ConstantInt::get(i8, 1));

  b.SetInsertPoint(bb_ret_f);
  b.CreateRet(llvm::ConstantInt::get(i8, 0));

  verify_fn(fn);

  static rtbot::JitContext jit;
  jit.compile_module(std::move(mod), std::move(llvm_ctx));

  using Join2Fn = int8_t (*)(double*, int32_t, int64_t, double,
                              int64_t*, double*, double*);
  return jit.lookup<Join2Fn>(fn_name);
}

}  // namespace

// ---------------------------------------------------------------------------
// SCENARIO 1: timestamps align (mirrors test_join_stage.cpp scenario 1)
// ---------------------------------------------------------------------------
SCENARIO("emit_join_push + try_sync match AOT JoinStage<2>: align",
         "[join][parity]") {
  using namespace rtbot::compiled;

  auto join_fn = build_join2_fn("join2_align");
  REQUIRE(join_fn != nullptr);

  constexpr std::size_t N          = 2;
  constexpr std::size_t state_size = N * 130;

  std::vector<double> state(state_size, 0.0);

  JoinStage<2> aot;

  // Each step: (port, t, v)
  struct Step {
    int port;
    int64_t t;
    double v;
  };
  const Step steps[] = {
    {0, 10, 1.0},
    {0, 20, 2.0},
    {1, 20, 99.0},
    {1, 30, 100.0},
    {0, 30, 3.0},
  };

  for (const auto& s : steps) {
    int64_t aot_t = 0;
    std::array<double, 2> aot_vs{};
    bool aot_sync = aot.push(static_cast<std::size_t>(s.port), s.t, s.v,
                              aot_t, aot_vs);

    int64_t jit_t  = 0;
    double  jit_v0 = 0.0;
    double  jit_v1 = 0.0;
    bool jit_sync = static_cast<bool>(
        join_fn(state.data(), static_cast<int32_t>(s.port), s.t, s.v,
                &jit_t, &jit_v0, &jit_v1));

    INFO("port=" << s.port << " t=" << s.t << " v=" << s.v
         << " aot_sync=" << aot_sync << " jit_sync=" << jit_sync);
    REQUIRE(aot_sync == jit_sync);

    if (aot_sync) {
      INFO("aot_t=" << aot_t << " jit_t=" << jit_t);
      REQUIRE(aot_t == jit_t);
      INFO("aot_vs[0]=" << aot_vs[0] << " jit_v0=" << jit_v0);
      REQUIRE(dbits(aot_vs[0]) == dbits(jit_v0));
      INFO("aot_vs[1]=" << aot_vs[1] << " jit_v1=" << jit_v1);
      REQUIRE(dbits(aot_vs[1]) == dbits(jit_v1));
    }
  }
}

// ---------------------------------------------------------------------------
// SCENARIO 2: drops older mismatched fronts (mirrors test_join_stage.cpp scenario 2)
// ---------------------------------------------------------------------------
SCENARIO("emit_join_push + try_sync match AOT JoinStage<2>: drop mismatched",
         "[join][parity]") {
  using namespace rtbot::compiled;

  auto join_fn = build_join2_fn("join2_drop");
  REQUIRE(join_fn != nullptr);

  constexpr std::size_t N          = 2;
  constexpr std::size_t state_size = N * 130;

  std::vector<double> state(state_size, 0.0);

  JoinStage<2> aot;

  struct Step {
    int port;
    int64_t t;
    double v;
  };
  const Step steps[] = {
    {0, 10, 1.0},
    {0, 15, 1.5},
    {0, 20, 2.0},
    {1, 20, 22.0},
  };

  for (const auto& s : steps) {
    int64_t aot_t = 0;
    std::array<double, 2> aot_vs{};
    bool aot_sync = aot.push(static_cast<std::size_t>(s.port), s.t, s.v,
                              aot_t, aot_vs);

    int64_t jit_t  = 0;
    double  jit_v0 = 0.0;
    double  jit_v1 = 0.0;
    bool jit_sync = static_cast<bool>(
        join_fn(state.data(), static_cast<int32_t>(s.port), s.t, s.v,
                &jit_t, &jit_v0, &jit_v1));

    INFO("port=" << s.port << " t=" << s.t << " v=" << s.v
         << " aot_sync=" << aot_sync << " jit_sync=" << jit_sync);
    REQUIRE(aot_sync == jit_sync);

    if (aot_sync) {
      INFO("aot_t=" << aot_t << " jit_t=" << jit_t);
      REQUIRE(aot_t == jit_t);
      INFO("aot_vs[0]=" << aot_vs[0] << " jit_v0=" << jit_v0);
      REQUIRE(dbits(aot_vs[0]) == dbits(jit_v0));
      INFO("aot_vs[1]=" << aot_vs[1] << " jit_v1=" << jit_v1);
      REQUIRE(dbits(aot_vs[1]) == dbits(jit_v1));
    }
  }
}
