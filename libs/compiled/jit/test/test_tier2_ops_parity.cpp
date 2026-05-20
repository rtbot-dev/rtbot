// test_tier2_ops_parity.cpp
//
// Bit-exact parity tests for tier-2 JIT IR emitters (RB-491):
//   ArithmeticScalar (Add, Power, Sin, Cos, Tan, Exp, Log, Log10, Abs, Sign,
//                     Floor, Ceil, Round)
//   ArithmeticSync   (Addition, Subtraction, Multiplication, Division)
//   CompareScalar    (CompareGT, CompareLT, CompareGTE, CompareLTE,
//                     CompareEQ, CompareNEQ)
//   CompareSync      (CompareSyncGT, CompareSyncLT, CompareSyncGTE,
//                     CompareSyncLTE, CompareSyncEQ, CompareSyncNEQ)
//   BooleanSync      (LogicalAnd, LogicalOr, LogicalXor, LogicalNand,
//                     LogicalNor, LogicalXnor, LogicalImplication)
//   FilterScalar     (LessThan, GreaterThan, EqualTo, NotEqualTo)
//   FilterSync       (SyncGreaterThan, SyncLessThan, SyncEqual, SyncNotEqual)
//
// Each test drives the same input stream through the FE interpreter pipeline
// and the JIT-compiled program, then asserts bit-exact (time, value) equality.

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
#include "rtbot/std/ArithmeticScalar.h"
#include "rtbot/std/ArithmeticSync.h"
#include "rtbot/std/BooleanSync.h"
#include "rtbot/std/BooleanToNumber.h"
#include "rtbot/std/CompareScalar.h"
#include "rtbot/std/CompareSync.h"
#include "rtbot/std/FilterScalar.h"
#include "rtbot/std/FilterSync.h"
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
    const auto* msg = static_cast<const rtbot::Message<rtbot::NumberData>*>(msg_ptr.get());
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

// 100-sample stream with a NaN and +/-Inf sprinkled in.
std::vector<Sample> make_inputs() {
  std::vector<Sample> inputs;
  inputs.reserve(100);
  std::mt19937_64 rng(0xCAFED00DULL);
  std::uniform_real_distribution<double> dist(-100.0, 100.0);
  for (std::int64_t i = 1; i <= 100; ++i) {
    double v = dist(rng);
    if (i == 17) v = std::numeric_limits<double>::quiet_NaN();
    if (i == 42) v = std::numeric_limits<double>::infinity();
    if (i == 73) v = -std::numeric_limits<double>::infinity();
    inputs.push_back({i, v});
  }
  return inputs;
}

// Positive-only stream useful for Log/Sqrt/Power-with-fractional-exp.
std::vector<Sample> make_positive_inputs() {
  std::vector<Sample> inputs;
  std::mt19937_64 rng(0xC0DECAFEULL);
  std::uniform_real_distribution<double> dist(0.1, 100.0);
  for (std::int64_t i = 1; i <= 100; ++i) {
    inputs.push_back({i, dist(rng)});
  }
  return inputs;
}

// Drive 2-input synced operator with (a, b) pairs aligned by timestamp.
void drive_sync_op(const std::shared_ptr<rtbot::Operator>& op,
                   const std::vector<Sample>& a,
                   const std::vector<Sample>& b) {
  for (std::size_t i = 0; i < a.size(); ++i) {
    op->receive_data(rtbot::create_message<rtbot::NumberData>(
                          a[i].t, rtbot::NumberData{a[i].v}),
                      0);
    op->receive_data(rtbot::create_message<rtbot::NumberData>(
                          b[i].t, rtbot::NumberData{b[i].v}),
                      1);
  }
  op->execute();
}

// Generic helper: build a JSON for a 1-input scalar op
// (Input -> op -> Output) and run + parity-check vs an FE op.
std::vector<Emit> run_scalar_jit(const std::string& op_type, double value,
                                  const std::vector<Sample>& inputs,
                                  bool include_value = true) {
  std::string json;
  json  = R"({"title":"t","apiVersion":"v1","entryOperator":"in",)";
  json += R"("output":{"out":["o1"]},"operators":[)";
  json += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  if (include_value) {
    json += R"({"id":"op","type":")" + op_type +
            R"(","value":)" + std::to_string(value) + "},";
  } else {
    json += R"({"id":"op","type":")" + op_type + R"("},)";
  }
  json += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  json += R"("connections":[)";
  json += R"({"from":"in","to":"op","fromPort":"o1","toPort":"i1"},)";
  json += R"({"from":"op","to":"out","fromPort":"o1","toPort":"i1"}]})";
  return run_jit(json, inputs);
}

