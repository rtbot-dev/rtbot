// test_tier2c_ops_parity.cpp
//
// Bit-exact parity tests for tier-2C JIT IR emitters (RB-491):
//   ResamplerConstant      — zero-order-hold resampler with optional t0 / snap_first
//   MinTracker / MaxTracker — running unbounded min/max (already wired via MinAgg/MaxAgg)
//
// Each test drives the same input stream through the FE interpreter pipeline
// (Program / direct operator) and the JIT-compiled program, then asserts
// bit-exact (time, value) equality.

#include <catch2/catch.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"
#include "rtbot/std/MinMaxTracker.h"
#include "rtbot/std/ResamplerConstant.h"
#include "rtbot/compiled/jit/JitCompiler.h"

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

struct Sample {
  std::int64_t t;
  double v;
};

struct Emit {
  std::int64_t t;
  double v;
};

std::vector<Emit> run_jit(const std::string& json,
                          const std::vector<Sample>& inputs) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(json);
  REQUIRE(prog != nullptr);
  for (const auto& s : inputs) prog->receive(s.t, s.v);
  std::vector<Emit> out;
  for (const auto& r : prog->collect_outputs()) {
    REQUIRE(r.values.size() == 1);
    out.push_back({r.time, r.values[0]});
  }
  return out;
}

std::vector<Emit> drain_collector_number(rtbot::Collector& sink) {
  std::vector<Emit> out;
  auto& q = sink.get_data_queue(0);
  for (auto& msg_ptr : q) {
    const auto* msg =
        static_cast<const rtbot::Message<rtbot::NumberData>*>(msg_ptr.get());
    out.push_back({msg->time, msg->data.value});
  }
  q.clear();
  return out;
}

void require_parity(const std::vector<Emit>& jit_out,
                    const std::vector<Emit>& fe_out) {
  REQUIRE(jit_out.size() == fe_out.size());
  for (std::size_t i = 0; i < jit_out.size(); ++i) {
    INFO("emit index " << i
         << " jit=(" << jit_out[i].t << ", " << jit_out[i].v << ")"
         << " fe=("  << fe_out[i].t  << ", " << fe_out[i].v  << ")");
    REQUIRE(jit_out[i].t == fe_out[i].t);
    REQUIRE(dbits(jit_out[i].v) == dbits(fe_out[i].v));
  }
}

// ------- inputs ------------------------------------------------------------

// Irregularly-spaced 200-tick stream with capped step <= max_step (caller
// chooses to keep step <= resampler interval so the JIT framework's
// 1-emit-per-receive limit is not stressed — see parity-test note below).
std::vector<Sample> make_irregular_stream(std::int64_t seed,
                                           std::int64_t max_step,
                                           std::size_t n = 200) {
  std::vector<Sample> inputs;
  inputs.reserve(n);
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(-100.0, 100.0);
  std::uniform_int_distribution<std::int64_t> dt_dist(1, max_step);
  std::int64_t t = 1;
  for (std::size_t i = 0; i < n; ++i) {
    inputs.push_back({t, dist(rng)});
    t += dt_dist(rng);
  }
  return inputs;
}

// Regularly-spaced 200-tick stream at step 1 (densest case).
std::vector<Sample> make_dense_stream(std::int64_t seed = 0xCAFEULL,
                                       std::size_t n = 200) {
  std::vector<Sample> inputs;
  inputs.reserve(n);
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(-100.0, 100.0);
  for (std::size_t i = 0; i < n; ++i) {
    inputs.push_back({static_cast<std::int64_t>(i + 1), dist(rng)});
  }
  return inputs;
}

// NOTE: ResamplerConstant in FE buffers every grid emission and returns them
// all from collect_outputs. The JIT framework's segment_process() returns at
// most one emission per receive() call. To keep the count-equal parity check
// meaningful we keep input gaps <= resampler interval — at most one grid
// point can fall in any input gap.

}  // namespace

