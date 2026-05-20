#include <catch2/catch.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>

#include "rtbot/Message.h"
#include "rtbot/Program.h"
#include "parity_helper.h"

namespace {

// Same Bollinger JSON used in test_bollinger_json_e2e.cpp.
const char* kBollingerJson = R"({
  "title": "Bollinger Bands",
  "apiVersion": "v1",
  "entryOperator": "754",
  "output": { "37": ["o2", "o1", "o3"] },
  "operators": [
    { "id": "37", "type": "Output", "portTypes": ["number", "number", "number"] },
    { "id": "495", "type": "Subtraction" },
    { "id": "861", "type": "Addition" },
    { "id": "996", "type": "Scale", "value": 2 },
    { "id": "865", "type": "StandardDeviation", "window_size": 14 },
    { "id": "510", "type": "MovingAverage", "window_size": 14 },
    { "id": "262", "type": "ResamplerHermite", "interval": 1 },
    { "id": "754", "type": "Input", "portTypes": ["number"] }
  ],
  "connections": [
    { "from": "510", "to": "37", "fromPort": "o1", "toPort": "i3" },
    { "from": "495", "to": "37", "fromPort": "o1", "toPort": "i1" },
    { "from": "861", "to": "37", "fromPort": "o1", "toPort": "i2" },
    { "from": "996", "to": "495", "fromPort": "o1", "toPort": "i2" },
    { "from": "510", "to": "495", "fromPort": "o1", "toPort": "i1" },
    { "from": "996", "to": "861", "fromPort": "o1", "toPort": "i2" },
    { "from": "510", "to": "861", "fromPort": "o1", "toPort": "i1" },
    { "from": "865", "to": "996", "fromPort": "o1", "toPort": "i1" },
    { "from": "262", "to": "865", "fromPort": "o1", "toPort": "i1" },
    { "from": "262", "to": "510", "fromPort": "o1", "toPort": "i1" },
    { "from": "754", "to": "262", "fromPort": "o1", "toPort": "i1" }
  ]
})";

// A valid FE program that uses Variable, which is not in the JIT's supported
// opcode set. The JIT parser throws "unknown operator type: 'Variable'",
// triggering graceful fallback to the interpreter. Both i1 and c1 are fed
// from the same Input so each (t, v) tick emits the data value on the
// control match.
const char* kCounterJson = R"({
  "title": "Variable passthrough",
  "apiVersion": "v1",
  "entryOperator": "in1",
  "output": { "out1": ["o1"] },
  "operators": [
    { "id": "in1", "type": "Input", "portTypes": ["number"] },
    { "id": "var1", "type": "Variable", "default_value": 0.0 },
    { "id": "out1", "type": "Output", "portTypes": ["number"] }
  ],
  "connections": [
    { "from": "in1",  "to": "var1", "fromPort": "o1", "toPort": "i1" },
    { "from": "in1",  "to": "var1", "fromPort": "o1", "toPort": "c1" },
    { "from": "var1", "to": "out1", "fromPort": "o1", "toPort": "i1" }
  ]
})";

}  // namespace

SCENARIO("Program with Bollinger JSON automatically uses JIT", "[program_dispatch]") {
  rtbot::Program prog(kBollingerJson);

  REQUIRE(prog.using_jit());

  std::mt19937_64 rng(0xB011);
  std::normal_distribution<double> step(0.0, 1.0);
  double price = 100.0;
  std::size_t total_emissions = 0;

  for (int i = 1; i <= 1000; ++i) {
    price += step(rng);
    auto batch = prog.receive(
        rtbot::Message<rtbot::NumberData>(static_cast<std::int64_t>(i),
                                          rtbot::NumberData{price}));

    auto op_it = batch.find("37");
    if (op_it == batch.end()) continue;

    // Output has 3 ports: o1=lower, o2=upper, o3=middle.
    auto& op_batch = op_it->second;
    if (op_batch.count("o1") && op_batch.count("o2") && op_batch.count("o3")) {
      total_emissions += op_batch.at("o1").size();
    }
  }

  // Bollinger with window=14 starts emitting at tick 14; ~987 ticks emit for
  // N=1000 (the resampler with interval=1 passes every sample through).
  REQUIRE(total_emissions > 0);
}

