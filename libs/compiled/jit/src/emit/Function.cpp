// Function.cpp
//
// IR emission for one Function (piecewise interpolator) step.
// Mirrors libs/std/include/rtbot/std/Function.h::interpolate.
//
// The point table (xs, ys) and — for Hermite — pre-computed tangents (ms)
// are baked into the IR as private global constant arrays.
//
// Per-tick algorithm:
//   1. Compute bracket index i = sum_{k in 1..N-1}(xs[k] <= x).
//      This matches the FE's `while (i < N-1 && xs[i+1] <= x) i++;` loop:
//      because xs is sorted ascending, the FE's final i equals the count of
//      indices k in [1..N-1] with xs[k] <= x.
//   2. i_used = min(i, N-2). This clamps right-extrapolation onto the last
//      segment, which matches the FE's right-side branch (i == N-1 picks
//      segment (i-1, i) = (N-2, N-1)). For all other i the FE picks segment
//      (i, i+1); after clamping, both behaviours collapse to (i_used,
//      i_used+1). Left-extrapolation (i==0, x < xs[0]) and the interior
//      i==0 case both pick segment (0, 1), which is also covered.
//   3. Load xs[i_used], xs[i_used+1], ys[i_used], ys[i_used+1] (and tangents
//      for Hermite) and emit the interpolation formula in FE FP order.

#include "rtbot/compiled/jit/emit/Function.h"

#include <cstddef>
#include <string>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

namespace {

llvm::GlobalVariable* make_const_array(llvm::Module& mod, llvm::Type* f64,
                                       const std::vector<double>& vals,
                                       const std::string& name) {
  auto* arr_ty = llvm::ArrayType::get(f64, vals.size());
  std::vector<llvm::Constant*> consts;
  consts.reserve(vals.size());
  for (double v : vals) consts.push_back(llvm::ConstantFP::get(f64, v));
  auto* init = llvm::ConstantArray::get(arr_ty, consts);
  return new llvm::GlobalVariable(
      mod, arr_ty, /*isConstant=*/true,
      llvm::GlobalValue::PrivateLinkage, init, name);
}

}  // namespace