// JSON for a 2-input synced op (in1, in2 -> op -> out).
// The JIT input always has 1 number port; the op has its own 2 ports.
// We model this by using two separate "Input" emissions through the same Input
// op, then a Join/Sync. But the JIT emit_program path doesn't support arbitrary
// two-input pipelines without a pre-Input split. So we instead test sync ops
// where one input comes from Input and the other from a Constant.
// Alternative: model via a TimeShift/Identity to provide a second number stream.
// For sync 2-input parity, the simplest pipeline is:
//   Input --o1--> opA --o1--> joinPort1 of op
//   Input --o1--> opB --o1--> joinPort2 of op
// where opA is Identity and opB is e.g. Add(value=k) so the second stream is
// (t, v + k).
std::string make_sync_2in_json(const std::string& op_type,
                                const std::string& extra_params,
                                double k_for_second) {
  std::string json;
  json  = R"({"title":"t","apiVersion":"v1","entryOperator":"in",)";
  json += R"("output":{"out":["o1"]},"operators":[)";
  json += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  json += R"({"id":"a","type":"Identity"},)";
  json += R"({"id":"b","type":"Add","value":)" + std::to_string(k_for_second) + "},";
  json += R"({"id":"op","type":")" + op_type + R"(")";
  if (!extra_params.empty()) json += "," + extra_params;
  json += "},";
  json += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  json += R"("connections":[)";
  json += R"({"from":"in","to":"a","fromPort":"o1","toPort":"i1"},)";
  json += R"({"from":"in","to":"b","fromPort":"o1","toPort":"i1"},)";
  json += R"({"from":"a","to":"op","fromPort":"o1","toPort":"i1"},)";
  json += R"({"from":"b","to":"op","fromPort":"o1","toPort":"i2"},)";
  json += R"({"from":"op","to":"out","fromPort":"o1","toPort":"i1"}]})";
  return json;
}

}  // namespace

// ===========================================================================
// ArithmeticScalar
// ===========================================================================
SCENARIO("JIT Add (scalar) matches FE bit-exactly", "[tier2][arithmetic][add_scalar]") {
  auto inputs = make_inputs();
  auto jit_out = run_scalar_jit("Add", 3.5, inputs, /*include_value=*/true);

  auto op   = rtbot::make_add("op", 3.5);
  auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  op->connect(sink, 0, 0);
  for (const auto& s : inputs) {
    op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
  }
  op->execute();
  auto fe_out = drain_collector_number(*sink);
  require_parity(jit_out, fe_out);
}

SCENARIO("JIT scalar transcendental ops match FE bit-exactly",
         "[tier2][arithmetic][transcendental]") {
  // Drive each unary op (Sin/Cos/Tan/Exp/Log/Log10/Abs/Sign/Floor/Ceil/Round)
  // through both pipelines and require parity. Power needs a constant exponent.
  auto inputs       = make_inputs();
  auto pos_inputs   = make_positive_inputs();

  struct UnaryCase {
    const char* type;
    std::shared_ptr<rtbot::Operator> (*make)(std::string);
    bool positive_only;
  };

  UnaryCase cases[] = {
    {"Sin",   [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_sin(std::move(id))); },   false},
    {"Cos",   [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_cos(std::move(id))); },   false},
    {"Tan",   [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_tan(std::move(id))); },   false},
    {"Exp",   [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_exp(std::move(id))); },   false},
    {"Log",   [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_log(std::move(id))); },   true },
    {"Log10", [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_log10(std::move(id))); }, true },
    {"Abs",   [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_abs(std::move(id))); },   false},
    {"Sign",  [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_sign(std::move(id))); },  false},
    {"Floor", [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_floor(std::move(id))); }, false},
    {"Ceil",  [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_ceil(std::move(id))); },  false},
    {"Round", [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_round(std::move(id))); }, false},
  };

  for (const auto& c : cases) {
    INFO("scalar op type=" << c.type);
    const auto& xs = c.positive_only ? pos_inputs : inputs;
    auto jit_out = run_scalar_jit(c.type, 0.0, xs, /*include_value=*/false);

    auto op   = c.make("op");
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(sink, 0, 0);
    for (const auto& s : xs) {
      op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
    }
    op->execute();
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }
}

