// Phase 3 windowed/stateful opcodes — parity harness against the standalone
// operators in libs/std. Each test feeds the same sequence through both paths
// and asserts bit-exact equality (after any emission-semantics adjustment, see
// the per-scenario comments).

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "rtbot/fuse/FusedAuxArgs.h"
#include "rtbot/fuse/FusedBatchEval.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedExpression.h"
#include "rtbot/fuse/FusedOps.h"
#include "rtbot/fuse/FusedScalarEval.h"
#include "rtbot/fuse/FusedStateLayout.h"
#include "rtbot/std/Difference.h"
#include "rtbot/std/FiniteImpulseResponse.h"
#include "rtbot/std/InfiniteImpulseResponse.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/MovingSum.h"
#include "rtbot/std/StandardDeviation.h"
#include "rtbot/std/WindowMinMax.h"

using namespace rtbot::fuse;
using namespace rtbot::fused_op;

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t b;
  std::memcpy(&b, &v, sizeof(double));
  return b;
}

// Drive the scalar evaluator over a bytecode + aux_args program for a sequence
// of per-message inputs. Skips messages for which the evaluator returns false
// (windowed opcode warmup) so the output stream matches what a standalone
// graph would produce.
std::vector<double> drive_fused_scalar(
    const std::vector<Instruction>& packed,
    const std::vector<double>& constants,
    const std::vector<AuxArgs>& aux_args,
    const std::vector<double>& coefficients,
    const std::vector<std::vector<double>>& inputs_per_message,
    std::vector<double> state,
    std::size_t num_outputs) {
  std::vector<double> out;
  out.reserve(inputs_per_message.size() * num_outputs);
  std::vector<double> scratch(num_outputs);
  for (std::size_t m = 0; m < inputs_per_message.size(); ++m) {
    const bool emit = evaluate_one(
        packed.data(), packed.size(), constants.data(),
        aux_args.empty() ? nullptr : aux_args.data(),
        coefficients.empty() ? nullptr : coefficients.data(),
        inputs_per_message[m].data(), state.data(), scratch.data(),
        num_outputs);
    if (emit) {
      out.insert(out.end(), scratch.begin(), scratch.end());
    }
  }
  return out;
}

// Drive any rtbot::Operator-derived op with a NumberData input port and a
// NumberData output port; returns the output stream in emission order.
template <class Op>
std::vector<double> drive_standalone_scalar(Op& op,
                                              const std::vector<double>& values) {
  std::vector<double> out;
  out.reserve(values.size());
  for (std::size_t i = 0; i < values.size(); ++i) {
    op.receive_data(rtbot::create_message<rtbot::NumberData>(
                         static_cast<std::int64_t>(i + 1),
                         rtbot::NumberData{values[i]}),
                     0);
    op.execute();
    auto& q = op.get_output_queue(0);
    while (out.size() < q.size()) {
      const auto* m = static_cast<const rtbot::Message<rtbot::NumberData>*>(
          q[out.size()].get());
      out.push_back(m->data.value);
    }
  }
  return out;
}

}  // namespace

SCENARIO("MA_UPDATE opcode matches standalone MovingAverage bit-exactly",
         "[windowed][ma]") {
  const std::size_t W = 50;
  const std::size_t N = 500;

  // Bytecode: INPUT 0, MA_UPDATE(aux_idx=0), END.
  std::vector<Instruction> packed = {
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {static_cast<std::uint8_t>(MA_UPDATE), 0, 0},
      {static_cast<std::uint8_t>(END), 0, 0},
  };
  std::vector<AuxArgs> aux = {{0, static_cast<std::uint16_t>(W), 0, 0}};
  auto layout = compute_state_layout(packed, aux);

  std::mt19937_64 rng(0xABCDULL);
  std::uniform_real_distribution<double> dist(-100.0, 100.0);
  std::vector<double> values(N);
  for (auto& v : values) v = dist(rng);

  std::vector<std::vector<double>> inputs_per_message;
  inputs_per_message.reserve(N);
  for (double v : values) inputs_per_message.push_back({v});

  auto fused_out = drive_fused_scalar(packed, {}, aux, {}, inputs_per_message,
                                        layout.initial_values, 1);
  auto ma = rtbot::make_moving_average("ma", W);
  auto standalone_out = drive_standalone_scalar(*ma, values);

  REQUIRE(fused_out.size() == N - W + 1);
  REQUIRE(standalone_out.size() == N - W + 1);
  for (std::size_t i = 0; i < fused_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(fused_out[i]) == dbits(standalone_out[i]));
  }
}

