// ReduceJoin.cpp
//
// IR emission for the FE ReduceJoin family (Addition, Subtraction,
// Multiplication, Division, LogicalAnd/Or/Xor/Nand/Nor/Xnor/Implication).
//
// Each variant performs a sequential left-fold over the N synced port
// values, exactly mirroring FE ReduceJoin<T>::process_data:
//   if has_init: acc = init; for i in 0..N-1:  acc = combine(acc, v_i)
//   else:        acc = v_0;  for i in 1..N-1:  acc = combine(acc, v_i)
//
// Division short-circuits: if any divisor v_i (i >= 1 when no init, or
// i >= 0 when init exists) equals 0.0 exactly, no output is emitted.
//
// FastMath flags are NOT set on the IRBuilder, so FAdd/FMul ordering is
// preserved bit-exactly against the FE.

#include "rtbot/compiled/jit/emit/ReduceJoin.h"

#include <stdexcept>

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>

#include "rtbot/compiled/jit/emit/Join.h"

namespace rtbot::jit::emit {

namespace {

llvm::Value* fp(llvm::IRBuilder<>& b, double x) {
  return llvm::ConstantFP::get(b.getDoubleTy(), x);
}

// Convert a double 0.0/1.0 representation to an i1 (true iff != 0.0).
llvm::Value* to_i1(llvm::IRBuilder<>& b, llvm::Value* v) {
  return b.CreateFCmpONE(v, fp(b, 0.0));
}
// Convert i1 to double 1.0/0.0.
llvm::Value* from_i1(llvm::IRBuilder<>& b, llvm::Value* p) {
  return b.CreateSelect(p, fp(b, 1.0), fp(b, 0.0));
}

// Combine accumulator with the next per-port value according to reduce_op.
// Both inputs and the output are doubles. Boolean variants treat any
// nonzero double as true and emit 1.0/0.0.
llvm::Value* emit_combine(llvm::IRBuilder<>& b, ReduceOp op, llvm::Value* acc,
                           llvm::Value* nxt) {
  switch (op) {
    case ReduceOp::AddReduce: return b.CreateFAdd(acc, nxt, "rj_acc");
    case ReduceOp::SubReduce: return b.CreateFSub(acc, nxt, "rj_acc");
    case ReduceOp::MulReduce: return b.CreateFMul(acc, nxt, "rj_acc");
    case ReduceOp::DivReduce: return b.CreateFDiv(acc, nxt, "rj_acc");
    case ReduceOp::AndReduce: {
      llvm::Value* a = to_i1(b, acc);
      llvm::Value* n = to_i1(b, nxt);
      return from_i1(b, b.CreateAnd(a, n));
    }
    case ReduceOp::OrReduce: {
      llvm::Value* a = to_i1(b, acc);
      llvm::Value* n = to_i1(b, nxt);
      return from_i1(b, b.CreateOr(a, n));
    }
    case ReduceOp::XorReduce: {
      llvm::Value* a = to_i1(b, acc);
      llvm::Value* n = to_i1(b, nxt);
      return from_i1(b, b.CreateICmpNE(a, n));
    }
    case ReduceOp::NandReduce: {
      // combine = !(acc && next)
      llvm::Value* a = to_i1(b, acc);
      llvm::Value* n = to_i1(b, nxt);
      llvm::Value* both = b.CreateAnd(a, n);
      return from_i1(b, b.CreateNot(both));
    }
    case ReduceOp::NorReduce: {
      // combine = !(acc || next)
      llvm::Value* a = to_i1(b, acc);
      llvm::Value* n = to_i1(b, nxt);
      llvm::Value* either = b.CreateOr(a, n);
      return from_i1(b, b.CreateNot(either));
    }
    case ReduceOp::XnorReduce: {
      // combine = (acc == next)
      llvm::Value* a = to_i1(b, acc);
      llvm::Value* n = to_i1(b, nxt);
      return from_i1(b, b.CreateICmpEQ(a, n));
    }
    case ReduceOp::ImplReduce: {
      // combine = !acc || next
      llvm::Value* a = to_i1(b, acc);
      llvm::Value* n = to_i1(b, nxt);
      return from_i1(b, b.CreateOr(b.CreateNot(a), n));
    }
  }
  return acc;
}

// Initial value for ops that have one. Returns nullptr when no init (the
// fold seeds with v_0).
llvm::Value* init_value(llvm::IRBuilder<>& b, ReduceOp op) {
  switch (op) {
    case ReduceOp::AddReduce:  return fp(b, 0.0);
    case ReduceOp::MulReduce:  return fp(b, 1.0);
    case ReduceOp::AndReduce:  return fp(b, 1.0);
    case ReduceOp::OrReduce:   return fp(b, 0.0);
    case ReduceOp::NandReduce: return fp(b, 1.0);
    case ReduceOp::NorReduce:  return fp(b, 1.0);
    case ReduceOp::ImplReduce: return fp(b, 1.0);
    case ReduceOp::SubReduce:
    case ReduceOp::DivReduce:
    case ReduceOp::XorReduce:
    case ReduceOp::XnorReduce:
      return nullptr;
  }
  return nullptr;
}

}  // namespace

ReducedSyncOutput emit_reduce_join_try_sync(IrEmissionContext& ec,
                                            std::size_t state_offset,
                                            std::size_t n_ports,
                                            ReduceOp reduce_op) {
  if (n_ports < 2) {
    throw std::runtime_error(
        "emit_reduce_join_try_sync: at least 2 ports required");
  }
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  // Drive the shared N-port sync.
  JoinSyncOutput sync = emit_join_try_sync(ec, state_offset, n_ports);

  // Build the fold straight-line. For DivReduce we additionally OR a
  // div_by_zero flag for each divisor value to suppress the emit.
  llvm::Value* init = init_value(b, reduce_op);

  // div_by_zero accumulator for DivReduce. Starts at false.
  llvm::Value* div_zero =
      llvm::ConstantInt::getFalse(ctx);

  llvm::Value* acc = nullptr;
  std::size_t start = 0;
  if (init) {
    acc = init;
  } else {
    acc = sync.out_vs[0];
    start = 1;
  }

  for (std::size_t i = start; i < n_ports; ++i) {
    llvm::Value* nxt = sync.out_vs[i];
    if (reduce_op == ReduceOp::DivReduce) {
      // FE checks `next.value == 0` exactly; if so combine returns nullopt
      // and the whole emission is skipped. We OR the per-step divisor==0
      // flag to suppress emit at the end.
      llvm::Value* is_zero = b.CreateFCmpOEQ(nxt, fp(b, 0.0), "div_z");
      div_zero = b.CreateOr(div_zero, is_zero, "div_zero_acc");
    }
    acc = emit_combine(b, reduce_op, acc, nxt);
  }

  // emit_flag = sync_flag AND NOT div_zero. For non-Div ops div_zero is
  // constant false so this collapses to sync_flag.
  llvm::Value* emit_flag = sync.sync_flag;
  if (reduce_op == ReduceOp::DivReduce) {
    llvm::Value* not_zero = b.CreateNot(div_zero, "div_ok");
    emit_flag = b.CreateAnd(sync.sync_flag, not_zero, "rj_emit");
  }

  return ReducedSyncOutput{emit_flag, sync.out_t, acc};
}

}  // namespace rtbot::jit::emit