SCENARIO("JIT Power (scalar) matches FE bit-exactly", "[tier2][arithmetic][power_scalar]") {
  auto inputs = make_positive_inputs();   // avoid pow(neg, frac) NaN
  auto jit_out = run_scalar_jit("Power", 1.5, inputs, /*include_value=*/true);

  auto op   = rtbot::make_power("op", 1.5);
  auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  op->connect(sink, 0, 0);
  for (const auto& s : inputs) {
    op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
  }
  op->execute();
  auto fe_out = drain_collector_number(*sink);
  require_parity(jit_out, fe_out);
}

// ===========================================================================
// ArithmeticSync (Addition / Subtraction / Multiplication / Division)
// ===========================================================================
SCENARIO("JIT Addition/Subtraction/Multiplication/Division (sync) match FE bit-exactly",
         "[tier2][arithmetic][sync]") {
  auto inputs = make_inputs();   // a stream
  // b stream = a + 7.25 (so divisions by zero won't happen for non-special inputs)
  std::vector<Sample> b_inputs;
  for (const auto& s : inputs) b_inputs.push_back({s.t, s.v + 7.25});

  struct SyncCase {
    const char* type;
    std::shared_ptr<rtbot::Operator> (*make)(std::string, std::size_t);
  };
  SyncCase cases[] = {
    {"Addition",       [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_addition(std::move(id), n)); }},
    {"Subtraction",    [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_subtraction(std::move(id), n)); }},
    {"Multiplication", [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_multiplication(std::move(id), n)); }},
    {"Division",       [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_division(std::move(id), n)); }},
  };

  for (const auto& c : cases) {
    INFO("sync arithmetic type=" << c.type);
    std::string json = make_sync_2in_json(c.type, "", 7.25);
    auto jit_out = run_jit(json, inputs);

    auto op   = c.make("op", 2);
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(sink, 0, 0);
    drive_sync_op(op, inputs, b_inputs);
    auto fe_out = drain_collector_number(*sink);

    require_parity(jit_out, fe_out);
  }
}

// ===========================================================================
// CompareScalar — emits BooleanData in FE; we wrap with BooleanToNumber so
// both pipelines emit doubles (0.0/1.0) for bit-exact comparison.
// ===========================================================================
SCENARIO("JIT CompareScalar GT/LT/GTE/LTE match FE bit-exactly",
         "[tier2][compare_scalar][rel]") {
  auto inputs = make_inputs();

  struct CompCase {
    const char* type;
    std::shared_ptr<rtbot::Operator> (*make)(std::string, double);
  };
  CompCase cases[] = {
    {"CompareGT",  [](std::string id, double v){ return std::shared_ptr<rtbot::Operator>(rtbot::make_compare_gt(std::move(id), v)); }},
    {"CompareLT",  [](std::string id, double v){ return std::shared_ptr<rtbot::Operator>(rtbot::make_compare_lt(std::move(id), v)); }},
    {"CompareGTE", [](std::string id, double v){ return std::shared_ptr<rtbot::Operator>(rtbot::make_compare_gte(std::move(id), v)); }},
    {"CompareLTE", [](std::string id, double v){ return std::shared_ptr<rtbot::Operator>(rtbot::make_compare_lte(std::move(id), v)); }},
  };

  for (const auto& c : cases) {
    INFO("compare scalar type=" << c.type);
    auto jit_out = run_scalar_jit(c.type, 7.5, inputs, /*include_value=*/true);

    auto op   = c.make("op", 7.5);
    auto b2n  = rtbot::make_boolean_to_number("b2n");
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(b2n, 0, 0);
    b2n->connect(sink, 0, 0);
    for (const auto& s : inputs) {
      op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
    }
    op->execute();
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }
}

