#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "fused_parity/fuzz_bytecode.h"
#include "fused_parity/reference_eval.h"
#include "rtbot/fuse/FusedBatchEval.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedExpression.h"
#include "rtbot/fuse/FusedExpressionVector.h"

using namespace rtbot::fuse;
using namespace rtbot::fused_op;
using rtbot::fused_parity::evaluate_scalar;
using rtbot::fused_parity::FuzzProgram;

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t b;
  std::memcpy(&b, &v, sizeof(double));
  return b;
}

}  // namespace

SCENARIO("Batch width is platform-appropriate", "[batched][width]") {
  REQUIRE(kBatch >= 4);
  REQUIRE(kBatch <= 8);
}

SCENARIO("Batched eval matches scalar reference on a full batch",
         "[batched][parity]") {
  constexpr std::size_t B = kBatch;

  std::vector<double> legacy = {INPUT, 0, INPUT, 1, ADD, CONST, 0, MUL, END};
  auto packed = encode_legacy(legacy);
  std::vector<double> consts = {1.5};

  std::vector<std::array<double, B>> batched_inputs(2);
  std::vector<std::vector<double>> ref_inputs(B, std::vector<double>(2));
  for (std::size_t l = 0; l < B; ++l) {
    batched_inputs[0][l] = 1.0 + static_cast<double>(l);
    batched_inputs[1][l] = 2.0 + static_cast<double>(l);
    ref_inputs[l] = {batched_inputs[0][l], batched_inputs[1][l]};
  }

  std::vector<double> state;
  std::vector<double> out(B * 1);
  evaluate_batched<B>(packed.data(), packed.size(), consts.data(),
                      batched_inputs.data(), B, state.data(), out.data(), 1);

  auto ref = evaluate_scalar(legacy, consts, ref_inputs, {}, 1);
  for (std::size_t l = 0; l < B; ++l) {
    REQUIRE(dbits(out[l]) == dbits(ref.outputs[l]));
  }
}

SCENARIO("Batched eval handles partial batches bit-exactly",
         "[batched][parity]") {
  constexpr std::size_t B = kBatch;

  std::vector<double> legacy = {INPUT, 0, INPUT, 1, SUB, END};
  auto packed = encode_legacy(legacy);

  std::vector<std::array<double, B>> batched_inputs(2);
  std::vector<std::vector<double>> ref_inputs;
  for (std::size_t l = 0; l < 3; ++l) {
    batched_inputs[0][l] = 10.0 + static_cast<double>(l);
    batched_inputs[1][l] = 1.0 + static_cast<double>(l);
    ref_inputs.push_back({batched_inputs[0][l], batched_inputs[1][l]});
  }

  std::vector<double> state;
  std::vector<double> out(B * 1, 0.0);
  evaluate_batched<B>(packed.data(), packed.size(), /*constants=*/nullptr,
                      batched_inputs.data(), /*active_lanes=*/3, state.data(),
                      out.data(), 1);

  auto ref = evaluate_scalar(legacy, {}, ref_inputs, {}, 1);
  for (std::size_t l = 0; l < 3; ++l) {
    REQUIRE(dbits(out[l]) == dbits(ref.outputs[l]));
  }
}

