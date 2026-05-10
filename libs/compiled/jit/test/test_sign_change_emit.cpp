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

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/SignChange.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"
#include "rtbot/fuse/FusedScalarEval.h"
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

}  // namespace

// ---------------------------------------------------------------------------
// SCENARIO — emit_sign_change parity vs FE SIGN_CHANGE
// ---------------------------------------------------------------------------
SCENARIO("emit_sign_change matches FE SIGN_CHANGE bit-exactly",
         "[stateful][sign_change][parity]") {
  constexpr std::size_t N = 200;

  std::mt19937_64 rng(0x5C1B);
  std::uniform_real_distribution<double> dist(-10.0, 10.0);
  std::vector<double> values(N);
  for (auto& x : values) x = dist(rng);

  // --- FE reference: INPUT 0, SIGN_CHANGE 0, END ---------------------------
  std::vector<double> bc = {
      fused_op::INPUT,       0.0,
      fused_op::SIGN_CHANGE, 0.0,  // state_offset = 0
      fused_op::END,
  };
  auto pack = rtbot::fuse::pack_bytecode(bc);
  std::vector<double> fe_state = pack.state_init;

  std::vector<std::pair<int64_t, double>> fe_out;
  for (std::size_t i = 0; i < N; ++i) {
    double out_v = 0.0;
    double inputs[1] = {values[i]};
    bool emitted = rtbot::fuse::evaluate_one(
        pack.packed.data(), pack.packed.size(),
        /*constants=*/nullptr,
        pack.aux_args.data(),
        /*coefficients=*/nullptr,
        inputs, fe_state.data(), &out_v, 1);
    if (emitted)
      fe_out.push_back({static_cast<int64_t>(i + 1), out_v});
  }

  // --- JIT path ------------------------------------------------------------
  // Build function:
  //   uint8_t sc_test(double* state, int64_t t, double v,
  //                   int64_t* out_t, double* out_v)
  auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
  auto mod      = std::make_unique<llvm::Module>("sc_test_mod", *llvm_ctx);

  llvm::Type* f64  = llvm::Type::getDoubleTy(*llvm_ctx);
  llvm::Type* i64  = llvm::Type::getInt64Ty(*llvm_ctx);
  llvm::Type* i8   = llvm::Type::getInt8Ty(*llvm_ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);
  llvm::Type* i64p = llvm::PointerType::getUnqual(i64);

  llvm::FunctionType* fn_ty = llvm::FunctionType::get(
      i8, {f64p, i64, f64, i64p, f64p}, false);
  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, "sc_test", mod.get());

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

  StatefulOutput out = emit_sign_change(ec, /*state_offset=*/0, arg_t, arg_v);

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

  static rtbot::JitContext jit_sc;
  jit_sc.compile_module(std::move(mod), std::move(llvm_ctx));
  auto sc_fn = jit_sc.lookup<bool (*)(double*, int64_t, double, int64_t*, double*)>("sc_test");
  REQUIRE(sc_fn != nullptr);

  std::vector<double> jit_state(2, 0.0);
  std::vector<std::pair<int64_t, double>> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t ot = 0;
    double  ov = 0.0;
    if (sc_fn(jit_state.data(), static_cast<int64_t>(i + 1), values[i], &ot, &ov))
      jit_out.push_back({ot, ov});
  }

  // --- Parity check --------------------------------------------------------
  // First input suppresses emission; ticks 2-200 each emit. So 199 outputs.
  REQUIRE(fe_out.size() == N - 1);
  REQUIRE(fe_out.size() == jit_out.size());

  for (std::size_t i = 0; i < fe_out.size(); ++i) {
    INFO("i=" << i
         << " fe_t="  << fe_out[i].first  << " jit_t=" << jit_out[i].first
         << " fe_v="  << fe_out[i].second << " jit_v=" << jit_out[i].second);
    REQUIRE(fe_out[i].first  == jit_out[i].first);
    REQUIRE(dbits(fe_out[i].second) == dbits(jit_out[i].second));
  }
}