SCENARIO("JIT CompareScalar EQ/NEQ (with tolerance) match FE bit-exactly",
         "[tier2][compare_scalar][eq]") {
  auto inputs = make_inputs();

  // EQ
  {
    INFO("CompareEQ");
    std::string json =
        R"({"title":"t","apiVersion":"v1","entryOperator":"in","output":{"out":["o1"]},"operators":[)"
        R"({"id":"in","type":"Input","portTypes":["number"]},)"
        R"({"id":"op","type":"CompareEQ","value":7.5,"tolerance":0.5},)"
        R"({"id":"out","type":"Output","portTypes":["number"]}],)"
        R"("connections":[{"from":"in","to":"op","fromPort":"o1","toPort":"i1"},)"
        R"({"from":"op","to":"out","fromPort":"o1","toPort":"i1"}]})";
    auto jit_out = run_jit(json, inputs);

    auto op   = rtbot::make_compare_eq("op", 7.5, 0.5);
    auto b2n  = rtbot::make_boolean_to_number("b2n");
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(b2n, 0, 0);
    b2n->connect(sink, 0, 0);
    for (const auto& s : inputs) {
      op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
    }
    op->execute();
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }

  // NEQ
  {
    INFO("CompareNEQ");
    std::string json =
        R"({"title":"t","apiVersion":"v1","entryOperator":"in","output":{"out":["o1"]},"operators":[)"
        R"({"id":"in","type":"Input","portTypes":["number"]},)"
        R"({"id":"op","type":"CompareNEQ","value":7.5,"tolerance":0.5},)"
        R"({"id":"out","type":"Output","portTypes":["number"]}],)"
        R"("connections":[{"from":"in","to":"op","fromPort":"o1","toPort":"i1"},)"
        R"({"from":"op","to":"out","fromPort":"o1","toPort":"i1"}]})";
    auto jit_out = run_jit(json, inputs);

    auto op   = rtbot::make_compare_neq("op", 7.5, 0.5);
    auto b2n  = rtbot::make_boolean_to_number("b2n");
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(b2n, 0, 0);
    b2n->connect(sink, 0, 0);
    for (const auto& s : inputs) {
      op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
    }
    op->execute();
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }
}

// ===========================================================================
// CompareSync (2-input, emits BooleanData -> wrap with BooleanToNumber)
// ===========================================================================
SCENARIO("JIT CompareSync GT/LT/GTE/LTE match FE bit-exactly",
         "[tier2][compare_sync][rel]") {
  auto inputs = make_inputs();
  std::vector<Sample> b_inputs;
  for (const auto& s : inputs) b_inputs.push_back({s.t, s.v + 7.25});

  struct CompCase {
    const char* type;
    std::shared_ptr<rtbot::Operator> (*make)(std::string);
  };
  CompCase cases[] = {
    {"CompareSyncGT",  [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_compare_sync_gt(std::move(id))); }},
    {"CompareSyncLT",  [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_compare_sync_lt(std::move(id))); }},
    {"CompareSyncGTE", [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_compare_sync_gte(std::move(id))); }},
    {"CompareSyncLTE", [](std::string id){ return std::shared_ptr<rtbot::Operator>(rtbot::make_compare_sync_lte(std::move(id))); }},
  };
  for (const auto& c : cases) {
    INFO("compare sync type=" << c.type);
    std::string json = make_sync_2in_json(c.type, "", 7.25);
    auto jit_out = run_jit(json, inputs);

    auto op   = c.make("op");
    auto b2n  = rtbot::make_boolean_to_number("b2n");
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(b2n, 0, 0);
    b2n->connect(sink, 0, 0);
    drive_sync_op(op, inputs, b_inputs);
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }
}

SCENARIO("JIT CompareSync EQ/NEQ (with tolerance) match FE bit-exactly",
         "[tier2][compare_sync][eq]") {
  auto inputs = make_inputs();
  std::vector<Sample> b_inputs;
  for (const auto& s : inputs) b_inputs.push_back({s.t, s.v + 0.25});

  // EQ
  {
    std::string json = make_sync_2in_json("CompareSyncEQ", R"("tolerance":0.5)", 0.25);
    auto jit_out = run_jit(json, inputs);
    auto op   = rtbot::make_compare_sync_eq("op", 0.5);
    auto b2n  = rtbot::make_boolean_to_number("b2n");
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(b2n, 0, 0);
    b2n->connect(sink, 0, 0);
    drive_sync_op(op, inputs, b_inputs);
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }

  // NEQ
  {
    std::string json = make_sync_2in_json("CompareSyncNEQ", R"("tolerance":0.5)", 0.25);
    auto jit_out = run_jit(json, inputs);
    auto op   = rtbot::make_compare_sync_neq("op", 0.5);
    auto b2n  = rtbot::make_boolean_to_number("b2n");
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(b2n, 0, 0);
    b2n->connect(sink, 0, 0);
    drive_sync_op(op, inputs, b_inputs);
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }
}