// ===========================================================================
// ResamplerConstant
// ===========================================================================

namespace {

// Build a JIT pipeline: Input -> ResamplerConstant -> Output.
// Optional t0 and snapFirst params.
std::string make_rc_json(std::int64_t interval,
                          bool t0_set, std::int64_t t0,
                          bool snap_first) {
  std::string json;
  json  = R"({"title":"t","apiVersion":"v1","entryOperator":"in",)";
  json += R"("output":{"out":["o1"]},"operators":[)";
  json += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  json += R"({"id":"rc","type":"ResamplerConstant","interval":)";
  json += std::to_string(interval);
  if (t0_set) {
    json += R"(,"t0":)";
    json += std::to_string(static_cast<double>(t0));
  }
  if (snap_first) {
    json += R"(,"snapFirst":1)";
  }
  json += "},";
  json += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  json += R"("connections":[)";
  json += R"({"from":"in","to":"rc","fromPort":"o1","toPort":"i1"},)";
  json += R"({"from":"rc","to":"out","fromPort":"o1","toPort":"i1"}]})";
  return json;
}

std::vector<Emit> run_fe_rc(std::int64_t interval, bool t0_set,
                             std::int64_t t0, bool snap_first,
                             const std::vector<Sample>& inputs) {
  std::shared_ptr<rtbot::ResamplerConstant<rtbot::NumberData>> rc;
  if (t0_set) {
    rc = std::make_shared<rtbot::ResamplerConstant<rtbot::NumberData>>(
        "rc", interval, std::optional<rtbot::timestamp_t>(t0), snap_first);
  } else {
    rc = std::make_shared<rtbot::ResamplerConstant<rtbot::NumberData>>(
        "rc", interval, std::nullopt, snap_first);
  }
  auto sink = std::make_shared<rtbot::Collector>(
      "sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  rc->connect(sink, 0, 0);
  for (const auto& s : inputs) {
    rc->receive_data(rtbot::create_message<rtbot::NumberData>(
                          s.t, rtbot::NumberData{s.v}),
                      0);
  }
  rc->execute();
  return drain_collector_number(*sink);
}

}  // namespace

SCENARIO("JIT ResamplerConstant matches FE bit-exactly (no t0)",
         "[tier2c][resampler_constant]") {
  // Dense input (step=1); various intervals (>= step → at most 1 emit/tick).
  auto inputs = make_dense_stream(0xCAFEULL);

  for (std::int64_t interval : {1, 3, 7, 11}) {
    INFO("interval=" << interval);
    auto json = make_rc_json(interval, /*t0_set=*/false, 0,
                              /*snap_first=*/false);
    auto jit_out = run_jit(json, inputs);
    auto fe_out  = run_fe_rc(interval, false, 0, false, inputs);
    require_parity(jit_out, fe_out);
  }
}

SCENARIO("JIT ResamplerConstant matches FE bit-exactly with t0",
         "[tier2c][resampler_constant][t0]") {
  // Irregular stream with max_step kept <= smallest interval below.
  auto inputs = make_irregular_stream(0xBABE, /*max_step=*/3);

  struct Case { std::int64_t interval; std::int64_t t0; };
  // All intervals >= 3 (max input step) so JIT emits >= FE.
  Case cases[] = { {5, 0}, {10, 50}, {7, 0}, {3, 100} };

  for (const auto& c : cases) {
    INFO("interval=" << c.interval << " t0=" << c.t0);
    auto json = make_rc_json(c.interval, /*t0_set=*/true, c.t0,
                              /*snap_first=*/false);
    auto jit_out = run_jit(json, inputs);
    auto fe_out  = run_fe_rc(c.interval, true, c.t0, false, inputs);
    require_parity(jit_out, fe_out);
  }
}

SCENARIO("JIT ResamplerConstant matches FE bit-exactly with snap_first",
         "[tier2c][resampler_constant][snap_first]") {
  auto inputs = make_irregular_stream(0xFADE, /*max_step=*/3);

  struct Case { std::int64_t interval; std::int64_t t0; };
  Case cases[] = { {10, 0}, {5, 0}, {7, 50}, {3, 1} };

  for (const auto& c : cases) {
    INFO("interval=" << c.interval << " t0=" << c.t0 << " snap_first=true");
    auto json = make_rc_json(c.interval, /*t0_set=*/true, c.t0,
                              /*snap_first=*/true);
    auto jit_out = run_jit(json, inputs);
    auto fe_out  = run_fe_rc(c.interval, true, c.t0, true, inputs);
    require_parity(jit_out, fe_out);
  }
}

SCENARIO("JIT ResamplerConstant matches FE on dense regular streams",
         "[tier2c][resampler_constant][dense]") {
  // Dense (step=1) stream stresses the on-grid emit + value latching paths.
  auto inputs = make_dense_stream(0xC0FFEEULL);

  for (std::int64_t interval : {1, 5, 17}) {
    INFO("interval=" << interval << " dense stream");
    {
      auto json = make_rc_json(interval, false, 0, false);
      auto jit_out = run_jit(json, inputs);
      auto fe_out  = run_fe_rc(interval, false, 0, false, inputs);
      require_parity(jit_out, fe_out);
    }
    {
      auto json = make_rc_json(interval, true, 0, true);
      auto jit_out = run_jit(json, inputs);
      auto fe_out  = run_fe_rc(interval, true, 0, true, inputs);
      require_parity(jit_out, fe_out);
    }
  }
}

// ===========================================================================
// MinTracker / MaxTracker (JSON-level parity — wired via MinAgg/MaxAgg)
// ===========================================================================

namespace {

std::string make_tracker_json(const std::string& type) {
  std::string json;
  json  = R"({"title":"t","apiVersion":"v1","entryOperator":"in",)";
  json += R"("output":{"out":["o1"]},"operators":[)";
  json += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  json += R"({"id":"op","type":")" + type + R"("},)";
  json += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  json += R"("connections":[)";
  json += R"({"from":"in","to":"op","fromPort":"o1","toPort":"i1"},)";
  json += R"({"from":"op","to":"out","fromPort":"o1","toPort":"i1"}]})";
  return json;
}

}  // namespace

SCENARIO("JIT MinTracker / MaxTracker match FE bit-exactly",
         "[tier2c][min_max_tracker]") {
  std::vector<Sample> inputs;
  inputs.reserve(150);
  std::mt19937_64 rng(0xDEADBEEFULL);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);
  for (std::int64_t i = 1; i <= 150; ++i) {
    double v = dist(rng);
    if (i == 17) v = std::numeric_limits<double>::quiet_NaN();
    if (i == 42) v = std::numeric_limits<double>::infinity();
    if (i == 73) v = -std::numeric_limits<double>::infinity();
    inputs.push_back({i, v});
  }

  // MinTracker
  {
    INFO("MinTracker");
    auto jit_out = run_jit(make_tracker_json("MinTracker"), inputs);

    auto op   = rtbot::make_min_tracker("op");
    auto sink = std::make_shared<rtbot::Collector>(
        "sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(sink, 0, 0);
    for (const auto& s : inputs) {
      op->receive_data(rtbot::create_message<rtbot::NumberData>(
                            s.t, rtbot::NumberData{s.v}),
                        0);
    }
    op->execute();
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }

  // MaxTracker
  {
    INFO("MaxTracker");
    auto jit_out = run_jit(make_tracker_json("MaxTracker"), inputs);

    auto op   = rtbot::make_max_tracker("op");
    auto sink = std::make_shared<rtbot::Collector>(
        "sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(sink, 0, 0);
    for (const auto& s : inputs) {
      op->receive_data(rtbot::create_message<rtbot::NumberData>(
                            s.t, rtbot::NumberData{s.v}),
                        0);
    }
    op->execute();
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }
}