namespace {
// Build a single-window-opcode program: INPUT 0, OP(aux=0), END.
struct WindowedProgram {
  std::vector<Instruction> packed;
  std::vector<AuxArgs> aux;
  std::vector<double> coefficients;
  std::vector<double> state;
  std::size_t num_outputs;
};

WindowedProgram make_unary_windowed_program(std::uint8_t op, AuxArgs a,
                                              std::vector<double> coefficients
                                                  = {}) {
  WindowedProgram p;
  p.packed = {
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {op, 0, 0},
      {static_cast<std::uint8_t>(END), 0, 0},
  };
  p.aux = {a};
  p.coefficients = std::move(coefficients);
  auto layout = compute_state_layout(p.packed, p.aux);
  p.state = std::move(layout.initial_values);
  p.num_outputs = 1;
  return p;
}

std::vector<double> random_values(std::uint64_t seed, std::size_t n,
                                    double lo = -100.0, double hi = 100.0) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(lo, hi);
  std::vector<double> v(n);
  for (auto& x : v) x = dist(rng);
  return v;
}

std::vector<std::vector<double>> as_inputs(const std::vector<double>& values) {
  std::vector<std::vector<double>> out;
  out.reserve(values.size());
  for (double v : values) out.push_back({v});
  return out;
}
}  // namespace

SCENARIO("MSUM_UPDATE opcode matches standalone MovingSum bit-exactly",
         "[windowed][msum]") {
  const std::size_t W = 40;
  const std::size_t N = 400;
  auto values = random_values(0x1CE, N);

  auto p = make_unary_windowed_program(
      static_cast<std::uint8_t>(MSUM_UPDATE),
      {0, static_cast<std::uint16_t>(W), 0, 0});
  auto fused_out = drive_fused_scalar(p.packed, {}, p.aux, p.coefficients,
                                        as_inputs(values), p.state, 1);
  auto op = rtbot::make_moving_sum("msum", W);
  auto standalone_out = drive_standalone_scalar(*op, values);
  REQUIRE(fused_out.size() == N - W + 1);
  REQUIRE(standalone_out.size() == N - W + 1);
  for (std::size_t i = 0; i < fused_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(fused_out[i]) == dbits(standalone_out[i]));
  }
}

SCENARIO("STD_UPDATE opcode matches standalone StandardDeviation bit-exactly",
         "[windowed][std]") {
  const std::size_t W = 30;
  const std::size_t N = 300;
  auto values = random_values(0x2D6, N);

  auto p = make_unary_windowed_program(
      static_cast<std::uint8_t>(STD_UPDATE),
      {0, static_cast<std::uint16_t>(W), 0, 0});
  auto fused_out = drive_fused_scalar(p.packed, {}, p.aux, p.coefficients,
                                        as_inputs(values), p.state, 1);
  rtbot::StandardDeviation op("std", W);
  auto standalone_out = drive_standalone_scalar(op, values);
  REQUIRE(fused_out.size() == N - W + 1);
  REQUIRE(standalone_out.size() == N - W + 1);
  for (std::size_t i = 0; i < fused_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(fused_out[i]) == dbits(standalone_out[i]));
  }
}

SCENARIO("DIFF opcode matches standalone Difference bit-exactly",
         "[windowed][diff]") {
  const std::size_t N = 200;
  auto values = random_values(0xD1FF, N);

  // DIFF uses a 2-field AuxArgs-less layout: Instruction::arg is the state
  // offset directly (2 slots reserved).
  std::vector<Instruction> packed = {
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {static_cast<std::uint8_t>(DIFF), 0, 0},
      {static_cast<std::uint8_t>(END), 0, 0},
  };
  auto layout = compute_state_layout(packed, {});
  auto fused_out = drive_fused_scalar(packed, {}, {}, {}, as_inputs(values),
                                        layout.initial_values, 1);
  rtbot::Difference op("diff");
  auto standalone_out = drive_standalone_scalar(op, values);
  REQUIRE(fused_out.size() == N - 1);
  REQUIRE(standalone_out.size() == N - 1);
  for (std::size_t i = 0; i < fused_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(fused_out[i]) == dbits(standalone_out[i]));
  }
}