// ===========================================================================
// BooleanSync (And/Or/Xor/Nand/Nor/Xnor/Implication, 2-input)
//
// FE BooleanSync expects BooleanData inputs and emits BooleanData. The JIT
// pipeline uses 0.0/1.0 doubles uniformly. We construct each pipeline with two
// streams of 0/1 booleans and compare via BooleanToNumber on the FE side.
// ===========================================================================
SCENARIO("JIT BooleanSync variants match FE bit-exactly",
         "[tier2][boolean_sync]") {
  // Generate 100 (a, b) bool-pairs covering all 4 combinations.
  std::vector<Sample> a_in, b_in;
  for (std::int64_t i = 1; i <= 100; ++i) {
    a_in.push_back({i, ((i / 2) % 2 == 0) ? 0.0 : 1.0});
    b_in.push_back({i, (i % 2 == 0) ? 0.0 : 1.0});
  }

  // For the JIT we need two streams of 0/1 doubles fed via Identity + Add(0)
  // BUT we also need both streams to actually carry distinct values. Use a
  // CompareEQ to derive the second stream from input parity.
  // Simpler: drive directly through two Constant-ish pipelines. We use the
  // sync pattern: Identity for stream-a, Add(value=0) for stream-b but that
  // makes both streams equal. Instead, use:
  //   in --> a (Identity) --> op:i1
  //   in --> floor_div_2 --> b (CompareEQ scaled) ... too complex.
  //
  // Cleaner approach: emit two values per timestep on the JIT side via a
  // pipeline that splits Input through CompareGT thresholds. But for parity
  // we need BIT-EXACT 0/1 values matching the booleans we send to FE.
  //
  // Approach: feed alternating a values via Input, derive b via:
  //   b = (input >= 0.5)  using CompareGTE
  // Make a ≡ Input (already 0 or 1), and b ≡ CompareGTE(input, 0.5).
  //
  // For the FE side, we send the same a/b booleans manually.
  //
  // To control a/b independently we instead drive the JIT with stream a only
  // and synthesize b on the JIT side via CompareGTE against threshold derived
  // to match b_in. Easier path: encode (a, b) -> v via a single bit pattern
  // and decompose via two CompareEQ. Use:
  //   v = 2*a + b   (so v in {0,1,2,3})
  //   a_stream = v >= 2.0    (CompareGTE 2.0)
  //   b_stream = (v == 1.0) || (v == 3.0)   harder.
  // Even simpler: a = (v >= 2), b = ((v % 2) >= 1) but no integer mod.
  //
  // SIMPLEST: encode the truth table choice with a direct (input value 0..3)
  // and synthesize a, b via floor/Add/Compare. Below uses:
  //   a = floor(v / 2.0)
  //   b = v - 2*floor(v/2)  ==> need TWO ops chain on JIT for b.
  //
  // For maintainability, we test JIT BooleanSync via this 2-input chain:
  //   JIT pipeline:
  //     in -> id_a (Identity)                                 --> op:i1
  //     in -> b_div2 (Scale 0.5) -> b_floor (Floor) ->
  //           b_scale2 (Scale 2.0) -> b_sub (compute v - 2*floor(v/2))
  //   But we need Subtraction (sync 2-input) to do v - X. That's a Join.
  //   This is getting complex.
  //
  // Pragmatic simplification: drive with v in {0,1} (bool) and test a single
  // axis (a == b). We instead use TWO separate runs to cover all 4 (a,b)
  // combinations by scanning v through the truth table values.

  // Build a JIT pipeline that consumes 'v' as a bit-encoded {0,1,2,3} index:
  //   a = (v >= 2)            => CompareGTE(2.0) -> 0/1
  //   b = ((v == 1) | (v==3)) => CompareEQ(1)|CompareEQ(3) at tolerance 0.0
  // Then op(a, b) -> Output.

  // 100 samples cycling through {0,1,2,3}
  std::vector<Sample> v_inputs;
  for (std::int64_t i = 1; i <= 100; ++i) {
    v_inputs.push_back({i, static_cast<double>(i % 4)});
  }
  // Reference a_in, b_in computed identically.
  a_in.clear(); b_in.clear();
  for (const auto& s : v_inputs) {
    int v = static_cast<int>(s.v);
    a_in.push_back({s.t, (v >= 2) ? 1.0 : 0.0});
    b_in.push_back({s.t, ((v == 1) || (v == 3)) ? 1.0 : 0.0});
  }

  struct BoolCase {
    const char* type;
    std::shared_ptr<rtbot::Operator> (*make)(std::string, std::size_t);
  };
  BoolCase cases[] = {
    {"LogicalAnd",         [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_and(std::move(id), n)); }},
    {"LogicalOr",          [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_or(std::move(id), n)); }},
    {"LogicalXor",         [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_xor(std::move(id), n)); }},
    {"LogicalNand",        [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_nand(std::move(id), n)); }},
    {"LogicalNor",         [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_nor(std::move(id), n)); }},
    {"LogicalXnor",        [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_xnor(std::move(id), n)); }},
    {"LogicalImplication", [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_implication(std::move(id), n)); }},
  };
  for (const auto& c : cases) {
    INFO("boolean sync type=" << c.type);
    // Build JSON. Two streams a, b derived from input v (cycling {0,1,2,3}).
    std::string json;
    json  = R"({"title":"t","apiVersion":"v1","entryOperator":"in",)";
    json += R"("output":{"out":["o1"]},"operators":[)";
    json += R"({"id":"in","type":"Input","portTypes":["number"]},)";
    // a = (v >= 2)
    json += R"({"id":"a","type":"CompareGTE","value":2.0},)";
    // b = (v == 1) OR (v == 3) — built via two CompareEQ + LogicalOr.
    json += R"({"id":"b1","type":"CompareEQ","value":1.0,"tolerance":0.0},)";
    json += R"({"id":"b3","type":"CompareEQ","value":3.0,"tolerance":0.0},)";
    json += R"({"id":"bjoin","type":"LogicalOr"},)";
    json += R"({"id":"op","type":")" + std::string(c.type) + R"("},)";
    json += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
    json += R"("connections":[)";
    json += R"({"from":"in","to":"a","fromPort":"o1","toPort":"i1"},)";
    json += R"({"from":"in","to":"b1","fromPort":"o1","toPort":"i1"},)";
    json += R"({"from":"in","to":"b3","fromPort":"o1","toPort":"i1"},)";
    json += R"({"from":"b1","to":"bjoin","fromPort":"o1","toPort":"i1"},)";
    json += R"({"from":"b3","to":"bjoin","fromPort":"o1","toPort":"i2"},)";
    json += R"({"from":"a","to":"op","fromPort":"o1","toPort":"i1"},)";
    json += R"({"from":"bjoin","to":"op","fromPort":"o1","toPort":"i2"},)";
    json += R"({"from":"op","to":"out","fromPort":"o1","toPort":"i1"}]})";
    auto jit_out = run_jit(json, v_inputs);

    // FE side: drive `op` directly with BooleanData a/b.
    auto op   = c.make("op", 2);
    auto b2n  = rtbot::make_boolean_to_number("b2n");
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(b2n, 0, 0);
    b2n->connect(sink, 0, 0);
    for (std::size_t i = 0; i < a_in.size(); ++i) {
      op->receive_data(rtbot::create_message<rtbot::BooleanData>(
                           a_in[i].t, rtbot::BooleanData{a_in[i].v != 0.0}),
                        0);
      op->receive_data(rtbot::create_message<rtbot::BooleanData>(
                           b_in[i].t, rtbot::BooleanData{b_in[i].v != 0.0}),
                        1);
    }
    op->execute();
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }
}

