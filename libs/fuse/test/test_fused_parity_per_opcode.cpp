#include <catch2/catch.hpp>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <sstream>
#include <vector>

#include "fused_parity/live_drivers.h"
#include "fused_parity/opcode_spec.h"
#include "fused_parity/reference_eval.h"
#include "rtbot/fuse/FusedExpression.h"
#include "rtbot/fuse/FusedExpressionVector.h"

using namespace rtbot;
using namespace rtbot::fused_op;
using namespace rtbot::fused_parity;

namespace {

inline std::uint64_t double_bits(double v) {
  std::uint64_t bits;
  std::memcpy(&bits, &v, sizeof(double));
  return bits;
}

inline std::int64_t double_bits_signed(double v) {
  std::int64_t bits;
  std::memcpy(&bits, &v, sizeof(double));
  return bits;
}

inline bool bit_equal(double a, double b) {
  return double_bits(a) == double_bits(b);
}

inline std::int64_t ulp_distance(double a, double b) {
  if (std::isnan(a) && std::isnan(b)) return 0;
  if (std::isnan(a) != std::isnan(b)) return std::numeric_limits<std::int64_t>::max();
  auto ai = double_bits_signed(a);
  auto bi = double_bits_signed(b);
  if (ai < 0) ai = std::numeric_limits<std::int64_t>::min() - ai;
  if (bi < 0) bi = std::numeric_limits<std::int64_t>::min() - bi;
  return std::llabs(ai - bi);
}

std::vector<double> sample_inputs(std::mt19937_64& rng, int arity,
                                   double min_val) {
  std::uniform_real_distribution<double> dist(
      std::max(min_val, -1e6), 1e6);
  std::vector<double> out(arity);
  for (auto& v : out) v = dist(rng);
  return out;
}

std::vector<double> build_single_op_bytecode(const OpcodeSpec& spec) {
  std::vector<double> bc;
  for (int i = 0; i < spec.arity; ++i) {
    bc.push_back(INPUT);
    bc.push_back(static_cast<double>(i));
  }
  bc.push_back(spec.opcode);
  bc.push_back(END);
  return bc;
}

double run_live(const std::vector<double>& bc, int num_ports,
                const std::vector<double>& inputs) {
  auto op = make_fused_expression("op", num_ports, 1, bc, {});
  auto col = make_vector_number_collector("c");
  op->connect(col, 0, 0);
  for (int i = 0; i < num_ports; ++i) {
    op->receive_data(create_message<NumberData>(1, NumberData{inputs[i]}), i);
  }
  op->execute();
  auto& q = col->get_data_queue(0);
  REQUIRE(q.size() == 1);
  const auto* msg = dynamic_cast<const Message<VectorNumberData>*>(q[0].get());
  REQUIRE(msg != nullptr);
  return (*msg->data.values)[0];
}

double run_live_vector(const std::vector<double>& bc, int num_ports,
                        const std::vector<double>& inputs) {
  auto op = make_fused_expression_vector("opv", 1, bc, {});
  auto col = make_vector_number_collector("c");
  op->connect(col, 0, 0);
  auto vec = std::make_shared<std::vector<double>>(inputs);
  (void)num_ports;
  op->receive_data(create_message<VectorNumberData>(
                       1, VectorNumberData(std::move(vec))),
                   0);
  op->execute();
  auto& q = col->get_data_queue(0);
  REQUIRE(q.size() == 1);
  const auto* msg = dynamic_cast<const Message<VectorNumberData>*>(q[0].get());
  REQUIRE(msg != nullptr);
  return (*msg->data.values)[0];
}

}  // namespace

SCENARIO("Stateless non-transcendental opcodes are bit-exact against reference",
         "[per_opcode][bit_exact]") {
  for (const auto& spec : stateless_non_transcendental_specs()) {
    DYNAMIC_SECTION(spec.name) {
      std::mt19937_64 rng(0xF05ED1ULL ^
                          std::hash<std::string>{}(spec.name));
      int num_ports = std::max(1, spec.arity);
      for (int trial = 0; trial < 1000; ++trial) {
        auto inputs = sample_inputs(rng, num_ports, spec.min_input_a);
        if (spec.arity == 0) continue;
        auto bc = build_single_op_bytecode(spec);
        auto ref = evaluate_scalar(bc, {}, {inputs}, {}, 1);
        REQUIRE(ref.outputs.size() == 1);
        double live = run_live(bc, num_ports, inputs);
        double live_v = run_live_vector(bc, num_ports, inputs);
        INFO("opcode=" << spec.name << " trial=" << trial
                       << " inputs[0]=" << inputs[0]
                       << (num_ports > 1 ? " inputs[1]=" : "")
                       << (num_ports > 1 ? inputs[1] : 0.0));
        REQUIRE(bit_equal(live, ref.outputs[0]));
        REQUIRE(bit_equal(live_v, ref.outputs[0]));
      }
    }
  }
}

