// Correctness tests for BurstAggregate.
//
// BurstAggregate must produce the same aggregate output as the current
// per-message aggregate fused-expression path. These scenarios feed a stream
// of VectorNumberData rows through BurstAggregate and compare the emitted
// aggregate against a hand-computed reference. A segment transition fires
// emission; two transitions in a row exercise the reset path.

#include <catch2/catch.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/fuse/BurstAggregate.h"
#include "rtbot/fuse/FusedOps.h"

using namespace rtbot;
using namespace fused_op;

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t b;
  std::memcpy(&b, &v, sizeof(double));
  return b;
}

// Single-column vibration-style workload: rows of shape [amplitude] only;
// aggregate the running mean via CUMSUM + COUNT + DIV; segment transitions
// when amplitude crosses through zero (ABS(amp) > 0).
std::shared_ptr<BurstAggregate> make_mean_with_zero_gate(const std::string& id) {
  // Aggregate bytecode: AVG(amplitude) via CUMSUM/COUNT/DIV.
  // INPUT 0, CUMSUM 0, COUNT 2, DIV, END
  // State[0..1] = kahan sum+comp for CUMSUM, State[2] = count
  std::vector<double> agg_bc = {INPUT, 0, CUMSUM, 0, COUNT, 2, DIV, END};

  // Segment predicate: ABS(amplitude) > 0  (emit when crossing through zero)
  // INPUT 0, ABS, CONST 0, GT, END
  std::vector<double> seg_bc = {INPUT, 0, ABS, CONST, 0, GT, END};
  std::vector<double> seg_consts = {0.0};

  return make_burst_aggregate(id, std::move(agg_bc), {},
                                std::move(seg_bc), std::move(seg_consts),
                                /*key_columns=*/{},
                                /*num_agg_outputs=*/1,
                                /*num_input_cols=*/1);
}

void feed_row(BurstAggregate& op, std::int64_t t, std::vector<double> row) {
  std::vector<std::unique_ptr<BaseMessage>> batch;
  batch.push_back(
      create_message<VectorNumberData>(t, VectorNumberData{std::move(row)}));
  op.receive_data_batch(batch, 0, /*debug=*/false);
}

}  // namespace

SCENARIO("BurstAggregate emits running mean at segment transition",
         "[burst_aggregate]") {
  auto op = make_mean_with_zero_gate("b1");
  auto col = make_vector_number_collector("c");
  op->connect(col, 0, 0);

  // Segment 1: amplitudes 1, 2, 3, 4 → mean = 2.5
  for (std::int64_t t = 1; t <= 4; ++t) {
    feed_row(*op, t, {static_cast<double>(t)});
  }

  // Sentinel: amplitude 0 → ABS(amp) > 0 becomes false, triggers emit.
  feed_row(*op, 5, {0.0});

  auto& q = col->get_data_queue(0);
  REQUIRE(q.size() == 1);
  const auto* m = dynamic_cast<const Message<VectorNumberData>*>(q[0].get());
  REQUIRE(m != nullptr);
  REQUIRE(m->time == 5);
  REQUIRE(m->data.values->size() == 1);
  REQUIRE(dbits((*m->data.values)[0]) == dbits(2.5));
}

SCENARIO("BurstAggregate resets state between segments", "[burst_aggregate]") {
  auto op = make_mean_with_zero_gate("b2");
  auto col = make_vector_number_collector("c");
  op->connect(col, 0, 0);

  // Emission order (mirroring current rtbot segment semantics — every
  // predicate transition closes one segment and opens the next):
  //   segment 1 (1,2,3,  pred=TRUE) → mean 2.0 at t=4
  //   segment 2 (sentinel 0, pred=FALSE) → mean 0.0 at t=5
  //   segment 3 (10,20, pred=TRUE)  → mean 15.0 at t=7
  // SQL callers that only want the amp>0 segments filter downstream
  // via `WHERE sample_count > 1` (matching IMS).
  feed_row(*op, 1, {1.0});
  feed_row(*op, 2, {2.0});
  feed_row(*op, 3, {3.0});
  feed_row(*op, 4, {0.0});
  feed_row(*op, 5, {10.0});
  feed_row(*op, 6, {20.0});
  feed_row(*op, 7, {0.0});

  auto& q = col->get_data_queue(0);
  REQUIRE(q.size() == 3);
  const auto* m1 = dynamic_cast<const Message<VectorNumberData>*>(q[0].get());
  const auto* m2 = dynamic_cast<const Message<VectorNumberData>*>(q[1].get());
  const auto* m3 = dynamic_cast<const Message<VectorNumberData>*>(q[2].get());
  REQUIRE(m1->time == 4);
  REQUIRE(dbits((*m1->data.values)[0]) == dbits(2.0));
  REQUIRE(m2->time == 5);
  REQUIRE(dbits((*m2->data.values)[0]) == dbits(0.0));
  REQUIRE(m3->time == 7);
  REQUIRE(dbits((*m3->data.values)[0]) == dbits(15.0));
}