// ===========================================================================
// FilterScalar (predicate filter, 1-input)
// ===========================================================================
SCENARIO("JIT FilterScalar GT/LT match FE bit-exactly",
         "[tier2][filter_scalar][rel]") {
  auto inputs = make_inputs();

  struct FCase {
    const char* type;
    std::shared_ptr<rtbot::Operator> (*make)(std::string, double);
  };
  FCase cases[] = {
    {"GreaterThan", [](std::string id, double v){ return std::shared_ptr<rtbot::Operator>(rtbot::make_greater_than(std::move(id), v)); }},
    {"LessThan",    [](std::string id, double v){ return std::shared_ptr<rtbot::Operator>(rtbot::make_less_than(std::move(id), v)); }},
  };
  for (const auto& c : cases) {
    INFO("filter scalar type=" << c.type);
    auto jit_out = run_scalar_jit(c.type, 7.5, inputs, /*include_value=*/true);

    auto op   = c.make("op", 7.5);
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(sink, 0, 0);
    for (const auto& s : inputs) {
      op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
    }
    op->execute();
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }
}

SCENARIO("JIT FilterScalar EqualTo/NotEqualTo match FE bit-exactly",
         "[tier2][filter_scalar][eq]") {
  auto inputs = make_inputs();

  // EqualTo
  {
    INFO("EqualTo");
    std::string json =
        R"({"title":"t","apiVersion":"v1","entryOperator":"in","output":{"out":["o1"]},"operators":[)"
        R"({"id":"in","type":"Input","portTypes":["number"]},)"
        R"({"id":"op","type":"EqualTo","value":7.5,"epsilon":50.0},)"   // wide eps for nontrivial filter
        R"({"id":"out","type":"Output","portTypes":["number"]}],)"
        R"("connections":[{"from":"in","to":"op","fromPort":"o1","toPort":"i1"},)"
        R"({"from":"op","to":"out","fromPort":"o1","toPort":"i1"}]})";
    auto jit_out = run_jit(json, inputs);

    auto op   = rtbot::make_equal_to("op", 7.5, 50.0);
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(sink, 0, 0);
    for (const auto& s : inputs) {
      op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
    }
    op->execute();
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }

  // NotEqualTo
  {
    INFO("NotEqualTo");
    std::string json =
        R"({"title":"t","apiVersion":"v1","entryOperator":"in","output":{"out":["o1"]},"operators":[)"
        R"({"id":"in","type":"Input","portTypes":["number"]},)"
        R"({"id":"op","type":"NotEqualTo","value":7.5,"epsilon":50.0},)"
        R"({"id":"out","type":"Output","portTypes":["number"]}],)"
        R"("connections":[{"from":"in","to":"op","fromPort":"o1","toPort":"i1"},)"
        R"({"from":"op","to":"out","fromPort":"o1","toPort":"i1"}]})";
    auto jit_out = run_jit(json, inputs);

    auto op   = rtbot::make_not_equal_to("op", 7.5, 50.0);
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(sink, 0, 0);
    for (const auto& s : inputs) {
      op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
    }
    op->execute();
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }
}

