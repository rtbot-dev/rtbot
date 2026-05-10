// IrEmissionContext.cpp
//
// Shared IR emission context for per-opcode emitters. Provides helpers for:
//   - state-buffer GEP
//   - Kahan compensated-sum add/subtract
//   - Hermite cubic interpolation (verbatim (1+0.0)*(1-0.0)/2 form)
//
// FP flags: strict IEEE-754, no fast-math — enforced by the constructor
// asserting that the builder has empty FastMathFlags.

#include "rtbot/compiled/jit/IrEmissionContext.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit {

IrEmissionContext::IrEmissionContext(llvm::LLVMContext& ctx, llvm::Module& mod,
                                     llvm::IRBuilder<>& builder,
                                     llvm::Value* state_ptr)
    : ctx_(ctx), mod_(mod), b_(builder), state_ptr_(state_ptr) {}

llvm::Value* IrEmissionContext::state_gep(std::size_t double_offset) {
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx_);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx_);
  return b_.CreateInBoundsGEP(
      f64, state_ptr_,
      llvm::ConstantInt::get(i64, static_cast<int64_t>(double_offset)));
}

void IrEmissionContext::emit_kahan_subtract(llvm::Value* sum_ptr,
                                            llvm::Value* comp_ptr,
                                            llvm::Value* value_to_remove) {
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx_);

  // Load current sum and comp from state.
  llvm::Value* sum  = b_.CreateLoad(f64, sum_ptr,  "ks_sum");
  llvm::Value* comp = b_.CreateLoad(f64, comp_ptr, "ks_comp");

  // ys = (-leaving) - comp
  llvm::Value* neg_leaving = b_.CreateFNeg(value_to_remove, "ks_neg_lv");
  llvm::Value* ys          = b_.CreateFSub(neg_leaving, comp, "ks_ys");
  // ts = sum + ys
  llvm::Value* ts          = b_.CreateFAdd(sum, ys, "ks_ts");
  // comp_new = (ts - sum) - ys
  llvm::Value* ts_minus_sum = b_.CreateFSub(ts, sum, "ks_tss");
  llvm::Value* new_comp     = b_.CreateFSub(ts_minus_sum, ys, "ks_new_comp");

  b_.CreateStore(ts,       sum_ptr);
  b_.CreateStore(new_comp, comp_ptr);
}

void IrEmissionContext::emit_kahan_add(llvm::Value* sum_ptr,
                                       llvm::Value* comp_ptr,
                                       llvm::Value* value_to_add) {
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx_);

  // Load current sum and comp from state.
  llvm::Value* sum  = b_.CreateLoad(f64, sum_ptr,  "ka_sum");
  llvm::Value* comp = b_.CreateLoad(f64, comp_ptr, "ka_comp");

  // ya = v - comp
  llvm::Value* ya          = b_.CreateFSub(value_to_add, comp, "ka_ya");
  // ta = sum + ya
  llvm::Value* ta          = b_.CreateFAdd(sum, ya, "ka_ta");
  // comp_new = (ta - sum) - ya
  llvm::Value* ta_minus_sum = b_.CreateFSub(ta, sum, "ka_tas");
  llvm::Value* new_comp     = b_.CreateFSub(ta_minus_sum, ya, "ka_new_comp");

  b_.CreateStore(ta,       sum_ptr);
  b_.CreateStore(new_comp, comp_ptr);
}

llvm::Value* IrEmissionContext::emit_hermite_interp(llvm::Value* y0,
                                                     llvm::Value* y1,
                                                     llvm::Value* y2,
                                                     llvm::Value* y3,
                                                     llvm::Value* mu) {
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx_);

  auto cf = [&](double v) -> llvm::Value* {
    return llvm::ConstantFP::get(f64, v);
  };

  llvm::Value* zero_d = cf(0.0);
  llvm::Value* one_d  = cf(1.0);

  // Verbatim (1+0.0)*(1-0.0)/2 form — NOT simplified to *0.5.
  llvm::Value* op0  = b_.CreateFAdd(one_d, zero_d, "op0");
  llvm::Value* om0  = b_.CreateFSub(one_d, zero_d, "om0");
  llvm::Value* ch   = b_.CreateFDiv(b_.CreateFMul(op0, om0, "c1"), cf(2.0), "ch");
  llvm::Value* ch2  = b_.CreateFDiv(b_.CreateFMul(om0, om0, "c2"), cf(2.0), "ch2");

  llvm::Value* y1y0 = b_.CreateFSub(y1, y0, "y1y0");
  llvm::Value* y2y1 = b_.CreateFSub(y2, y1, "y2y1");
  llvm::Value* y3y2 = b_.CreateFSub(y3, y2, "y3y2");

  llvm::Value* m0 = b_.CreateFAdd(b_.CreateFMul(y1y0, ch, "m0a"),
                                   b_.CreateFMul(y2y1, ch2, "m0b"), "m0");
  llvm::Value* m1 = b_.CreateFAdd(b_.CreateFMul(y2y1, ch, "m1a"),
                                   b_.CreateFMul(y3y2, ch2, "m1b"), "m1");

  llvm::Value* mu2 = b_.CreateFMul(mu, mu, "mu2");
  llvm::Value* mu3 = b_.CreateFMul(mu2, mu, "mu3");

  // h00 = (2*mu3 - 3*mu2) + 1
  llvm::Value* h00 = b_.CreateFAdd(
      b_.CreateFSub(b_.CreateFMul(cf(2.0), mu3, "h00a"),
                    b_.CreateFMul(cf(3.0), mu2, "h00b"), "h00c"),
      one_d, "h00");
  // h10 = (mu3 - 2*mu2) + mu
  llvm::Value* h10 = b_.CreateFAdd(
      b_.CreateFSub(mu3, b_.CreateFMul(cf(2.0), mu2, "h10b"), "h10c"),
      mu, "h10");
  // h01 = (-2*mu3) + 3*mu2
  llvm::Value* h01 = b_.CreateFAdd(
      b_.CreateFNeg(b_.CreateFMul(cf(2.0), mu3, "h01a"), "h01b"),
      b_.CreateFMul(cf(3.0), mu2, "h01c"), "h01");
  // h11 = mu3 - mu2
  llvm::Value* h11 = b_.CreateFSub(mu3, mu2, "h11");

  // rv = h00*y1 + h10*m0 + h01*y2 + h11*m1
  // Sequential left-to-right grouping matches the C++ AOT:
  //   ((h00*y1 + h10*m0) + h01*y2) + h11*m1
  llvm::Value* s1 = b_.CreateFAdd(b_.CreateFMul(h00, y1, "t1"),
                                   b_.CreateFMul(h10, m0, "t2"), "s1");
  llvm::Value* s2 = b_.CreateFAdd(s1, b_.CreateFMul(h01, y2, "t3"), "s2");
  llvm::Value* rv = b_.CreateFAdd(s2, b_.CreateFMul(h11, m1, "t4"), "rv");

  return rv;
}

}  // namespace rtbot::jit
