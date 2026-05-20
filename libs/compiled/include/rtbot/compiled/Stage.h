#ifndef RTBOT_COMPILED_STAGE_H
#define RTBOT_COMPILED_STAGE_H

// Stage concept (duck-typed; no formal C++ concept declaration to keep the
// library C++17-compatible).
//
// A Stage is a struct with state members and one or more inline process()
// or push() methods. Stages are composed at compile time into a pipeline
// struct that holds them by value.
//
// Stage shapes by purpose:
//
//   Stateful 1->1 (MovingAverage, StandardDeviation, Difference, PeakDetector):
//     bool process(int64_t t, double v,
//                  int64_t& out_t, double& out_v) noexcept;
//     // Returns true if a value should be emitted; out_t is the emit
//     // timestamp (often == t but PeakDetector emits at a delayed time).
//
//   Stateful 1->many (ResamplerHermite):
//     template <class F>
//     void process(int64_t t, double v, F&& emit) noexcept;
//     // emit is invoked 0 or more times: emit(int64_t out_t, double out_v).
//
//   Stateless 1->1 (Scale):
//     double process(double v) const noexcept;
//     // Caller propagates the timestamp; this stage doesn't see it.
//
//   Stateless 2->1 (Addition, Subtraction, Multiplication, Division):
//     double process(double a, double b) const noexcept;
//     // Caller is responsible for upstream alignment of the two inputs.
//
//   Multi-input N->1 with sync (Join<N>):
//     bool push(std::size_t port, int64_t t, double v,
//               int64_t& out_t, std::array<double, N>& out_vs) noexcept;
//     // Caller pushes per port; returns true and fills out_vs when an
//     // N-tuple synchronizes.
//
// Numerical contract: every stateful stage produces output bit-exact to the
// corresponding standalone Operator (or FE bytecode opcode where there's no
// standalone equivalent). The FE interpreter is the canonical semantic
// oracle for parity tests.
//
// Composability invariants:
//   - Stages do not allocate, do not throw, do not observe wall time.
//   - State lives entirely inside the struct's value members (default-init
//     to zero/identity).
//   - No dependency on the rtbot Operator framework. Pipeline composers
//     hold stages by value in a struct and call their methods directly.

#endif  // RTBOT_COMPILED_STAGE_H