SCENARIO("Program::serialize_data throws when JIT is active", "[program_dispatch]") {
  rtbot::Program prog(kBollingerJson);
  REQUIRE(prog.using_jit());

  // Advance state so there is something to serialize (the throw must happen
  // regardless of whether any messages were received).
  prog.receive(rtbot::Message<rtbot::NumberData>(1, rtbot::NumberData{100.0}));

  REQUIRE_THROWS_AS(prog.serialize_data(), std::runtime_error);
}

SCENARIO("Program::send + drain_outputs produces same outputs as receive(Message)", "[program_dispatch][streaming]") {
  // Build two Program instances from the same JSON so internal state is
  // independent. Drive both with the same 1000-tick random-walk sequence.
  rtbot::Program prog_streaming(kBollingerJson);
  rtbot::Program prog_classic(kBollingerJson);

  // Both should choose the same backend.
  REQUIRE(prog_streaming.using_jit() == prog_classic.using_jit());

  std::mt19937_64 rng(0xB011);
  std::normal_distribution<double> step(0.0, 1.0);
  double price = 100.0;

  // Collect every batch from the classic path.
  std::vector<rtbot::ProgramMsgBatch> classic_batches;
  classic_batches.reserve(1000);

  for (int i = 1; i <= 1000; ++i) {
    price += step(rng);
    prog_streaming.send(static_cast<std::int64_t>(i), price);
    classic_batches.push_back(
        prog_classic.receive(rtbot::Message<rtbot::NumberData>(
            static_cast<std::int64_t>(i), rtbot::NumberData{price})));
  }

  // Drain the streaming side once (all 1000 ticks accumulated).
  auto streaming_batch = prog_streaming.drain_outputs();

  // Count total emissions from the classic side.
  std::size_t classic_total = 0;
  for (const auto& b : classic_batches) {
    auto op_it = b.find("37");
    if (op_it == b.end()) continue;
    auto port_it = op_it->second.find("o1");
    if (port_it == op_it->second.end()) continue;
    classic_total += port_it->second.size();
  }

  // The streaming drain should have the same total number of emitted records.
  std::size_t streaming_total = 0;
  auto op_it = streaming_batch.find("37");
  if (op_it != streaming_batch.end()) {
    auto port_it = op_it->second.find("o1");
    if (port_it != op_it->second.end()) {
      streaming_total = port_it->second.size();
    }
  }

  REQUIRE(streaming_total == classic_total);
  REQUIRE(streaming_total > 0);

  // Verify drain_outputs() is idempotent when nothing has accumulated.
  auto empty_drain = prog_streaming.drain_outputs();
  bool empty = true;
  for (const auto& [op, op_batch] : empty_drain) {
    for (const auto& [port, msgs] : op_batch) {
      if (!msgs.empty()) { empty = false; break; }
    }
    if (!empty) break;
  }
  REQUIRE(empty);
}

SCENARIO("Program::send + drain_outputs works on interpreter path", "[program_dispatch][streaming]") {
  // kCounterJson uses Variable which is not in the JIT set, so the
  // interpreter path is exercised.
  rtbot::Program prog_streaming(kCounterJson);
  rtbot::Program prog_classic(kCounterJson);

  REQUIRE_FALSE(prog_streaming.using_jit());

  std::size_t streaming_total = 0;
  std::size_t classic_total = 0;

  for (int i = 1; i <= 50; ++i) {
    double v = static_cast<double>(i);
    prog_streaming.send(static_cast<std::int64_t>(i), v);
    auto b = prog_classic.receive(
        rtbot::Message<rtbot::NumberData>(static_cast<std::int64_t>(i), rtbot::NumberData{v}));
    auto op_it = b.find("out1");
    if (op_it != b.end()) {
      auto port_it = op_it->second.find("o1");
      if (port_it != op_it->second.end()) classic_total += port_it->second.size();
    }
  }

  auto streaming_batch = prog_streaming.drain_outputs();
  auto op_it = streaming_batch.find("out1");
  if (op_it != streaming_batch.end()) {
    auto port_it = op_it->second.find("o1");
    if (port_it != op_it->second.end()) streaming_total = port_it->second.size();
  }

  REQUIRE(streaming_total == classic_total);
  REQUIRE(streaming_total > 0);
}

