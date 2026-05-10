// test_iir_emit.cpp
//
// Parity test: emit_iir vs FE IIR_UPDATE (FusedScalarEval.h case 43).
// Drives 200 random inputs through both paths with a 2-pole low-pass filter
// (B_len=3, A_len=2) and asserts bit-exact match on every emitted tick.

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
#include "rtbot/compiled/jit/emit/IIR.h"
#include "rtbot/fuse/FusedAuxArgs.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"
#include "rtbot/fuse/FusedScalarEval.h"
#include "libs/compiled/jit_spike/JitContext.h"

using namespace rtbot::jit;
using namespace rtbot::jit::emit;

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
// SCENARIO — emit_iir parity vs FE IIR_UPDATE
// ---------------------------------------------------------------------------
SCENARIO("emit_iir matches FE IIR_UPDATE bit-exactly", "[stateful][iir][parity]") {
  // 2-pole low-pass filter coefficients (example Butterworth-like).
  constexpr std::size_t B_len = 3;
  constexpr std::size_t A_len = 2;
  std::vector<double> b_coeffs = {0.0675, 0.1349, 0.0675};
  std::vector<double> a_coeffs = {-1.143, 0.4128};

  std::vector<double> coeffs;
  coeffs.insert(coeffs.end(), b_coeffs.begin(), b_coeffs.end());
  coeffs.insert(coeffs.end(), a_coeffs.begin(), a_coeffs.end());

  constexpr std::size_t N = 200;

  std::mt19937_64 rng(0xF1A001ULL);
  std::uniform_real_distribution<double> dist(-10.0, 10.0);
  std::vector<double> values(N);
  for (auto& x : values) x = dist(rng);

  // --- FE reference ---------------------------------------------------------
  // Bytecode: INPUT 0, IIR_UPDATE(aux_idx=0), END.
  // AuxArgs: {state_off=0, b_len=3, a_len=2, coeff_off=0}
  std::vector<rtbot::fuse::Instruction> packed_bc = {
      {static_cast<std::uint8_t>(rtbot::fused_op::INPUT),      0, 0},
      {static_cast<std::uint8_t>(rtbot::fused_op::IIR_UPDATE), 0, 0},
      {static_cast<std::uint8_t>(rtbot::fused_op::END),         0, 0},
  };
  std::vector<rtbot::fuse::AuxArgs> aux = {
      {0,
       static_cast<std::uint16_t>(B_len),
       static_cast<std::uint16_t>(A_len),
       0}
  };
  // State: x_head + x_count + y_head + y_count + x_ring(B) + y_ring(A)
  //       = 4 + 3 + 2 = 9 doubles, all zero.
  const std::size_t state_size = 4 + B_len + A_len;
  std::vector<double> fe_state(state_size, 0.0);

  std::vector<std::pair<int64_t, double>> fe_out;
  for (std::size_t i = 0; i < N; ++i) {
    double out_v = 0.0;
    const double in_v = values[i];
    bool emitted = rtbot::fuse::evaluate_one(
        packed_bc.data(), packed_bc.size(),
        /*constants=*/nullptr,
        aux.data(),
        coeffs.data(),
        &in_v, fe_state.data(), &out_v, 1);
    if (emitted)
      fe_out.push_back({static_cast<int64_t>(i + 1), out_v});
  }

  // Sanity: FE should emit N - B_len + 1 values after warmup.
  REQUIRE(fe_out.size() == N - B_len + 1);

  // --- JIT path -------------------------------------------------------------
  // Build function: uint8_t iir_test(double* state, int64_t t, double v,
  //                                  int64_t* out_t, double* out_v)
  auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
  auto mod      = std::make_unique<llvm::Module>("iir_test_mod", *llvm_ctx);

  llvm::Type* f64  = llvm::Type::getDoubleTy(*llvm_ctx);
  llvm::Type* i64  = llvm::Type::getInt64Ty(*llvm_ctx);
  llvm::Type* i8   = llvm::Type::getInt8Ty(*llvm_ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);
  llvm::Type* i64p = llvm::PointerType::getUnqual(i64);

  llvm::FunctionType* fn_ty = llvm::FunctionType::get(
      i8, {f64p, i64, f64, i64p, f64p}, false);
  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, "iir_test", mod.get());

  auto ai = fn->arg_begin();
  llvm::Argument* arg_state = &*ai++;
  llvm::Argument* arg_t     = &*ai++;
  llvm::Argument* arg_v     = &*ai++;
  llvm::Argument* arg_out_t = &*ai++;
  llvm::Argument* arg_out_v = &*ai++;

  llvm::BasicBlock* bb_entry = llvm::BasicBlock::Create(*llvm_ctx, "entry", fn);
  llvm::IRBuilder<> ir_builder(bb_entry);
  ir_builder.setFastMathFlags(llvm::FastMathFlags{});

  IrEmissionContext ec(*llvm_ctx, *mod, ir_builder, arg_state);

  StatefulOutput out = emit_iir(ec, /*state_offset=*/0,
                                 B_len, A_len, coeffs,
                                 arg_t, arg_v);

  // Branch on emit_flag to write outputs.
  llvm::Function* cur_fn = ir_builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* bb_store = llvm::BasicBlock::Create(*llvm_ctx, "store",  cur_fn);
  llvm::BasicBlock* bb_ret_f = llvm::BasicBlock::Create(*llvm_ctx, "ret_f",  cur_fn);
  llvm::BasicBlock* bb_ret_t = llvm::BasicBlock::Create(*llvm_ctx, "ret_t",  cur_fn);

  ir_builder.CreateCondBr(out.emit_flag, bb_store, bb_ret_f);

  ir_builder.SetInsertPoint(bb_store);
  ir_builder.CreateStore(out.out_t, arg_out_t);
  ir_builder.CreateStore(out.out_v, arg_out_v);
  ir_builder.CreateBr(bb_ret_t);

  ir_builder.SetInsertPoint(bb_ret_t);
  ir_builder.CreateRet(llvm::ConstantInt::get(i8, 1));

  ir_builder.SetInsertPoint(bb_ret_f);
  ir_builder.CreateRet(llvm::ConstantInt::get(i8, 0));

  verify_fn(fn);

  static rtbot::JitContext jit_iir;
  jit_iir.compile_module(std::move(mod), std::move(llvm_ctx));
  auto iir_fn = jit_iir.lookup<bool (*)(double*, int64_t, double, int64_t*, double*)>("iir_test");
  REQUIRE(iir_fn != nullptr);

  // State: 4 + B_len + A_len doubles, all zero — matches FE initial state.
  std::vector<double> jit_state(state_size, 0.0);
  std::vector<std::pair<int64_t, double>> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    int64_t ot = 0;
    double  ov = 0.0;
    if (iir_fn(jit_state.data(), static_cast<int64_t>(i + 1), values[i], &ot, &ov))
      jit_out.push_back({ot, ov});
  }

  // --- Parity check ---------------------------------------------------------
  REQUIRE(jit_out.size() == fe_out.size());
  REQUIRE(jit_out.size() == N - B_len + 1);

  for (std::size_t i = 0; i < fe_out.size(); ++i) {
    INFO("i=" << i
         << " fe_t="  << fe_out[i].first  << " jit_t=" << jit_out[i].first
         << " fe_v="  << fe_out[i].second << " jit_v=" << jit_out[i].second);
    REQUIRE(fe_out[i].first  == jit_out[i].first);
    REQUIRE(dbits(fe_out[i].second) == dbits(jit_out[i].second));
  }
}