SCENARIO(
    "Batched eval over random stateless programs matches the reference",
    "[batched][fuzz]") {
  constexpr std::size_t B = kBatch;
  const int kTrials = 500;

  for (int t = 0; t < kTrials; ++t) {
    const std::uint64_t seed = 1'000 + static_cast<std::uint64_t>(t);
    auto prog = rtbot::fused_parity::generate_program(
        seed, /*max_inputs=*/6, /*max_outputs=*/3,
        /*include_transcendentals=*/true, /*include_stateful=*/false);
    auto packed = encode_legacy(prog.bytecode);

    for (std::size_t active = 1; active <= B; ++active) {
      auto ref_inputs = rtbot::fused_parity::generate_input_sequence(
          seed ^ 0xABCDULL, prog.num_inputs, active);

      std::vector<std::array<double, B>> batched_inputs(prog.num_inputs);
      for (std::size_t l = 0; l < active; ++l)
        for (std::size_t p = 0; p < prog.num_inputs; ++p)
          batched_inputs[p][l] = ref_inputs[l][p];

      std::vector<double> state = prog.state_init;
      std::vector<double> out(B * prog.num_outputs, 0.0);
      evaluate_batched<B>(packed.data(), packed.size(), prog.constants.data(),
                          batched_inputs.data(), active, state.data(),
                          out.data(), prog.num_outputs);

      auto ref = evaluate_scalar(prog.bytecode, prog.constants, ref_inputs,
                                  prog.state_init, prog.num_outputs);
      for (std::size_t l = 0; l < active; ++l) {
        for (std::size_t k = 0; k < prog.num_outputs; ++k) {
          INFO("seed=" << seed << " active=" << active << " l=" << l
                       << " k=" << k);
          REQUIRE(dbits(out[l * prog.num_outputs + k]) ==
                  dbits(ref.outputs[l * prog.num_outputs + k]));
        }
      }
    }
  }
}

SCENARIO(
    "Stateful opcodes preserve bit-exactness across multiple batch cycles",
    "[batched][state]") {
  constexpr std::size_t B = kBatch;
  std::vector<double> legacy = {INPUT, 0, CUMSUM, 0, END};
  auto packed = encode_legacy(legacy);

  const std::size_t kMsgs = 4 * B + 3;  // partial final batch
  std::mt19937_64 rng(0xC0DEULL);
  std::uniform_real_distribution<double> dist(-100.0, 100.0);
  std::vector<std::vector<double>> msgs(kMsgs, std::vector<double>(1));
  for (auto& m : msgs) m[0] = dist(rng);

  auto ref = evaluate_scalar(legacy, {}, msgs, /*state_init=*/{0.0, 0.0}, 1);

  std::vector<double> state = {0.0, 0.0};
  std::vector<double> all_outputs;
  std::size_t pos = 0;
  while (pos < kMsgs) {
    const std::size_t active = std::min(B, kMsgs - pos);
    std::vector<std::array<double, B>> batched(1);
    for (std::size_t l = 0; l < active; ++l) batched[0][l] = msgs[pos + l][0];
    std::vector<double> out(B, 0.0);
    evaluate_batched<B>(packed.data(), packed.size(), nullptr, batched.data(),
                        active, state.data(), out.data(), 1);
    for (std::size_t l = 0; l < active; ++l) all_outputs.push_back(out[l]);
    pos += active;
  }

  REQUIRE(all_outputs.size() == ref.outputs.size());
  for (std::size_t i = 0; i < all_outputs.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(all_outputs[i]) == dbits(ref.outputs[i]));
  }
}

SCENARIO(
    "FusedExpression emits correct partial-batch output through port path",
    "[batched][partial_batch]") {
  using namespace rtbot;
  auto op = make_fused_expression("fe", 2, 1,
                                    std::vector<double>{INPUT, 0, INPUT, 1,
                                                         ADD, END},
                                    std::vector<double>{});
  // 3 messages — fewer than kBatch (either 4 or 8), so this exercises the
  // partial-final-batch code path. Per tick t: port0=10*t, port1=20*t, so
  // the fused ADD output is 30*t.
  for (std::int64_t t = 1; t <= 3; ++t) {
    op->receive_data(
        create_message<NumberData>(t, NumberData{10.0 * static_cast<double>(t)}),
        0);
    op->receive_data(
        create_message<NumberData>(t, NumberData{20.0 * static_cast<double>(t)}),
        1);
  }
  op->execute();

  auto& q = op->get_output_queue(0);
  REQUIRE(q.size() == 3);
  for (std::size_t i = 0; i < 3; ++i) {
    const auto* m = dynamic_cast<const Message<VectorNumberData>*>(q[i].get());
    REQUIRE(m != nullptr);
    REQUIRE(m->time == static_cast<std::int64_t>(i + 1));
    REQUIRE(m->data.values->size() == 1);
    REQUIRE((*m->data.values)[0] == Approx(30.0 * (i + 1)));
  }
}