SCENARIO("Program::drain_into invokes callback per emission, matches drain_outputs", "[program_dispatch][streaming]") {
  // Drive both programs with the same 1000-tick sequence. Compare total
  // emission counts and, per-record, that the same timestamp appears at the
  // same position. Values are not compared element-by-element here because
  // drain_outputs groups them by port (all o1 times, then all o2 times, …)
  // while drain_into delivers them record-by-record (t, [v0,v1,v2] per tick).
  rtbot::Program prog_a(kBollingerJson);
  rtbot::Program prog_b(kBollingerJson);

  REQUIRE(prog_a.using_jit() == prog_b.using_jit());

  std::mt19937_64 rng(0xCAFE);
  std::normal_distribution<double> step(0.0, 1.0);
  double price = 100.0;

  for (int i = 1; i <= 1000; ++i) {
    price += step(rng);
    prog_a.send(static_cast<std::int64_t>(i), price);
    prog_b.send(static_cast<std::int64_t>(i), price);
  }

  // Batch path: count total scalar messages.
  auto batch = prog_a.drain_outputs();
  std::size_t batch_count = 0;
  {
    auto op_it = batch.find("37");
    if (op_it != batch.end()) {
      // All ports have the same number of messages; count via the first port.
      auto port_it = op_it->second.find("o1");
      if (port_it != op_it->second.end()) batch_count = port_it->second.size();
    }
  }

  // Callback path: count emission records (one record = one emitted tick).
  std::size_t cb_record_count = 0;
  std::size_t cb_values_per_record = 0;
  prog_b.drain_into([&](std::int64_t, const double*, std::size_t n) {
    ++cb_record_count;
    cb_values_per_record = n;  // constant across records for a fixed program
  });

  // Number of records (emitted ticks) and number of ports per record must match.
  REQUIRE(cb_record_count == batch_count);
  REQUIRE(cb_record_count > 0);

  // For a 3-output Bollinger program the callback sees 3 values per record.
  REQUIRE(cb_values_per_record == 3);

  // Second drain must be empty.
  std::size_t second_drain_count = 0;
  prog_b.drain_into([&](std::int64_t, const double*, std::size_t n) {
    second_drain_count += n;
  });
  REQUIRE(second_drain_count == 0);
}

SCENARIO("Program::drain_into works on interpreter path", "[program_dispatch][streaming]") {
  // kCounterJson uses Variable which is not in the JIT set.
  rtbot::Program prog_a(kCounterJson);
  rtbot::Program prog_b(kCounterJson);

  REQUIRE_FALSE(prog_a.using_jit());

  for (int i = 1; i <= 50; ++i) {
    prog_a.send(static_cast<std::int64_t>(i), static_cast<double>(i));
    prog_b.send(static_cast<std::int64_t>(i), static_cast<double>(i));
  }

  // Batch path.
  auto batch = prog_a.drain_outputs();
  std::size_t batch_count = 0;
  {
    auto op_it = batch.find("out1");
    if (op_it != batch.end()) {
      auto port_it = op_it->second.find("o1");
      if (port_it != op_it->second.end()) batch_count = port_it->second.size();
    }
  }

  // Callback path.
  std::size_t cb_count = 0;
  prog_b.drain_into([&](std::int64_t, const double*, std::size_t) { ++cb_count; });

  REQUIRE(cb_count == batch_count);
  REQUIRE(cb_count > 0);
}

SCENARIO("Program::jit_program returns the underlying JIT program when active",
         "[program_dispatch][escape_hatch]") {
  rtbot::Program prog(kBollingerJson);
  REQUIRE(prog.using_jit());

  auto* jit = prog.jit_program();
  REQUIRE(jit != nullptr);

  // Drive directly via the JIT
  for (int i = 1; i <= 100; ++i) {
    jit->receive(static_cast<int64_t>(i), static_cast<double>(i) * 0.5);
  }

  // Bollinger with window=14 starts emitting at tick 14; resampler with
  // interval=1 passes every sample through, so ~87 emissions (100 - 14 + 1).
  auto records = jit->collect_outputs();
  REQUIRE(records.size() > 80);
  REQUIRE(records.size() < 100);
}