// ===========================================================================
// FilterSync (predicate filter, 2-input)
// ===========================================================================
SCENARIO("JIT FilterSync GreaterThan/LessThan match FE bit-exactly",
         "[tier2][filter_sync][rel]") {
  auto inputs = make_inputs();
  std::vector<Sample> b_inputs;
  for (const auto& s : inputs) b_inputs.push_back({s.t, s.v + 7.25});

  struct FCase {
    const char* type;
    std::shared_ptr<rtbot::Operator> (*make)(std::string, std::size_t);
  };
  FCase cases[] = {
    {"SyncGreaterThan", [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_sync_greater_than(std::move(id), n)); }},
    {"SyncLessThan",    [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_sync_less_than(std::move(id), n)); }},
  };
  for (const auto& c : cases) {
    INFO("filter sync type=" << c.type);
    std::string json = make_sync_2in_json(c.type, "", 7.25);
    auto jit_out = run_jit(json, inputs);

    auto op   = c.make("op", 2);
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(sink, 0, 0);
    drive_sync_op(op, inputs, b_inputs);
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }
}

SCENARIO("JIT FilterSync Equal/NotEqual (with epsilon) match FE bit-exactly",
         "[tier2][filter_sync][eq]") {
  auto inputs = make_inputs();
  std::vector<Sample> b_inputs;
  for (const auto& s : inputs) b_inputs.push_back({s.t, s.v + 0.05});

  // Equal
  {
    std::string json = make_sync_2in_json("SyncEqual", R"("epsilon":1.0)", 0.05);
    auto jit_out = run_jit(json, inputs);

    auto op   = rtbot::make_sync_equal("op", 2, 1.0);
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(sink, 0, 0);
    drive_sync_op(op, inputs, b_inputs);
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }

  // NotEqual
  {
    std::string json = make_sync_2in_json("SyncNotEqual", R"("epsilon":1.0)", 0.05);
    auto jit_out = run_jit(json, inputs);

    auto op   = rtbot::make_sync_not_equal("op", 2, 1.0);
    auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    op->connect(sink, 0, 0);
    drive_sync_op(op, inputs, b_inputs);
    auto fe_out = drain_collector_number(*sink);
    require_parity(jit_out, fe_out);
  }
}
