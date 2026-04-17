#ifndef RTBOT_FUSE_SIMD_OPS_H
#define RTBOT_FUSE_SIMD_OPS_H

#include <array>
#include <cstddef>

// xsimd-backed SIMD implementations of the pure binary/unary arithmetic
// opcodes. Kept in a separate header so the batched evaluator can opt into
// them via the RTBOT_FUSED_SIMD compile-time flag without changing the scalar
// code path.
//
// Activation policy: RTBOT_FUSED_SIMD is OFF by default. When set, the
// batched evaluator routes ADD/SUB/MUL/DIV/NEG/ABS inner loops through xsimd
// vectorized operations. Parity is preserved by iterating lanes in the same
// order as the autovectorized C++ loops would — the compiler is likely
// already producing equivalent code for these simple patterns; enabling the
// flag is a controlled experiment and a foundation for FMA peephole work in
// a later task. No transcendental variants here; those require declared ULP
// tolerance (see parity spec section on RTBOT_FUSED_FAST_TRANS).

#include <xsimd/xsimd.hpp>

namespace rtbot::fuse::simd {

using batch_t = xsimd::batch<double>;
inline constexpr std::size_t kSimdLanes = batch_t::size;

// add_lanes: stack[top-1][0..active_lanes) += stack[top][0..active_lanes).
// Caller has already decremented sp; this handles the arithmetic only.
template <std::size_t B>
inline void add_lanes(std::array<double, B>& a, const std::array<double, B>& b,
                       std::size_t active_lanes) {
  std::size_t l = 0;
  if (active_lanes == B && B % kSimdLanes == 0) {
    for (; l + kSimdLanes <= B; l += kSimdLanes) {
      auto va = xsimd::load_unaligned(&a[l]);
      auto vb = xsimd::load_unaligned(&b[l]);
      (va + vb).store_unaligned(&a[l]);
    }
  }
  for (; l < active_lanes; ++l) a[l] += b[l];
}

template <std::size_t B>
inline void sub_lanes(std::array<double, B>& a, const std::array<double, B>& b,
                       std::size_t active_lanes) {
  std::size_t l = 0;
  if (active_lanes == B && B % kSimdLanes == 0) {
    for (; l + kSimdLanes <= B; l += kSimdLanes) {
      auto va = xsimd::load_unaligned(&a[l]);
      auto vb = xsimd::load_unaligned(&b[l]);
      (va - vb).store_unaligned(&a[l]);
    }
  }
  for (; l < active_lanes; ++l) a[l] -= b[l];
}

template <std::size_t B>
inline void mul_lanes(std::array<double, B>& a, const std::array<double, B>& b,
                       std::size_t active_lanes) {
  std::size_t l = 0;
  if (active_lanes == B && B % kSimdLanes == 0) {
    for (; l + kSimdLanes <= B; l += kSimdLanes) {
      auto va = xsimd::load_unaligned(&a[l]);
      auto vb = xsimd::load_unaligned(&b[l]);
      (va * vb).store_unaligned(&a[l]);
    }
  }
  for (; l < active_lanes; ++l) a[l] *= b[l];
}

template <std::size_t B>
inline void div_lanes(std::array<double, B>& a, const std::array<double, B>& b,
                       std::size_t active_lanes) {
  std::size_t l = 0;
  if (active_lanes == B && B % kSimdLanes == 0) {
    for (; l + kSimdLanes <= B; l += kSimdLanes) {
      auto va = xsimd::load_unaligned(&a[l]);
      auto vb = xsimd::load_unaligned(&b[l]);
      (va / vb).store_unaligned(&a[l]);
    }
  }
  for (; l < active_lanes; ++l) a[l] /= b[l];
}

template <std::size_t B>
inline void neg_lanes(std::array<double, B>& a, std::size_t active_lanes) {
  std::size_t l = 0;
  if (active_lanes == B && B % kSimdLanes == 0) {
    for (; l + kSimdLanes <= B; l += kSimdLanes) {
      auto va = xsimd::load_unaligned(&a[l]);
      (-va).store_unaligned(&a[l]);
    }
  }
  for (; l < active_lanes; ++l) a[l] = -a[l];
}

template <std::size_t B>
inline void abs_lanes(std::array<double, B>& a, std::size_t active_lanes) {
  std::size_t l = 0;
  if (active_lanes == B && B % kSimdLanes == 0) {
    for (; l + kSimdLanes <= B; l += kSimdLanes) {
      auto va = xsimd::load_unaligned(&a[l]);
      xsimd::abs(va).store_unaligned(&a[l]);
    }
  }
  for (; l < active_lanes; ++l) a[l] = std::abs(a[l]);
}

}  // namespace rtbot::fuse::simd

#endif  // RTBOT_FUSE_SIMD_OPS_H