// Regression guard: STATE_LOAD reads shared state, and naive batched
// evaluation would contaminate it across lanes. This test queues many
// messages BEFORE execute() so process_data drains batches of up to B
// through the CUMSUM + COUNT + STATE_LOAD (AVG) pattern. Output must match
// per-message scalar evaluation.
SCENARIO(
    "FusedExpression with STATE_LOAD matches scalar under a backlog",
    "[batched][state_load]") {
  using namespace rtbot;
  // Output 0: CUMSUM(x). Output 1: COUNT. Output 2: CUMSUM/COUNT (running avg).
  std::vector<double> bc = {INPUT,      0, CUMSUM,     0, END,
                             COUNT,      2, END,
                             STATE_LOAD, 0, STATE_LOAD, 2, DIV, END};
  std::vector<double> state_init = {0.0, 0.0, 0.0};

  const std::size_t N = 100;
  std::mt19937_64 rng(0xA5107ULL);
  std::uniform_real_distribution<double> d(-10.0, 10.0);
  std::vector<double> values(N);
  for (auto& v : values) v = d(rng);

  // Reference: per-message scalar path via evaluate_scalar.
  std::vector<std::vector<double>> ref_inputs;
  for (double v : values) ref_inputs.push_back({v});
  auto ref = evaluate_scalar(bc, {}, ref_inputs, state_init, 3);

  // FE: queue all messages, then execute once. process_data drains in
  // batches of up to B.
  auto op = make_fused_expression("fe_sl", 1, 3, bc, std::vector<double>{},
                                    state_init);
  for (std::size_t i = 0; i < N; ++i) {
    op->receive_data(
        create_message<NumberData>(static_cast<std::int64_t>(i + 1),
                                    NumberData{values[i]}),
        0);
  }
  op->execute();

  auto& q = op->get_output_queue(0);
  REQUIRE(q.size() == N);
  for (std::size_t i = 0; i < N; ++i) {
    const auto* m = dynamic_cast<const Message<VectorNumberData>*>(q[i].get());
    REQUIRE(m != nullptr);
    for (std::size_t k = 0; k < 3; ++k) {
      INFO("i=" << i << " k=" << k);
      REQUIRE(dbits((*m->data.values)[k]) == dbits(ref.outputs[i * 3 + k]));
    }
  }
}

SCENARIO(
    "FusedExpressionVector emits correct partial-batch output through port path",
    "[batched][partial_batch]") {
  using namespace rtbot;
  auto op = make_fused_expression_vector(
      "fev", 1, std::vector<double>{INPUT, 0, INPUT, 1, ADD, END},
      std::vector<double>{});
  for (std::size_t t = 1; t <= 3; ++t) {
    auto vec = std::make_shared<std::vector<double>>(
        std::vector<double>{10.0 * static_cast<double>(t),
                             20.0 * static_cast<double>(t)});
    op->receive_data(create_message<VectorNumberData>(
                         static_cast<std::int64_t>(t),
                         VectorNumberData(std::move(vec))),
                     0);
  }
  op->execute();

  auto& q = op->get_output_queue(0);
  REQUIRE(q.size() == 3);
  for (std::size_t i = 0; i < 3; ++i) {
    const auto* m = dynamic_cast<const Message<VectorNumberData>*>(q[i].get());
    REQUIRE(m != nullptr);
    REQUIRE(m->time == static_cast<std::int64_t>(i + 1));
    REQUIRE((*m->data.values)[0] ==
            Approx(10.0 * (i + 1) + 20.0 * (i + 1)));
  }
}