SCENARIO("BurstAggregate passes key columns through on emit",
         "[burst_aggregate][keys]") {
  // Rows of shape [device_id, channel_id, amplitude]. Keys = [0, 1].
  std::vector<double> agg_bc = {INPUT, 2, CUMSUM, 0, COUNT, 2, DIV, END};
  std::vector<double> seg_bc = {INPUT, 2, ABS, CONST, 0, GT, END};
  std::vector<double> seg_consts = {0.0};
  auto op = make_burst_aggregate(
      "b3", std::move(agg_bc), {}, std::move(seg_bc), std::move(seg_consts),
      /*key_columns=*/{0, 1}, /*num_agg_outputs=*/1, /*num_input_cols=*/3);
  auto col = make_vector_number_collector("c");
  op->connect(col, 0, 0);

  // Amplitudes 2, 4, 6 → mean 4; keys device=7, channel=1.
  feed_row(*op, 1, {7.0, 1.0, 2.0});
  feed_row(*op, 2, {7.0, 1.0, 4.0});
  feed_row(*op, 3, {7.0, 1.0, 6.0});
  feed_row(*op, 4, {7.0, 1.0, 0.0});

  auto& q = col->get_data_queue(0);
  REQUIRE(q.size() == 1);
  const auto* m = dynamic_cast<const Message<VectorNumberData>*>(q[0].get());
  REQUIRE(m != nullptr);
  REQUIRE(m->data.values->size() == 3);
  REQUIRE(dbits((*m->data.values)[0]) == dbits(7.0));
  REQUIRE(dbits((*m->data.values)[1]) == dbits(1.0));
  REQUIRE(dbits((*m->data.values)[2]) == dbits(4.0));
}

SCENARIO("BurstAggregate raw-buffer path matches batch path",
         "[burst_aggregate][raw_buffer]") {
  auto op = make_mean_with_zero_gate("b_raw");
  auto col = make_vector_number_collector("c");
  op->connect(col, 0, 0);

  // Same inputs as the first scenario but delivered as a flat double buffer +
  // parallel timestamps array — the path production callers take.
  std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 0.0};
  std::vector<timestamp_t> times = {1, 2, 3, 4, 5};
  op->receive_data_buffer(data.data(), data.size(), /*num_cols=*/1,
                           times.data(), /*port=*/0, /*debug=*/false);

  auto& q = col->get_data_queue(0);
  REQUIRE(q.size() == 1);
  const auto* m = dynamic_cast<const Message<VectorNumberData>*>(q[0].get());
  REQUIRE(m != nullptr);
  REQUIRE(m->time == 5);
  REQUIRE(dbits((*m->data.values)[0]) == dbits(2.5));
}

SCENARIO("BurstAggregate fallback per-message path matches batch path",
         "[burst_aggregate][per_message]") {
  auto op = make_mean_with_zero_gate("b4");
  auto col = make_vector_number_collector("c");
  op->connect(col, 0, 0);

  // Same inputs as the first scenario, but delivered via per-message
  // receive_data so the fallback process_data() path runs.
  for (std::int64_t t = 1; t <= 4; ++t) {
    op->receive_data(create_message<VectorNumberData>(
                          t, VectorNumberData{{static_cast<double>(t)}}),
                      0);
  }
  op->receive_data(
      create_message<VectorNumberData>(5, VectorNumberData{{0.0}}), 0);
  op->execute();

  auto& q = col->get_data_queue(0);
  REQUIRE(q.size() == 1);
  const auto* m = dynamic_cast<const Message<VectorNumberData>*>(q[0].get());
  REQUIRE(m != nullptr);
  REQUIRE(dbits((*m->data.values)[0]) == dbits(2.5));
}
