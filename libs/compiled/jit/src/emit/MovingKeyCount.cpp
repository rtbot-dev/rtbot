// MovingKeyCount.cpp
//
// IR emission for one MovingKeyCount<W> step. Mirrors the FE
// MovingKeyCount::process_data semantics: evict the oldest key from the
// ring buffer (if at capacity), insert the new key, then return the count
// of new_key in the current window.
//
// State layout (state_offset + W+3 doubles):
//   [0..W-1]  ring buffer of recent keys
//   [W]       ring_count (double in [0, W])
//   [W+1]     ring_head (double in [0, W-1])
//   [W+2]     ctx_ptr (bit-cast void* to MovingKeyCountNodeCtx)
//
// The ring buffer + accounting live in the JIT's static state buffer; the
// per-key counts hashmap is owned by the runtime ctx. The IR baked the ctx
// pointer into the state slot at JitCompiledProgram construction.

#include "rtbot/compiled/jit/emit/MovingKeyCount.h"

#include <cstdint>

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>

#include "rtbot/compiled/jit/JitCompiledProgram.h"

namespace rtbot::jit::emit {

StatefulOutput emit_moving_key_count(IrEmissionContext& ec,
                                     std::size_t state_offset, std::size_t W,
                                     llvm::Value* t, llvm::Value* v) {
  auto& b = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Type* i8 = llvm::Type::getInt8Ty(ctx);
  llvm::Type* i8p = llvm::PointerType::getUnqual(i8);

  auto cf = [&](double val) -> llvm::Value* {
    return llvm::ConstantFP::get(f64, val);
  };

  // State pointers.
  llvm::Value* ring_base = ec.state_gep(state_offset);
  llvm::Value* count_ptr = ec.state_gep(state_offset + W);
  llvm::Value* head_ptr = ec.state_gep(state_offset + W + 1);
  llvm::Value* ctx_slot_ptr = ec.state_gep(state_offset + W + 2);

  // Load state slots.
  llvm::Value* count_d = b.CreateLoad(f64, count_ptr, "mkc_count_d");
  llvm::Value* head_d = b.CreateLoad(f64, head_ptr, "mkc_head_d");
  llvm::Value* head_u64 = b.CreateFPToUI(head_d, i64, "mkc_head_u64");

  // evict_valid = (count >= W) ? 1.0 : 0.0
  llvm::Value* w_d = cf(static_cast<double>(W));
  llvm::Value* cond_evict = b.CreateFCmpOGE(count_d, w_d, "mkc_evict");
  llvm::Value* evict_valid_d =
      b.CreateSelect(cond_evict, cf(1.0), cf(0.0), "mkc_evict_d");

  // Read evicted_key from ring[head]; if not evicting, the value is unused
  // by the helper but we still load it to keep the IR shape simple.
  llvm::Value* ring_slot = b.CreateGEP(f64, ring_base, head_u64, "mkc_ring_slot");
  llvm::Value* evicted_key = b.CreateLoad(f64, ring_slot, "mkc_evicted");

  // Store the new key into ring[head].
  b.CreateStore(v, ring_slot);

  // head = (head + 1) % W. W is a constant so use modulo on integer.
  llvm::Value* w_u64 = llvm::ConstantInt::get(i64, static_cast<int64_t>(W));
  llvm::Value* head_p1 =
      b.CreateAdd(head_u64, llvm::ConstantInt::get(i64, 1), "mkc_head_p1");
  llvm::Value* head_new_u64 = b.CreateURem(head_p1, w_u64, "mkc_head_new_u64");
  llvm::Value* head_new_d = b.CreateUIToFP(head_new_u64, f64, "mkc_head_new_d");
  b.CreateStore(head_new_d, head_ptr);

  // count = min(count + 1, W).
  llvm::Value* count_p1 = b.CreateFAdd(count_d, cf(1.0), "mkc_count_p1");
  llvm::Value* cond_clamp = b.CreateFCmpOGT(count_p1, w_d, "mkc_count_clamp");
  llvm::Value* count_new = b.CreateSelect(cond_clamp, w_d, count_p1, "mkc_count_new");
  b.CreateStore(count_new, count_ptr);

  // Load ctx pointer (bit-cast double -> i64 -> i8*).
  llvm::Value* ctx_d = b.CreateLoad(f64, ctx_slot_ptr, "mkc_ctx_d");
  llvm::Value* ctx_i64 = b.CreateBitCast(ctx_d, i64, "mkc_ctx_i64");
  llvm::Value* ctx_ptr_v = b.CreateIntToPtr(ctx_i64, i8p, "mkc_ctx_ptr");

  // Bake the helper's address as an i64 constant + IntToPtr to avoid LLJIT
  // symbol resolution. Signature:
  //   double rtbot_jit_mkc_step(void* ctx, double new_key,
  //                              double evicted_key, double evict_valid);
  llvm::FunctionType* helper_ty =
      llvm::FunctionType::get(f64, {i8p, f64, f64, f64}, false);
  void* helper_addr = reinterpret_cast<void*>(&rtbot_jit_mkc_step);
  std::uintptr_t helper_addr_int = reinterpret_cast<std::uintptr_t>(helper_addr);
  llvm::Type* helper_fn_ptr_ty = llvm::PointerType::getUnqual(helper_ty);
  llvm::Value* helper_addr_v = llvm::ConstantInt::get(
      i64, static_cast<std::uint64_t>(helper_addr_int));
  llvm::Value* helper_callee =
      b.CreateIntToPtr(helper_addr_v, helper_fn_ptr_ty, "mkc_helper");
  llvm::Value* helper_args[4] = {ctx_ptr_v, v, evicted_key, evict_valid_d};
  llvm::Value* count_for_key =
      b.CreateCall(helper_ty, helper_callee, helper_args, "mkc_count_for_key");

  // Always-emit op.
  llvm::Value* always_true = llvm::ConstantInt::getTrue(ctx);
  return StatefulOutput{t, count_for_key, always_true};
}

}  // namespace rtbot::jit::emit