llvm::Value* emit_function(IrEmissionContext& ec,
                           const std::vector<std::pair<double, double>>& points,
                           bool use_hermite,
                           const std::vector<double>& tangents,
                           llvm::Value* x) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);

  const std::size_t N = points.size();
  // Function constructor enforces N >= 2; assert that here too.
  if (N < 2) {
    llvm::report_fatal_error("emit_function: points.size() < 2");
  }

  auto cf = [&](double v) -> llvm::Value* {
    return llvm::ConstantFP::get(f64, v);
  };
  auto ci = [&](int64_t v) -> llvm::Value* {
    return llvm::ConstantInt::get(i64, v);
  };

  // -- Bake xs / ys into private global constant arrays ---------------------
  // Use a counter to make global names unique across multiple Function ops in
  // the same module.
  static int counter = 0;
  const std::string suffix = "_" + std::to_string(counter++);

  std::vector<double> xs;
  std::vector<double> ys;
  xs.reserve(N);
  ys.reserve(N);
  for (const auto& p : points) {
    xs.push_back(p.first);
    ys.push_back(p.second);
  }

  auto* xs_global = make_const_array(ec.mod(), f64, xs, "fn_xs" + suffix);
  auto* ys_global = make_const_array(ec.mod(), f64, ys, "fn_ys" + suffix);
  auto* arr_ty   = llvm::ArrayType::get(f64, N);

  llvm::GlobalVariable* ms_global = nullptr;
  if (use_hermite) {
    if (tangents.size() != N) {
      llvm::report_fatal_error(
          "emit_function: tangents size does not match points size");
    }
    ms_global = make_const_array(ec.mod(), f64, tangents, "fn_ms" + suffix);
  }

  // -- Compute bracket index i = sum_{k in 1..N-1}(xs[k] <= x) --------------
  // Uses i64 sum of zero-extended i1 comparisons. xs[k] is loaded as a
  // ConstantFP (the global slot) — equivalent to a load from the global, but
  // emitted as compile-time constants so LLVM can fold the comparison if x
  // turns out to be a constant.
  llvm::Value* i_idx = ci(0);
  for (std::size_t k = 1; k < N; ++k) {
    llvm::Value* cmp =
        b.CreateFCmpOLE(cf(xs[k]), x, "fn_le_" + std::to_string(k));
    llvm::Value* one_or_zero =
        b.CreateZExt(cmp, i64, "fn_zx_" + std::to_string(k));
    i_idx = b.CreateAdd(i_idx, one_or_zero, "fn_iacc_" + std::to_string(k));
  }

  // -- Clamp: i_used = min(i_idx, N-2) --------------------------------------
  llvm::Value* nminus2 = ci(static_cast<int64_t>(N - 2));
  llvm::Value* needs_clamp =
      b.CreateICmpUGT(i_idx, nminus2, "fn_clamp_cond");
  llvm::Value* i_used =
      b.CreateSelect(needs_clamp, nminus2, i_idx, "fn_i_used");

  llvm::Value* i_p1 = b.CreateAdd(i_used, ci(1), "fn_i_p1");

  // -- Load xs[i_used], xs[i_used+1], ys[i_used], ys[i_used+1] --------------
  auto load_at = [&](llvm::GlobalVariable* arr, llvm::Value* idx,
                     const llvm::Twine& name) -> llvm::Value* {
    llvm::Value* gep = b.CreateGEP(arr_ty, arr, {ci(0), idx}, name + "_p");
    return b.CreateLoad(f64, gep, name);
  };

  llvm::Value* x1 = load_at(xs_global, i_used, "fn_x1");
  llvm::Value* x2 = load_at(xs_global, i_p1,   "fn_x2");
  llvm::Value* y1 = load_at(ys_global, i_used, "fn_y1");
  llvm::Value* y2 = load_at(ys_global, i_p1,   "fn_y2");

  if (!use_hermite) {
    // Linear: y1 + (y2 - y1) * (x - x1) / (x2 - x1)
    // FE evaluation order:
    //   a = y2 - y1     (FSub)
    //   b = x  - x1     (FSub)
    //   c = a * b       (FMul)
    //   d = x2 - x1     (FSub)
    //   e = c / d       (FDiv)
    //   result = y1 + e (FAdd)
    llvm::Value* a   = b.CreateFSub(y2, y1, "fn_dy");
    llvm::Value* bv  = b.CreateFSub(x,  x1, "fn_dx");
    llvm::Value* c   = b.CreateFMul(a,  bv, "fn_num");
    llvm::Value* d   = b.CreateFSub(x2, x1, "fn_den");
    llvm::Value* e   = b.CreateFDiv(c,  d,  "fn_ratio");
    return b.CreateFAdd(y1, e, "fn_lin");
  }

  // Hermite: pre-baked tangents.
  llvm::Value* m0 = load_at(ms_global, i_used, "fn_m0");
  llvm::Value* m1 = load_at(ms_global, i_p1,   "fn_m1");

  // mu = (x - x1) / (x2 - x1)
  llvm::Value* mu_num = b.CreateFSub(x,  x1, "fn_h_munum");
  llvm::Value* mu_den = b.CreateFSub(x2, x1, "fn_h_muden");
  llvm::Value* mu     = b.CreateFDiv(mu_num, mu_den, "fn_h_mu");

  // FE order:
  //   mu2 = mu * mu
  //   mu3 = mu2 * mu
  //   h00 = 2*mu3 - 3*mu2 + 1
  //   h10 = mu3 - 2*mu2 + mu
  //   h01 = -2*mu3 + 3*mu2
  //   h11 = mu3 - mu2
  //   y   = h00*y0 + h10*m0 + h01*y1 + h11*m1   (left-to-right)
  llvm::Value* mu2 = b.CreateFMul(mu,  mu,  "fn_h_mu2");
  llvm::Value* mu3 = b.CreateFMul(mu2, mu,  "fn_h_mu3");

  llvm::Value* two_d   = cf(2.0);
  llvm::Value* three_d = cf(3.0);
  llvm::Value* one_d   = cf(1.0);

  // h00 = (2*mu3 - 3*mu2) + 1
  llvm::Value* h00_a  = b.CreateFMul(two_d,   mu3, "fn_h_h00a");
  llvm::Value* h00_b  = b.CreateFMul(three_d, mu2, "fn_h_h00b");
  llvm::Value* h00_c  = b.CreateFSub(h00_a,   h00_b, "fn_h_h00c");
  llvm::Value* h00    = b.CreateFAdd(h00_c,   one_d, "fn_h_h00");

  // h10 = (mu3 - 2*mu2) + mu
  llvm::Value* h10_a  = b.CreateFMul(two_d,  mu2, "fn_h_h10a");
  llvm::Value* h10_b  = b.CreateFSub(mu3,    h10_a, "fn_h_h10b");
  llvm::Value* h10    = b.CreateFAdd(h10_b,  mu,   "fn_h_h10");

  // h01 = (-2)*mu3 + 3*mu2
  // FE source: `-2 * mu3 + 3 * mu2`. -2 is a negative literal, so emit
  // FMul(-2.0, mu3) directly — not FNeg(FMul(2.0, mu3)).
  llvm::Value* neg_two_d = cf(-2.0);
  llvm::Value* h01_a  = b.CreateFMul(neg_two_d, mu3, "fn_h_h01a");
  llvm::Value* h01_b  = b.CreateFMul(three_d,   mu2, "fn_h_h01b");
  llvm::Value* h01    = b.CreateFAdd(h01_a,     h01_b, "fn_h_h01");

  // h11 = mu3 - mu2
  llvm::Value* h11    = b.CreateFSub(mu3, mu2, "fn_h_h11");

  // y = ((h00*y1 + h10*m0) + h01*y2) + h11*m1
  // (FE: hermite_interpolate(y0=y_i, y1=y_{i+1}, m0, m1, mu))
  // Naming: y1 here is ys[i_used] = FE's y0; y2 is ys[i_used+1] = FE's y1.
  llvm::Value* t1 = b.CreateFMul(h00, y1, "fn_h_t1");
  llvm::Value* t2 = b.CreateFMul(h10, m0, "fn_h_t2");
  llvm::Value* s1 = b.CreateFAdd(t1, t2, "fn_h_s1");
  llvm::Value* t3 = b.CreateFMul(h01, y2, "fn_h_t3");
  llvm::Value* s2 = b.CreateFAdd(s1, t3, "fn_h_s2");
  llvm::Value* t4 = b.CreateFMul(h11, m1, "fn_h_t4");
  return b.CreateFAdd(s2, t4, "fn_herm");
}

}  // namespace rtbot::jit::emit
