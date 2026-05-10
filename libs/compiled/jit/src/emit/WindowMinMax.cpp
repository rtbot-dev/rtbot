// WindowMinMax.cpp
//
// IR emission for one WIN_MIN / WIN_MAX step.  Mirrors the monotonic-deque
// algorithm in FusedScalarEval.h (cases 40/41) exactly, including the O(W)
// shift-left used for front-eviction.
//
// State layout (state_offset + 2+2*W doubles):
//   [0]          pos   — number of messages seen so far (double)
//   [1]          size  — current deque occupancy (double, value in [0,W])
//   [2..W+1]     dq_vals — deque values
//   [W+2..2W+1]  dq_pos  — deque positions

#include "rtbot/compiled/jit/emit/WindowMinMax.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

// ---------------------------------------------------------------------------
// Internal shared helper — parameterised by is_min.
// ---------------------------------------------------------------------------
static StatefulOutput emit_window_min_max(IrEmissionContext& ec,
                                          std::size_t state_offset,
                                          std::size_t W,
                                          llvm::Value* t,
                                          llvm::Value* v,
                                          bool is_min) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Type* i1  = llvm::Type::getInt1Ty(ctx);

  auto ci = [&](int64_t val) -> llvm::Value* {
    return llvm::ConstantInt::get(i64, val);
  };
  auto cf = [&](double val) -> llvm::Value* {
    return llvm::ConstantFP::get(f64, val);
  };

  // Absolute state-slot indices.
  const std::size_t IDX_POS     = state_offset;
  const std::size_t IDX_SIZE    = state_offset + 1;
  const std::size_t IDX_DQ_VALS = state_offset + 2;
  const std::size_t IDX_DQ_POS  = state_offset + 2 + W;

  llvm::Value* pos_ptr      = ec.state_gep(IDX_POS);
  llvm::Value* size_ptr     = ec.state_gep(IDX_SIZE);
  llvm::Value* dq_vals_base = ec.state_gep(IDX_DQ_VALS);
  llvm::Value* dq_pos_base  = ec.state_gep(IDX_DQ_POS);

  llvm::Value* w_d    = cf(static_cast<double>(W));
  llvm::Value* one_d  = cf(1.0);
  llvm::Value* zero_d = cf(0.0);

  // Load pos and size at entry.
  llvm::Value* pos_d  = b.CreateLoad(f64, pos_ptr,  "wmm_pos");
  llvm::Value* size_d = b.CreateLoad(f64, size_ptr, "wmm_size");

  llvm::Function* fn = b.GetInsertBlock()->getParent();

  // Record the block we are currently in — it is the entry predecessor
  // of the pop-back loop header.
  llvm::BasicBlock* bb_before_pb = b.GetInsertBlock();

  // -------------------------------------------------------------------------
  // 1. Pop-back loop:
  //      while (size > 0 && dominated(dq_vals[size-1], v)) size--;
  //
  //  bb_pb_hdr  — loop header / PHI for running size; test size > 0
  //  bb_pb_load — load back element, test dominance
  //  bb_pb_pop  — decrement size, jump back to header
  //  bb_pb_exit — loop exit (size==0 or !dominated)
  // -------------------------------------------------------------------------
  llvm::BasicBlock* bb_pb_hdr  = llvm::BasicBlock::Create(ctx, "wmm_pb_hdr",  fn);
  llvm::BasicBlock* bb_pb_load = llvm::BasicBlock::Create(ctx, "wmm_pb_load", fn);
  llvm::BasicBlock* bb_pb_pop  = llvm::BasicBlock::Create(ctx, "wmm_pb_pop",  fn);
  llvm::BasicBlock* bb_pb_exit = llvm::BasicBlock::Create(ctx, "wmm_pb_exit", fn);

  b.CreateBr(bb_pb_hdr);

  // pb_hdr: PHI for size (two incoming edges added after bb_pb_pop is built).
  b.SetInsertPoint(bb_pb_hdr);
  llvm::PHINode* phi_pb_size = b.CreatePHI(f64, 2, "wmm_pb_size");
  phi_pb_size->addIncoming(size_d, bb_before_pb);
  // back-edge from bb_pb_pop is added below.

  llvm::Value* pb_gt0 = b.CreateFCmpOGT(phi_pb_size, zero_d, "wmm_pb_gt0");
  b.CreateCondBr(pb_gt0, bb_pb_load, bb_pb_exit);

  // pb_load: back_idx = (int)(size - 1); load dq_vals[back_idx]; test dominance.
  b.SetInsertPoint(bb_pb_load);
  llvm::Value* pb_sm1      = b.CreateFSub(phi_pb_size, one_d, "wmm_pb_sm1");
  llvm::Value* pb_back_idx = b.CreateFPToUI(pb_sm1, i64, "wmm_pb_back_idx");
  llvm::Value* pb_back_ptr = b.CreateGEP(f64, dq_vals_base, pb_back_idx, "wmm_pb_back_ptr");
  llvm::Value* pb_back_v   = b.CreateLoad(f64, pb_back_ptr, "wmm_pb_back_v");

  llvm::Value* pb_dominated;
  if (is_min) {
    // MIN: dominated means back_v >= v (new element is <= old, old gets kicked)
    pb_dominated = b.CreateFCmpOGE(pb_back_v, v, "wmm_pb_dom");
  } else {
    // MAX: dominated means back_v <= v (new element is >= old, old gets kicked)
    pb_dominated = b.CreateFCmpOLE(pb_back_v, v, "wmm_pb_dom");
  }
  b.CreateCondBr(pb_dominated, bb_pb_pop, bb_pb_exit);

  // pb_pop: size--; loop back.
  b.SetInsertPoint(bb_pb_pop);
  llvm::Value* pb_size_dec = b.CreateFSub(phi_pb_size, one_d, "wmm_pb_size_dec");
  phi_pb_size->addIncoming(pb_size_dec, bb_pb_pop);
  b.CreateBr(bb_pb_hdr);

  // pb_exit: PHI to forward the last known size.
  // Exits come from pb_hdr (size==0) and pb_load (!dominated).
  b.SetInsertPoint(bb_pb_exit);
  llvm::PHINode* phi_after_pop_size = b.CreatePHI(f64, 2, "wmm_aps");
  phi_after_pop_size->addIncoming(phi_pb_size, bb_pb_hdr);   // exit via size==0 check
  phi_after_pop_size->addIncoming(phi_pb_size, bb_pb_load);  // exit via !dominated

  // -------------------------------------------------------------------------
  // 2. Push: dq_vals[size] = v; dq_pos[size] = pos; size++
  // -------------------------------------------------------------------------
  llvm::Value* push_idx  = b.CreateFPToUI(phi_after_pop_size, i64, "wmm_push_idx");
  llvm::Value* push_vp   = b.CreateGEP(f64, dq_vals_base, push_idx, "wmm_push_vp");
  llvm::Value* push_pp   = b.CreateGEP(f64, dq_pos_base,  push_idx, "wmm_push_pp");
  b.CreateStore(v,     push_vp);
  b.CreateStore(pos_d, push_pp);
  llvm::Value* size_after_push = b.CreateFAdd(phi_after_pop_size, one_d, "wmm_sap");

  llvm::BasicBlock* bb_after_push = b.GetInsertBlock();  // pb_exit continues here

  // -------------------------------------------------------------------------
  // 3. Front-eviction outer loop:
  //      while (size > 0 && dq_pos[0] + W <= pos) { shift_left; size--; }
  //
  //  bb_fe_hdr     — outer header / PHI for fe_size; test size > 0
  //  bb_fe_test    — test dq_pos[0] + W <= pos
  //  bb_fe_sh_hdr  — inner shift header / PHI for k; test k < sz
  //  bb_fe_sh_body — shift: dq_vals[k-1]=dq_vals[k], dq_pos[k-1]=dq_pos[k]
  //  bb_fe_sh_next — k++; back to shift_hdr
  //  bb_fe_pop     — size--; back to fe_hdr
  //  bb_fe_exit    — done
  // -------------------------------------------------------------------------
  llvm::BasicBlock* bb_fe_hdr    = llvm::BasicBlock::Create(ctx, "wmm_fe_hdr",    fn);
  llvm::BasicBlock* bb_fe_test   = llvm::BasicBlock::Create(ctx, "wmm_fe_test",   fn);
  llvm::BasicBlock* bb_fe_sh_hdr = llvm::BasicBlock::Create(ctx, "wmm_fe_sh_hdr", fn);
  llvm::BasicBlock* bb_fe_sh_body= llvm::BasicBlock::Create(ctx, "wmm_fe_sh_body",fn);
  llvm::BasicBlock* bb_fe_sh_next= llvm::BasicBlock::Create(ctx, "wmm_fe_sh_next",fn);
  llvm::BasicBlock* bb_fe_pop    = llvm::BasicBlock::Create(ctx, "wmm_fe_pop",    fn);
  llvm::BasicBlock* bb_fe_exit   = llvm::BasicBlock::Create(ctx, "wmm_fe_exit",   fn);

  b.CreateBr(bb_fe_hdr);

  // fe_hdr: PHI for fe_size; test size > 0.
  b.SetInsertPoint(bb_fe_hdr);
  llvm::PHINode* phi_fe_size = b.CreatePHI(f64, 2, "wmm_fe_size");
  phi_fe_size->addIncoming(size_after_push, bb_after_push);
  // back-edge from bb_fe_pop added below.

  llvm::Value* fe_gt0 = b.CreateFCmpOGT(phi_fe_size, zero_d, "wmm_fe_gt0");
  b.CreateCondBr(fe_gt0, bb_fe_test, bb_fe_exit);

  // fe_test: load dq_pos[0]; check front_pos + W <= pos.
  b.SetInsertPoint(bb_fe_test);
  llvm::Value* fe_fp_ptr    = b.CreateGEP(f64, dq_pos_base, ci(0), "wmm_fe_fp_ptr");
  llvm::Value* fe_fp_d      = b.CreateLoad(f64, fe_fp_ptr, "wmm_fe_fp");
  llvm::Value* fe_evict_thr = b.CreateFAdd(fe_fp_d, w_d, "wmm_fe_evict_thr");
  llvm::Value* should_evict = b.CreateFCmpOLE(fe_evict_thr, pos_d, "wmm_should_evict");
  b.CreateCondBr(should_evict, bb_fe_sh_hdr, bb_fe_exit);

  // fe_sh_hdr: inner shift loop header. k starts at 1; sz = (int)fe_size.
  // PHI node must come before any non-PHI instruction.
  b.SetInsertPoint(bb_fe_sh_hdr);
  llvm::PHINode* phi_sh_k = b.CreatePHI(i64, 2, "wmm_sh_k");
  phi_sh_k->addIncoming(ci(1), bb_fe_test);
  // back-edge from bb_fe_sh_next added below.

  // Now safe to insert non-PHI instructions.
  llvm::Value* fe_sz_i64  = b.CreateFPToUI(phi_fe_size, i64, "wmm_fe_sz");
  llvm::Value* sh_k_lt_sz = b.CreateICmpULT(phi_sh_k, fe_sz_i64, "wmm_sh_k_lt_sz");
  b.CreateCondBr(sh_k_lt_sz, bb_fe_sh_body, bb_fe_pop);

  // fe_sh_body: dq_vals[k-1] = dq_vals[k]; dq_pos[k-1] = dq_pos[k].
  b.SetInsertPoint(bb_fe_sh_body);
  llvm::Value* k_m1      = b.CreateSub(phi_sh_k, ci(1), "wmm_k_m1");
  llvm::Value* sh_sv     = b.CreateLoad(f64, b.CreateGEP(f64, dq_vals_base, phi_sh_k, "wmm_sh_svp"), "wmm_sh_sv");
  llvm::Value* sh_sp     = b.CreateLoad(f64, b.CreateGEP(f64, dq_pos_base,  phi_sh_k, "wmm_sh_spp"), "wmm_sh_sp");
  b.CreateStore(sh_sv, b.CreateGEP(f64, dq_vals_base, k_m1, "wmm_sh_dvp"));
  b.CreateStore(sh_sp, b.CreateGEP(f64, dq_pos_base,  k_m1, "wmm_sh_dpp"));
  b.CreateBr(bb_fe_sh_next);

  // fe_sh_next: k++; back to shift_hdr.
  b.SetInsertPoint(bb_fe_sh_next);
  llvm::Value* k_p1 = b.CreateAdd(phi_sh_k, ci(1), "wmm_k_p1");
  phi_sh_k->addIncoming(k_p1, bb_fe_sh_next);
  b.CreateBr(bb_fe_sh_hdr);

  // fe_pop: size-- after shift; loop back.
  b.SetInsertPoint(bb_fe_pop);
  llvm::Value* fe_size_dec = b.CreateFSub(phi_fe_size, one_d, "wmm_fe_size_dec");
  phi_fe_size->addIncoming(fe_size_dec, bb_fe_pop);
  b.CreateBr(bb_fe_hdr);

  // fe_exit: PHI for final size.
  b.SetInsertPoint(bb_fe_exit);
  llvm::PHINode* phi_final_size = b.CreatePHI(f64, 2, "wmm_final_size");
  phi_final_size->addIncoming(phi_fe_size, bb_fe_hdr);   // exited via size==0
  phi_final_size->addIncoming(phi_fe_size, bb_fe_test);  // exited via !should_evict

  // -------------------------------------------------------------------------
  // 4. Emit check: (pos + 1) >= W → emit dq_vals[0]; else no emit.
  //    Advance pos and store updated size.
  // -------------------------------------------------------------------------
  llvm::Value* pos_new     = b.CreateFAdd(pos_d, one_d, "wmm_pos_new");
  llvm::Value* pos_p1_ge_w = b.CreateFCmpOGE(pos_new, w_d, "wmm_should_emit");

  b.CreateStore(pos_new,        pos_ptr);
  b.CreateStore(phi_final_size, size_ptr);

  llvm::BasicBlock* bb_emit_t = llvm::BasicBlock::Create(ctx, "wmm_emit_t", fn);
  llvm::BasicBlock* bb_emit_f = llvm::BasicBlock::Create(ctx, "wmm_emit_f", fn);
  llvm::BasicBlock* bb_merge  = llvm::BasicBlock::Create(ctx, "wmm_merge",  fn);

  b.CreateCondBr(pos_p1_ge_w, bb_emit_t, bb_emit_f);

  b.SetInsertPoint(bb_emit_t);
  llvm::Value* fv_ptr = b.CreateGEP(f64, dq_vals_base, ci(0), "wmm_fv_ptr");
  llvm::Value* fv     = b.CreateLoad(f64, fv_ptr, "wmm_fv");
  b.CreateBr(bb_merge);

  b.SetInsertPoint(bb_emit_f);
  b.CreateBr(bb_merge);

  b.SetInsertPoint(bb_merge);
  llvm::PHINode* phi_out_v = b.CreatePHI(f64, 2, "wmm_phi_v");
  phi_out_v->addIncoming(fv,     bb_emit_t);
  phi_out_v->addIncoming(zero_d, bb_emit_f);

  llvm::PHINode* phi_flag = b.CreatePHI(i1, 2, "wmm_phi_flag");
  phi_flag->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_emit_t);
  phi_flag->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_emit_f);

  return StatefulOutput{t, phi_out_v, phi_flag};
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

StatefulOutput emit_win_min(IrEmissionContext& ec, std::size_t state_offset,
                            std::size_t W,
                            llvm::Value* t, llvm::Value* v) {
  return emit_window_min_max(ec, state_offset, W, t, v, /*is_min=*/true);
}

StatefulOutput emit_win_max(IrEmissionContext& ec, std::size_t state_offset,
                            std::size_t W,
                            llvm::Value* t, llvm::Value* v) {
  return emit_window_min_max(ec, state_offset, W, t, v, /*is_min=*/false);
}

}  // namespace rtbot::jit::emit