SCENARIO("SIGN_CHANGE opcode matches a hand-coded sign-of-delta reference",
         "[windowed][sign_change]") {
  const std::size_t N = 120;
  auto values = random_values(0x51C, N);

  std::vector<Instruction> packed = {
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {static_cast<std::uint8_t>(SIGN_CHANGE), 0, 0},
      {static_cast<std::uint8_t>(END), 0, 0},
  };
  auto layout = compute_state_layout(packed, {});
  auto fused_out = drive_fused_scalar(packed, {}, {}, {}, as_inputs(values),
                                        layout.initial_values, 1);
  // Hand-coded reference: sign(v[i] - v[i-1]) for i >= 1.
  std::vector<double> reference(N - 1);
  for (std::size_t i = 1; i < N; ++i) {
    const double d = values[i] - values[i - 1];
    reference[i - 1] = (d > 0.0) ? 1.0 : (d < 0.0) ? -1.0 : 0.0;
  }
  REQUIRE(fused_out.size() == reference.size());
  for (std::size_t i = 0; i < fused_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(fused_out[i]) == dbits(reference[i]));
  }
}

SCENARIO("WIN_MIN opcode matches standalone WindowMinMax(min) bit-exactly",
         "[windowed][win_min]") {
  const std::size_t W = 16;
  const std::size_t N = 200;
  auto values = random_values(0xABC1, N);

  auto p = make_unary_windowed_program(
      static_cast<std::uint8_t>(WIN_MIN),
      {0, static_cast<std::uint16_t>(W), 0, 0});
  auto fused_out = drive_fused_scalar(p.packed, {}, p.aux, p.coefficients,
                                        as_inputs(values), p.state, 1);
  rtbot::WindowMinMax op("wmin", W, /*is_min=*/true);
  auto standalone_out = drive_standalone_scalar(op, values);
  REQUIRE(fused_out.size() == N - W + 1);
  REQUIRE(standalone_out.size() == N - W + 1);
  for (std::size_t i = 0; i < fused_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(fused_out[i]) == dbits(standalone_out[i]));
  }
}

SCENARIO("WIN_MAX opcode matches standalone WindowMinMax(max) bit-exactly",
         "[windowed][win_max]") {
  const std::size_t W = 16;
  const std::size_t N = 200;
  auto values = random_values(0xABC2, N);

  auto p = make_unary_windowed_program(
      static_cast<std::uint8_t>(WIN_MAX),
      {0, static_cast<std::uint16_t>(W), 0, 0});
  auto fused_out = drive_fused_scalar(p.packed, {}, p.aux, p.coefficients,
                                        as_inputs(values), p.state, 1);
  rtbot::WindowMinMax op("wmax", W, /*is_min=*/false);
  auto standalone_out = drive_standalone_scalar(op, values);
  REQUIRE(fused_out.size() == N - W + 1);
  REQUIRE(standalone_out.size() == N - W + 1);
  for (std::size_t i = 0; i < fused_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(fused_out[i]) == dbits(standalone_out[i]));
  }
}

SCENARIO("FIR_UPDATE opcode matches standalone FIR bit-exactly",
         "[windowed][fir]") {
  const std::vector<double> coeffs = {0.2, 0.1, -0.3, 0.5, 0.15, -0.05, 0.4};
  const std::size_t W = coeffs.size();
  const std::size_t N = 250;
  auto values = random_values(0xF12, N);

  auto p = make_unary_windowed_program(
      static_cast<std::uint8_t>(FIR_UPDATE),
      {0, static_cast<std::uint16_t>(W), 0, 0}, coeffs);
  auto fused_out = drive_fused_scalar(p.packed, {}, p.aux, p.coefficients,
                                        as_inputs(values), p.state, 1);
  rtbot::FiniteImpulseResponse op("fir", coeffs);
  auto standalone_out = drive_standalone_scalar(op, values);
  REQUIRE(fused_out.size() == N - W + 1);
  REQUIRE(standalone_out.size() == N - W + 1);
  for (std::size_t i = 0; i < fused_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(fused_out[i]) == dbits(standalone_out[i]));
  }
}