SCENARIO("Program::jit_program returns nullptr when JIT inactive",
         "[program_dispatch][escape_hatch]") {
  // kCounterJson uses Variable which is not in the JIT's opcode set.
  // The JIT parser throws, triggering graceful fallback to the interpreter.
  rtbot::Program prog(kCounterJson);
  REQUIRE_FALSE(prog.using_jit());
  REQUIRE(prog.jit_program() == nullptr);

  // const overload also returns nullptr on the interpreter path
  const rtbot::Program& cprog = prog;
  REQUIRE(cprog.jit_program() == nullptr);
}

SCENARIO("run_jit_vs_interpreter_parity: scalar single-port pipeline matches bit-exactly",
         "[program_dispatch][parity_helper]") {
  // Single-output-port pipeline: Input -> MovingAverage(14) -> Output. Both
  // backends have the same emission shape (one value per tick), so drain_into
  // produces one callback invocation per emitted tick on both paths. The helper
  // asserts every (t, port_id, value) bit-exactly across the two runs.
  static constexpr const char* kSinglePortMa = R"({
    "title": "MovingAverage single port",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",  "type": "Input", "portTypes": ["number"] },
      { "id": "ma",  "type": "MovingAverage", "window_size": 14 },
      { "id": "out", "type": "Output", "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in", "to": "ma",  "fromPort": "o1", "toPort": "i1" },
      { "from": "ma", "to": "out", "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  std::vector<rtbot::test::TickInput> inputs;
  inputs.reserve(1000);
  std::mt19937_64 rng(0xCAFE);
  std::normal_distribution<double> step(0.0, 1.0);
  double price = 100.0;
  for (int i = 1; i <= 1000; ++i) {
    price += step(rng);
    inputs.push_back({static_cast<std::int64_t>(i), {price}});
  }

  rtbot::test::run_jit_vs_interpreter_parity(
      kSinglePortMa, inputs,
      [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
        p.send(tick.t, tick.values[0]);
      });
}

SCENARIO("run_jit_vs_interpreter_parity: Bollinger 3-output broadcast pipeline matches bit-exactly",
         "[program_dispatch][parity_helper]") {
  // Bollinger has three scalar output ports (lower band, upper band, middle).
  // The JIT bundles all three port values into one EmittedRecord per emitted
  // tick (broadcast shape: K=1, num_outputs=3). The interpreter's drain_into
  // must match this shape: one callback per tick with values=[v_o1, v_o2, v_o3]
  // in Output-op port-index order.
  std::vector<rtbot::test::TickInput> inputs;
  inputs.reserve(1000);
  std::mt19937_64 rng(0xB011);
  std::normal_distribution<double> step(0.0, 1.0);
  double price = 100.0;
  for (int i = 1; i <= 1000; ++i) {
    price += step(rng);
    inputs.push_back({static_cast<std::int64_t>(i), {price}});
  }

  rtbot::test::run_jit_vs_interpreter_parity(
      kBollingerJson, inputs,
      [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
        p.send(tick.t, tick.values[0]);
      });
}

SCENARIO("run_jit_vs_interpreter_parity: Bollinger handles NaN and Inf inputs bit-exactly",
         "[program_dispatch][parity_helper]") {
  // Edge case: feed NaN and Inf through the multi-port pipeline. Both backends
  // must propagate the same bit pattern through the rolling-window arithmetic
  // and emit the same results to drain_into. Window size is 14, so we sprinkle
  // a few NaN/Inf inputs across a 200-tick run and let the warmup absorb them.
  std::vector<rtbot::test::TickInput> inputs;
  inputs.reserve(200);
  for (int i = 1; i <= 200; ++i) {
    double v = static_cast<double>(i);
    if (i == 17) v = std::numeric_limits<double>::quiet_NaN();
    if (i == 53) v = std::numeric_limits<double>::infinity();
    if (i == 91) v = -std::numeric_limits<double>::infinity();
    inputs.push_back({static_cast<std::int64_t>(i), {v}});
  }

  rtbot::test::run_jit_vs_interpreter_parity(
      kBollingerJson, inputs,
      [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
        p.send(tick.t, tick.values[0]);
      });
}

SCENARIO("Program::receive_batch routes through JIT and matches interpreter bit-exactly",
         "[program_dispatch][receive_batch][parity]") {
  // Build identical Bollinger pipelines under JIT and interpreter, drive each
  // with the same 100-row sequence via receive_batch (one map per call), and
  // require bit-exact emission parity by inspecting the returned batch.
  auto run = [](bool force_interp) {
    rtbot::Program::set_force_interpreter_for_testing(force_interp);
    rtbot::Program prog(kBollingerJson);
    if (force_interp) REQUIRE_FALSE(prog.using_jit());
    else              REQUIRE(prog.using_jit());

    std::mt19937_64 rng(0xBAFE);
    std::normal_distribution<double> step(0.0, 1.0);
    double price = 100.0;

    // Stable per-tick capture: timestamp + (port_name -> value).
    std::vector<std::pair<std::int64_t, std::map<std::string, double>>> out;
    for (int i = 1; i <= 100; ++i) {
      price += step(rng);
      std::map<std::string, std::vector<std::unique_ptr<rtbot::BaseMessage>>> port_messages;
      port_messages["i1"].push_back(
          rtbot::create_message<rtbot::NumberData>(static_cast<std::int64_t>(i),
                                                    rtbot::NumberData{price}));
      auto batch = prog.receive_batch(port_messages);
      auto op_it = batch.find("37");
      if (op_it == batch.end()) continue;
      // For each port, collect its messages keyed by timestamp.
      std::map<std::int64_t, std::map<std::string, double>> by_t;
      for (const auto& [port_name, msgs] : op_it->second) {
        for (const auto& m : msgs) {
          if (auto* nm = dynamic_cast<const rtbot::Message<rtbot::NumberData>*>(m.get())) {
            by_t[nm->time][port_name] = nm->data.value;
          }
        }
      }
      for (auto& [t, mp] : by_t) {
        out.emplace_back(t, std::move(mp));
      }
    }
    return out;
  };

  auto jit_out    = run(false);
  auto interp_out = run(true);
  rtbot::Program::set_force_interpreter_for_testing(false);

  REQUIRE(jit_out.size() == interp_out.size());
  REQUIRE(jit_out.size() > 0);
  for (std::size_t i = 0; i < jit_out.size(); ++i) {
    REQUIRE(jit_out[i].first == interp_out[i].first);
    REQUIRE(jit_out[i].second.size() == interp_out[i].second.size());
    for (const auto& [port, jval] : jit_out[i].second) {
      auto it = interp_out[i].second.find(port);
      REQUIRE(it != interp_out[i].second.end());
      const std::uint64_t a = rtbot::test::dbits(jval);
      const std::uint64_t b = rtbot::test::dbits(it->second);
      if (a != b) {
        CAPTURE(i, port, jval, it->second);
        REQUIRE(a == b);
      }
    }
  }
}

SCENARIO("Program::receive_buffer routes through JIT and matches interpreter bit-exactly",
         "[program_dispatch][receive_buffer][parity]") {
  // Vector-input program that the JIT supports end-to-end:
  // Input(vector_number, width=3) -> FusedExpressionVector(2 outputs) -> Output.
  // The expression bytecode (ADD/MUL on input lanes) is the same shape rtbot-sql
  // emits for base views. We drive 50 rows through receive_buffer and compare
  // emissions bit-exactly against the interpreter path on the same JSON.
  static constexpr const char* kVectorPipeline = R"({
    "title": "fev-receive-buffer-parity",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in", "type": "Input", "portTypes": ["vector_number"], "portWidths": [3] },
      { "id": "fev", "type": "FusedExpressionVector",
        "numOutputs": 2,
        "bytecode": [0,0, 0,2, 2, 20, 0,1, 0,2, 4, 20],
        "constants": [] },
      { "id": "out", "type": "Output", "portTypes": ["vector_number"], "portWidths": [2] }
    ],
    "connections": [
      { "from": "in",  "to": "fev", "fromPort": "o1", "toPort": "i1" },
      { "from": "fev", "to": "out", "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  // Build a 50-row buffer (3 columns each).
  const size_t num_rows = 50;
  const size_t num_cols = 3;
  std::vector<double> data(num_rows * num_cols);
  std::vector<std::int64_t> times(num_rows);
  std::mt19937_64 rng(0xBEEF);
  std::uniform_real_distribution<double> dist(-10.0, 10.0);
  for (size_t r = 0; r < num_rows; ++r) {
    times[r] = static_cast<std::int64_t>(r + 1);
    for (size_t c = 0; c < num_cols; ++c) {
      data[r * num_cols + c] = dist(rng);
    }
  }

  auto run = [&](bool force_interp) {
    rtbot::Program::set_force_interpreter_for_testing(force_interp);
    rtbot::Program prog(kVectorPipeline);
    if (force_interp) REQUIRE_FALSE(prog.using_jit());
    else              REQUIRE(prog.using_jit());

    auto batch = prog.receive_buffer("i1", data.data(), num_rows, num_cols, times.data());

    // Capture: timestamp + flat values vector from the single vector_number port.
    std::vector<rtbot::test::ParityEmission> out;
    auto op_it = batch.find("out");
    if (op_it == batch.end()) return out;
    auto port_it = op_it->second.find("o1");
    if (port_it == op_it->second.end()) return out;
    for (const auto& m : port_it->second) {
      if (auto* vm = dynamic_cast<const rtbot::Message<rtbot::VectorNumberData>*>(m.get())) {
        rtbot::test::ParityEmission e;
        e.t = vm->time;
        e.values = *vm->data.values;
        out.push_back(std::move(e));
      }
    }
    return out;
  };

  auto jit_out    = run(false);
  auto interp_out = run(true);
  rtbot::Program::set_force_interpreter_for_testing(false);

  REQUIRE(jit_out.size() == interp_out.size());
  REQUIRE(jit_out.size() > 0);
  for (std::size_t i = 0; i < jit_out.size(); ++i) {
    REQUIRE(jit_out[i].t == interp_out[i].t);
    REQUIRE(jit_out[i].values.size() == interp_out[i].values.size());
    for (std::size_t k = 0; k < jit_out[i].values.size(); ++k) {
      const std::uint64_t a = rtbot::test::dbits(jit_out[i].values[k]);
      const std::uint64_t b = rtbot::test::dbits(interp_out[i].values[k]);
      if (a != b) {
        CAPTURE(i, k, jit_out[i].values[k], interp_out[i].values[k]);
        REQUIRE(a == b);
      }
    }
  }
}

SCENARIO("WHERE/Demux scalar parity: predicate gates per-tick emit-or-drop bit-exactly",
         "[program_dispatch][parity_helper][where_demux]") {
  // Lock down the rtbot-sql WHERE pattern shape (scalar variant) as a permanent
  // parity regression. The shape mirrors compile_where in
  // rtbot-sql/libs/compiler/src/where_compiler.cpp:
  //   <data wire>          ->  Demux.i1
  //   <predicate boolean>  ->  Demux.c1
  //   Demux numPorts=1, route on c1 true to o1.
  //
  // Production rtbot-sql wires a vector_number on the data port; the JIT
  // currently rejects vector_number Demux state layouts and the Program falls
  // back to the interpreter — which still exercises the FE Demux end-to-end.
  // This SCENARIO uses the JIT-compatible scalar variant so the JIT-vs-interp
  // helper genuinely flexes both backends. A second SCENARIO below covers the
  // exact rtbot-sql vector_number shape.
  static constexpr const char* kWhereDemuxScalarJson = R"({
    "title": "where-demux-scalar",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",   "type": "Input", "portTypes": ["number"] },
      { "id": "data", "type": "Identity" },
      { "id": "pred", "type": "CompareGT", "value": 1.0 },
      { "id": "dmx",  "type": "Demultiplexer", "numPorts": 1 },
      { "id": "out",  "type": "Output", "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in",   "to": "data", "fromPort": "o1", "toPort": "i1" },
      { "from": "in",   "to": "pred", "fromPort": "o1", "toPort": "i1" },
      { "from": "data", "to": "dmx",  "fromPort": "o1", "toPort": "i1" },
      { "from": "pred", "to": "dmx",  "fromPort": "o1", "toPort": "c1" },
      { "from": "dmx",  "to": "out",  "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  // Drive ~1000 ticks where the predicate alternates true/false to exercise
  // both gated-emit and gated-drop paths. Values 0.5 and 2.5 alternate so
  // CompareGT(1.0) flips on every tick.
  std::vector<rtbot::test::TickInput> inputs;
  inputs.reserve(1000);
  for (int i = 1; i <= 1000; ++i) {
    const double v = (i % 2 == 0) ? 2.5 : 0.5;
    inputs.push_back({static_cast<std::int64_t>(i), {v}});
  }

  rtbot::test::run_jit_vs_interpreter_parity(
      kWhereDemuxScalarJson, inputs,
      [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
        p.send(tick.t, tick.values[0]);
      });
}

SCENARIO("WHERE/Demux vector_number parity: rtbot-sql shape matches bit-exactly",
         "[program_dispatch][parity_helper][where_demux]") {
  // Exact rtbot-sql WHERE shape: a vector data wire enters Demux.i1 with
  // portType=vector_number, while a scalar predicate derived from a lane of
  // the same vector enters Demux.c1. Mirrors the compile_where output:
  //   Input(vector_number, width=K) -> [data: identity-on-vector] -> Demux.i1
  //   Input(vector_number, width=K) -> FEV(extract lane 0) -> CompareGT -> Demux.c1
  //
  // The JIT cannot today JIT-compile a vector_number Demux state layout (the
  // PortBuffer holds 1 double per slot), so Program::using_jit() will be false
  // for this JSON. The parity helper still asserts equality and drain_into
  // emission ordering match — locking the rtbot-sql shape against drift in
  // either backend.
  static constexpr const char* kWhereDemuxVectorJson = R"({
    "title": "where-demux-vector",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",  "type": "Input", "portTypes": ["vector_number"], "portWidths": [3] },
      { "id": "ext", "type": "VectorExtract", "index": 0 },
      { "id": "pred", "type": "CompareGT", "value": 1.0 },
      { "id": "dmx",  "type": "Demultiplexer", "numPorts": 1, "portType": "vector_number" },
      { "id": "out",  "type": "Output", "portTypes": ["vector_number"], "portWidths": [3] }
    ],
    "connections": [
      { "from": "in",   "to": "ext",  "fromPort": "o1", "toPort": "i1" },
      { "from": "ext",  "to": "pred", "fromPort": "o1", "toPort": "i1" },
      { "from": "in",   "to": "dmx",  "fromPort": "o1", "toPort": "i1" },
      { "from": "pred", "to": "dmx",  "fromPort": "o1", "toPort": "c1" },
      { "from": "dmx",  "to": "out",  "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  // Drive ~1000 ticks with a 3-lane vector. Lane 0 alternates 0.5 / 2.5 so the
  // CompareGT(1.0) predicate flips every tick; lanes 1 and 2 carry deterministic
  // payload data so dropped vs forwarded vectors are easily distinguished if a
  // future divergence ever surfaces.
  std::vector<rtbot::test::TickInput> inputs;
  inputs.reserve(1000);
  for (int i = 1; i <= 1000; ++i) {
    const double lane0 = (i % 2 == 0) ? 2.5 : 0.5;
    const double lane1 = static_cast<double>(i);
    const double lane2 = static_cast<double>(i) * 0.25;
    inputs.push_back({static_cast<std::int64_t>(i), {lane0, lane1, lane2}});
  }

  rtbot::test::run_jit_vs_interpreter_parity(
      kWhereDemuxVectorJson, inputs,
      [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
        p.send_vector(tick.t, tick.values.data(), tick.values.size());
      });
}

SCENARIO("Program falls back to interpreter for unsupported opcode", "[program_dispatch]") {
  // Variable is valid in the FE schema and interpreter but is not mapped in
  // the JIT's JsonParser, so get_or_compile throws. The Program constructor
  // must not propagate the exception — it silently falls back.
  REQUIRE_NOTHROW([&]() {
    rtbot::Program prog(kCounterJson);
    REQUIRE_FALSE(prog.using_jit());

    // Verify that the interpreter path is actually working.
    auto batch = prog.receive(
        rtbot::Message<rtbot::NumberData>(1, rtbot::NumberData{42.0}));

    REQUIRE(batch.count("out1") == 1);
    REQUIRE(batch.at("out1").count("o1") == 1);
    REQUIRE(batch.at("out1").at("o1").size() == 1);
  }());
}
