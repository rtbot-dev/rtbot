// Aggregate.cpp
//
// IR emission for CumSum, Count, MaxAgg, MinAgg.
//
// All four are always-emit: every call produces a value. No warmup guard,
// no PHI nodes, no branching.
//
// State layouts:
//   CumSum: [state_offset+0] sum, [state_offset+1] Kahan compensation.
//   Count:  [state_offset+0] count.
//   MaxAgg: [state_offset+0] running max. Init to -inf via state_init_overrides.
//   MinAgg: [state_offset+0] running min. Init to +inf via state_init_overrides.

#include "rtbot/compiled/jit/emit/Aggregate.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

// ---------------------------------------------------------------------------
// emit_cumsum — Kahan-compensated running sum.
//
// Mirrors FE case 21:
//   double y = stack[--sp] - state[si + 1];
//   double t = state[si] + y;
//   state[si + 1] = (t - state[si]) - y;
//   state[si] = t;
//   stack[sp++] = state[si];
// ---------------------------------------------------------------------------
llvm::Value* emit_cumsum(IrEmissionContext& ec, std::size_t state_offset,
                          llvm::Value* v) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);

  llvm::Value* sum_ptr  = ec.state_gep(state_offset + 0);
  llvm::Value* comp_ptr = ec.state_gep(state_offset + 1);

  // Delegate to the shared Kahan add helper, then return the updated sum.
  ec.emit_kahan_add(sum_ptr, comp_ptr, v);

  return b.CreateLoad(f64, sum_ptr, "cs_sum");
}

// ---------------------------------------------------------------------------
// emit_count — running counter.
//
// Mirrors FE case 22:
//   state[si] += 1.0;
//   stack[sp++] = state[si];
// ---------------------------------------------------------------------------
llvm::Value* emit_count(IrEmissionContext& ec, std::size_t state_offset) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);

  llvm::Value* cnt_ptr = ec.state_gep(state_offset);

  llvm::Value* old_cnt = b.CreateLoad(f64, cnt_ptr, "cnt_old");
  llvm::Value* new_cnt = b.CreateFAdd(old_cnt,
                                      llvm::ConstantFP::get(f64, 1.0),
                                      "cnt_new");
  b.CreateStore(new_cnt, cnt_ptr);
  return new_cnt;
}

// ---------------------------------------------------------------------------
// emit_max_agg — running maximum.
//
// Mirrors FE case 23:
//   double v = stack[--sp];
//   if (v > state[si]) state[si] = v;
//   stack[sp++] = state[si];
//
// Uses fcmp ogt + select so there is no branch. Slot must be initialised to
// -inf (done via state_init_overrides in StateLayout / JitCompiler).
// ---------------------------------------------------------------------------
llvm::Value* emit_max_agg(IrEmissionContext& ec, std::size_t state_offset,
                           llvm::Value* v) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);

  llvm::Value* slot_ptr = ec.state_gep(state_offset);

  llvm::Value* cur    = b.CreateLoad(f64, slot_ptr, "max_cur");
  llvm::Value* gt     = b.CreateFCmpOGT(v, cur, "max_gt");
  llvm::Value* new_v  = b.CreateSelect(gt, v, cur, "max_new");
  b.CreateStore(new_v, slot_ptr);
  return new_v;
}

// ---------------------------------------------------------------------------
// emit_min_agg — running minimum.
//
// Mirrors FE case 24:
//   double v = stack[--sp];
//   if (v < state[si]) state[si] = v;
//   stack[sp++] = state[si];
//
// Uses fcmp olt + select. Slot must be initialised to +inf via
// state_init_overrides.
// ---------------------------------------------------------------------------
llvm::Value* emit_min_agg(IrEmissionContext& ec, std::size_t state_offset,
                           llvm::Value* v) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);

  llvm::Value* slot_ptr = ec.state_gep(state_offset);

  llvm::Value* cur    = b.CreateLoad(f64, slot_ptr, "min_cur");
  llvm::Value* lt     = b.CreateFCmpOLT(v, cur, "min_lt");
  llvm::Value* new_v  = b.CreateSelect(lt, v, cur, "min_new");
  b.CreateStore(new_v, slot_ptr);
  return new_v;
}

}  // namespace rtbot::jit::emit