SCENARIO(
    "FusedExpression with MA_UPDATE matches standalone MovingAverage via ports",
    "[windowed][ma][live_driver]") {
  const std::size_t W = 32;
  const std::size_t N = 300;
  auto values = random_values(0x1A11E, N);

  // Inline-arg form: MA_UPDATE carries W; state and aux_args are derived by
  // the FE's packer.
  std::vector<double> source_bytecode = {
      INPUT, 0, MA_UPDATE, static_cast<double>(W), END};

  auto fe = rtbot::make_fused_expression("fe_ma", /*num_ports=*/1,
                                           /*num_outputs=*/1, source_bytecode,
                                           /*constants=*/{});
  auto ma = rtbot::make_moving_average("ma", W);
  for (std::size_t i = 0; i < N; ++i) {
    fe->receive_data(rtbot::create_message<rtbot::NumberData>(
                          static_cast<std::int64_t>(i + 1),
                          rtbot::NumberData{values[i]}),
                      0);
    fe->execute();
    ma->receive_data(rtbot::create_message<rtbot::NumberData>(
                          static_cast<std::int64_t>(i + 1),
                          rtbot::NumberData{values[i]}),
                      0);
    ma->execute();
  }

  auto& fe_q = fe->get_output_queue(0);
  auto& ma_q = ma->get_output_queue(0);
  REQUIRE(fe_q.size() == N - W + 1);
  REQUIRE(ma_q.size() == N - W + 1);

  for (std::size_t i = 0; i < fe_q.size(); ++i) {
    const auto* fe_m = dynamic_cast<const rtbot::Message<rtbot::VectorNumberData>*>(
        fe_q[i].get());
    const auto* ma_m = dynamic_cast<const rtbot::Message<rtbot::NumberData>*>(
        ma_q[i].get());
    REQUIRE(fe_m != nullptr);
    REQUIRE(ma_m != nullptr);
    REQUIRE(fe_m->time == ma_m->time);
    REQUIRE(fe_m->data.values->size() == 1);
    INFO("i=" << i);
    REQUIRE(dbits((*fe_m->data.values)[0]) == dbits(ma_m->data.value));
  }
}

// Windowed opcodes have complete implementations in evaluate_batched (see
// FusedBatchEval.h). FE/FEV route windowed programs to the scalar fallback
// path because batching them is a net perf regression for realistic
// workloads (they run serially per lane anyway; the batched outer loop
// adds overhead without vectorization wins). This test keeps the batched
// windowed code exercised and bit-exact against the scalar evaluator.
SCENARIO("evaluate_batched MA_UPDATE matches evaluate_one lane-for-lane",
         "[windowed][ma][batched]") {
  const std::size_t W = 24;
  constexpr std::size_t B = kBatch;
  const std::size_t N = 5 * B + 3;  // multiple full batches plus a partial
  auto values = random_values(0xBA1CEDULL, N);

  std::vector<Instruction> packed = {
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {static_cast<std::uint8_t>(MA_UPDATE), 0, 0},
      {static_cast<std::uint8_t>(END), 0, 0},
  };
  std::vector<AuxArgs> aux = {{0, static_cast<std::uint16_t>(W), 0, 0}};
  auto layout = compute_state_layout(packed, aux);

  // Reference: scalar evaluate_one over the full sequence.
  std::vector<double> scalar_state = layout.initial_values;
  std::vector<double> scalar_outputs;
  scalar_outputs.reserve(N);
  {
    double scratch = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
      const double v = values[i];
      const bool emit = evaluate_one(packed.data(), packed.size(),
                                      /*constants=*/nullptr, aux.data(),
                                      /*coefficients=*/nullptr, &v,
                                      scalar_state.data(), &scratch, 1);
      if (emit) scalar_outputs.push_back(scratch);
    }
  }

  // Under test: drive evaluate_batched in cycles of up to B lanes.
  std::vector<double> batched_state = layout.initial_values;
  std::vector<double> batched_outputs;
  batched_outputs.reserve(N);
  std::size_t pos = 0;
  while (pos < N) {
    const std::size_t active = std::min(B, N - pos);
    std::vector<std::array<double, B>> in(1);
    for (std::size_t l = 0; l < active; ++l) in[0][l] = values[pos + l];
    std::vector<double> out(B, 0.0);
    std::array<bool, B> lane_emit;
    lane_emit.fill(true);
    evaluate_batched<B>(packed.data(), packed.size(),
                        /*constants=*/nullptr, aux.data(),
                        /*coefficients=*/nullptr, in.data(), active,
                        batched_state.data(), out.data(), 1, lane_emit);
    for (std::size_t l = 0; l < active; ++l) {
      if (lane_emit[l]) batched_outputs.push_back(out[l]);
    }
    pos += active;
  }

  REQUIRE(batched_outputs.size() == scalar_outputs.size());
  REQUIRE(batched_outputs.size() == N - W + 1);
  for (std::size_t i = 0; i < batched_outputs.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(batched_outputs[i]) == dbits(scalar_outputs[i]));
  }

  // Final state must match too — no lane-to-lane drift.
  REQUIRE(batched_state.size() == scalar_state.size());
  for (std::size_t k = 0; k < batched_state.size(); ++k) {
    INFO("state[" << k << "]");
    REQUIRE(dbits(batched_state[k]) == dbits(scalar_state[k]));
  }
}