SCENARIO("Stateful opcodes are bit-exact across message sequences",
         "[per_opcode][stateful]") {
  constexpr std::size_t kSeqLen = 256;

  // Build a program: INPUT 0, OPCODE 0 (state_off=0), END.
  for (const auto& spec : stateful_specs()) {
    DYNAMIC_SECTION(spec.name) {
      std::mt19937_64 rng(0xC0FFEEULL ^ std::hash<std::string>{}(spec.name));

      // Bytecode and state layout depend on the opcode.
      std::vector<double> bc;
      std::vector<double> state_init;
      int num_ports = 1;

      if (spec.name == "CUMSUM") {
        bc = {INPUT, 0, CUMSUM, 0, END};
        state_init = {0.0, 0.0};  // sum + kahan compensation
      } else if (spec.name == "COUNT") {
        // COUNT doesn't pop; it still needs a port to sync the tick.
        bc = {INPUT, 0, COUNT, 0, END};
        state_init = {0.0};
        // The leading INPUT pushes a value; COUNT pushes the counter.
        // END pops the top (the counter). The INPUT value stays on the stack.
        // Per FusedExpression::process_data, sp is reset to 0 after END, so
        // the leading INPUT is harmless.
      } else if (spec.name == "MAX_AGG") {
        bc = {INPUT, 0, MAX_AGG, 0, END};
        state_init = {-std::numeric_limits<double>::infinity()};
      } else if (spec.name == "MIN_AGG") {
        bc = {INPUT, 0, MIN_AGG, 0, END};
        state_init = {std::numeric_limits<double>::infinity()};
      } else if (spec.name == "STATE_LOAD") {
        // Seed state[0] = 42.0; program pushes it each tick. Use INPUT 0 to
        // synchronize the tick.
        bc = {INPUT, 0, STATE_LOAD, 0, END};
        state_init = {42.0};
      } else {
        continue;  // unhandled spec
      }

      std::vector<std::vector<double>> msgs(kSeqLen);
      std::uniform_real_distribution<double> dist(-1e3, 1e3);
      for (auto& m : msgs) m = {dist(rng)};

      auto ref = evaluate_scalar(bc, {}, msgs, state_init, 1);
      REQUIRE(ref.outputs.size() == kSeqLen);

      // Drive live FusedExpression — full message sequence.
      auto fe_flat = drive_fused_expression(bc, {}, msgs, state_init,
                                              static_cast<std::size_t>(num_ports),
                                              1);
      REQUIRE(fe_flat.size() == ref.outputs.size());
      for (std::size_t t = 0; t < kSeqLen; ++t) {
        INFO("opcode=" << spec.name << " t=" << t << " fe");
        REQUIRE(bit_equal(fe_flat[t], ref.outputs[t]));
      }

      // Drive live FusedExpressionVector — same sequence, vector input.
      auto fev_flat = drive_fused_expression_vector(bc, {}, msgs, state_init, 1);
      REQUIRE(fev_flat.size() == ref.outputs.size());
      for (std::size_t t = 0; t < kSeqLen; ++t) {
        INFO("opcode=" << spec.name << " t=" << t << " fev");
        REQUIRE(bit_equal(fev_flat[t], ref.outputs[t]));
      }
    }
  }
}

SCENARIO("Stateless transcendental opcodes are bit-exact under scalar libm",
         "[per_opcode][ulp]") {
  // With default build (scalar libm on both sides), bit-exact parity is
  // achievable. Phase 5 may relax this for xsimd polynomial approximations.
  const std::int64_t kUlpTolerance = 0;

  for (const auto& spec : stateless_transcendental_specs()) {
    DYNAMIC_SECTION(spec.name) {
      std::mt19937_64 rng(0xC05EULL ^ std::hash<std::string>{}(spec.name));
      int num_ports = std::max(1, spec.arity);
      for (int trial = 0; trial < 1000; ++trial) {
        auto inputs = sample_inputs(rng, num_ports, spec.min_input_a);
        // Domain restrictions for functions that would produce NaN on
        // negative arguments — compare NaN vs NaN is fine (ulp_distance
        // returns 0 for same-bit NaNs) but we want to stay in the real
        // domain to exercise the actual polynomial paths.
        if (spec.name == "SQRT" || spec.name == "LOG" || spec.name == "LOG10") {
          inputs[0] = std::abs(inputs[0]) + 1e-6;
        }
        if (spec.name == "POW") {
          inputs[0] = std::abs(inputs[0]) + 1e-6;
          inputs[1] = std::fmod(inputs[1], 10.0);  // keep exponent reasonable
        }
        auto bc = build_single_op_bytecode(spec);
        auto ref = evaluate_scalar(bc, {}, {inputs}, {}, 1);
        double live = run_live(bc, num_ports, inputs);
        double live_v = run_live_vector(bc, num_ports, inputs);
        INFO("opcode=" << spec.name << " trial=" << trial);
        REQUIRE(ulp_distance(live, ref.outputs[0]) <= kUlpTolerance);
        REQUIRE(ulp_distance(live_v, ref.outputs[0]) <= kUlpTolerance);
      }
    }
  }
}
