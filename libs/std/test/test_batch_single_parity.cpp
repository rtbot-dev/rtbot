#include <catch2/catch.hpp>
#include <memory>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/std/ArithmeticScalar.h"
#include "rtbot/std/CumulativeSum.h"

using namespace rtbot;

namespace {

// Run `op` against `inputs` in single-emit mode (execute() between each
// receive; input queue never exceeds 1, so emit_output takes the single-msg
// path) and collect the (time, value) outputs from port 0.
std::vector<std::pair<timestamp_t, double>> drive_single(
    const std::shared_ptr<Operator>& op,
    const std::vector<std::pair<timestamp_t, double>>& inputs) {
  auto col = std::make_shared<Collector>("c_single", std::vector<std::string>{"number"});
  op->connect(col, 0, 0);
  for (const auto& [t, v] : inputs) {
    op->receive_data(create_message<NumberData>(t, NumberData{v}), 0);
    op->execute();
  }
  std::vector<std::pair<timestamp_t, double>> out;
  auto& q = col->get_data_queue(0);
  for (auto& msg : q) {
    auto* m = dynamic_cast<const Message<NumberData>*>(msg.get());
    out.emplace_back(m->time, m->data.value);
  }
  return out;
}

// Same inputs but queue them all, then execute() once — drives the batch
// path when inputs.size() >= kEmitBatchThreshold.
std::vector<std::pair<timestamp_t, double>> drive_batch(
    const std::shared_ptr<Operator>& op,
    const std::vector<std::pair<timestamp_t, double>>& inputs) {
  auto col = std::make_shared<Collector>("c_batch", std::vector<std::string>{"number"});
  op->connect(col, 0, 0);
  for (const auto& [t, v] : inputs) {
    op->receive_data(create_message<NumberData>(t, NumberData{v}), 0);
  }
  op->execute();
  std::vector<std::pair<timestamp_t, double>> out;
  auto& q = col->get_data_queue(0);
  for (auto& msg : q) {
    auto* m = dynamic_cast<const Message<NumberData>*>(msg.get());
    out.emplace_back(m->time, m->data.value);
  }
  return out;
}

std::vector<std::pair<timestamp_t, double>> ramp(size_t n) {
  std::vector<std::pair<timestamp_t, double>> v;
  v.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    v.emplace_back(static_cast<timestamp_t>(i + 1), static_cast<double>(i) * 0.5 - 3.0);
  }
  return v;
}

}  // namespace

SCENARIO("Batch-emit path produces identical outputs to single-emit path",
         "[batch_parity][regression]") {
  // N spans: below threshold (5), exactly at (20), above (25), and a large
  // burst (100) to stress repeated batch cycles if the operator drains piecewise.
  auto sizes = {size_t{5}, size_t{20}, size_t{25}, size_t{100}};

  SECTION("Add (stateless scalar)") {
    for (size_t n : sizes) {
      auto in = ramp(n);
      auto a = std::make_shared<Add>("a_s", 1.25);
      auto b = std::make_shared<Add>("a_b", 1.25);
      REQUIRE(drive_single(a, in) == drive_batch(b, in));
    }
  }

  SECTION("Scale (stateless scalar)") {
    for (size_t n : sizes) {
      auto in = ramp(n);
      auto a = std::make_shared<Scale>("s_s", 2.5);
      auto b = std::make_shared<Scale>("s_b", 2.5);
      REQUIRE(drive_single(a, in) == drive_batch(b, in));
    }
  }

  SECTION("CumulativeSum (stateful)") {
    for (size_t n : sizes) {
      auto in = ramp(n);
      auto a = std::make_shared<CumulativeSum>("cs_s");
      auto b = std::make_shared<CumulativeSum>("cs_b");
      REQUIRE(drive_single(a, in) == drive_batch(b, in));
    }
  }

  SECTION("Exactly at threshold boundary N=kEmitBatchThreshold") {
    auto in = ramp(kEmitBatchThreshold);
    auto a = std::make_shared<CumulativeSum>("b_s");
    auto b = std::make_shared<CumulativeSum>("b_b");
    REQUIRE(drive_single(a, in) == drive_batch(b, in));
  }
}