SCENARIO("Windowed-opcode state survives collect_bytes + restore mid-stream",
         "[windowed][ma][serialize]") {
  using namespace rtbot;
  const std::size_t W = 20;
  const std::size_t N = 200;
  auto values = random_values(0x5E21A11EULL, N);

  std::vector<double> bytecode = {
      INPUT, 0, MA_UPDATE, static_cast<double>(W), END};

  // Reference: uninterrupted run through one FE instance.
  auto ref_op = make_fused_expression("ref", 1, 1, bytecode, /*constants=*/{});
  std::vector<double> ref_out;
  for (std::size_t i = 0; i < N; ++i) {
    ref_op->receive_data(create_message<NumberData>(
                              static_cast<std::int64_t>(i + 1),
                              NumberData{values[i]}),
                          0);
    ref_op->execute();
  }
  auto& ref_q = ref_op->get_output_queue(0);
  for (std::size_t i = 0; i < ref_q.size(); ++i) {
    const auto* m = dynamic_cast<const Message<VectorNumberData>*>(
        ref_q[i].get());
    ref_out.push_back((*m->data.values)[0]);
  }

  // Under test: feed half the messages through one instance, serialize, then
  // restore into a fresh instance and feed the rest. Output stream must match
  // the uninterrupted reference bit-exactly.
  auto fe_a = make_fused_expression("a", 1, 1, bytecode, /*constants=*/{});
  const std::size_t half = N / 2;
  for (std::size_t i = 0; i < half; ++i) {
    fe_a->receive_data(create_message<NumberData>(
                           static_cast<std::int64_t>(i + 1),
                           NumberData{values[i]}),
                        0);
    fe_a->execute();
  }
  auto bytes = fe_a->collect_bytes();

  auto fe_b = make_fused_expression("a", 1, 1, bytecode, /*constants=*/{});
  auto it = bytes.cbegin();
  fe_b->restore(it);
  for (std::size_t i = half; i < N; ++i) {
    fe_b->receive_data(create_message<NumberData>(
                           static_cast<std::int64_t>(i + 1),
                           NumberData{values[i]}),
                        0);
    fe_b->execute();
  }

  // Operator::collect_bytes serializes the output queue along with state, so
  // fe_b's queue — after restore + continued processing — already holds the
  // full stream (fe_a's pre-serialize emissions + fe_b's post-restore ones).
  std::vector<double> got;
  auto& qb = fe_b->get_output_queue(0);
  for (std::size_t i = 0; i < qb.size(); ++i) {
    const auto* m = dynamic_cast<const Message<VectorNumberData>*>(qb[i].get());
    got.push_back((*m->data.values)[0]);
  }

  REQUIRE(got.size() == ref_out.size());
  for (std::size_t i = 0; i < got.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(got[i]) == dbits(ref_out[i]));
  }
}

SCENARIO("IIR_UPDATE opcode matches standalone IIR bit-exactly",
         "[windowed][iir]") {
  const std::vector<double> b = {0.25, 0.5, 0.25};
  const std::vector<double> a = {-0.3, 0.1};
  const std::size_t N = 200;
  auto values = random_values(0x111A, N);

  // Coefficients laid out as b then a; AuxArgs = {state_off=0, b_len,
  // a_len, coeff_off=0}.
  std::vector<double> coeffs;
  coeffs.insert(coeffs.end(), b.begin(), b.end());
  coeffs.insert(coeffs.end(), a.begin(), a.end());

  auto p = make_unary_windowed_program(
      static_cast<std::uint8_t>(IIR_UPDATE),
      {0, static_cast<std::uint16_t>(b.size()),
       static_cast<std::uint16_t>(a.size()), 0},
      coeffs);

  auto fused_out = drive_fused_scalar(p.packed, {}, p.aux, p.coefficients,
                                        as_inputs(values), p.state, 1);
  rtbot::InfiniteImpulseResponse op("iir", b, a);
  auto standalone_out = drive_standalone_scalar(op, values);
  REQUIRE(fused_out.size() == N - b.size() + 1);
  REQUIRE(standalone_out.size() == N - b.size() + 1);
  for (std::size_t i = 0; i < fused_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(fused_out[i]) == dbits(standalone_out[i]));
  }
}
