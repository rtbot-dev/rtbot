#ifndef RTBOT_JIT_TEST_FUZZ_HELPER_H
#define RTBOT_JIT_TEST_FUZZ_HELPER_H

// Random-fuzz parity helper for the rtbot JIT. Generates valid Program JSON
// pipelines from a seeded RNG, drives each through both JIT and framework
// modes, and reports the first per-emission divergence to /tmp.
//
// The grammar deliberately stays inside the JIT's in-scope op set (no
// Pipeline/KeyedPipeline/TopK/BurstAggregate/Fused*/Vector*/Demux/Mux). The
// goal is to find new bug shapes in the operator-mix space the curated audits
// did not enumerate by hand.

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "parity_helper.h"
#include "rtbot/Program.h"

namespace rtbot::test::fuzz {

// -----------------------------------------------------------------------------
// Op metadata. Every op the fuzz generator may emit has an entry below.
//
//   PortType::Number   — scalar number (most ops produce and consume this)
//   PortType::Boolean  — boolean (Logical*, Compare* outputs feed booleans)
// -----------------------------------------------------------------------------

enum class PortType { Number, Boolean };

// Description of a candidate op the generator can emit. The output port is
// always "o1" (multi-output joins are special-cased in the generator).
struct OpSpec {
  const char* type_str;
  PortType output_type;
  // Number of data input ports. 1 for stateless unary or stateful 1->1. 2 or 3
  // for joins / linear / reduce-joins.
  int num_inputs;
  // Type of each input port (size == num_inputs).
  std::vector<PortType> input_types;
  // If true, the op needs a "window_size" int param.
  bool needs_window = false;
  // If true, the op needs a "value" double param (Scale, AddScalar, Compare*).
  bool needs_scalar = false;
  // If true, the op needs a "coefficients" array param sized num_inputs (Linear).
  bool needs_coefficients = false;
  // If true, the op needs a "numPorts" int matching num_inputs (Join, ReduceJoin).
  bool needs_num_ports = false;
  // If true, the op needs an "interval" int >= 1 (Resampler*).
  bool needs_interval = false;
};

inline const std::vector<OpSpec>& curated_ops() {
  static const std::vector<OpSpec> ops = {
      // Stateless 1->1 number ops.
      {"Identity",            PortType::Number,  1, {PortType::Number}},
      {"Scale",               PortType::Number,  1, {PortType::Number}, false, true},
      {"Add",                 PortType::Number,  1, {PortType::Number}, false, true},
      {"Abs",                 PortType::Number,  1, {PortType::Number}},
      {"Sin",                 PortType::Number,  1, {PortType::Number}},
      {"Cos",                 PortType::Number,  1, {PortType::Number}},
      {"Floor",               PortType::Number,  1, {PortType::Number}},

      // Stateful 1->1 number ops.
      {"MovingAverage",       PortType::Number,  1, {PortType::Number}, true},
      {"MovingSum",           PortType::Number,  1, {PortType::Number}, true},
      {"StandardDeviation",   PortType::Number,  1, {PortType::Number}, true},
      {"Difference",          PortType::Number,  1, {PortType::Number}},
      {"SignChange",          PortType::Number,  1, {PortType::Number}},
      {"CumulativeSum",       PortType::Number,  1, {PortType::Number}},
      {"CountNumber",         PortType::Number,  1, {PortType::Number}},
      {"MaxTracker",          PortType::Number,  1, {PortType::Number}},
      {"MinTracker",          PortType::Number,  1, {PortType::Number}},
      {"PeakDetector",        PortType::Number,  1, {PortType::Number}, true},

      // Filter-gated comparator family (Number in, Number out — only emits
      // when comparator is true). These are the asymmetric-emit producers
      // most likely to surface multi-port-Output bugs.
      {"GreaterThan",         PortType::Number,  1, {PortType::Number}, false, true},
      {"LessThan",            PortType::Number,  1, {PortType::Number}, false, true},
      {"EqualTo",             PortType::Number,  1, {PortType::Number}, false, true},
      {"NotEqualTo",          PortType::Number,  1, {PortType::Number}, false, true},

      // Sync comparators / boolean producers. CompareSync* output booleans.
      {"CompareSyncGT",       PortType::Boolean, 2,
       {PortType::Number, PortType::Number}},
      {"CompareSyncLT",       PortType::Boolean, 2,
       {PortType::Number, PortType::Number}},
      {"CompareSyncGTE",      PortType::Boolean, 2,
       {PortType::Number, PortType::Number}},
      {"CompareSyncLTE",      PortType::Boolean, 2,
       {PortType::Number, PortType::Number}},

      // Boolean ops.
      {"LogicalAnd",          PortType::Boolean, 2,
       {PortType::Boolean, PortType::Boolean}},
      {"LogicalOr",           PortType::Boolean, 2,
       {PortType::Boolean, PortType::Boolean}},
      {"LogicalXor",          PortType::Boolean, 2,
       {PortType::Boolean, PortType::Boolean}},

      // Sync 2-port number arithmetic.
      {"Addition",            PortType::Number,  2,
       {PortType::Number, PortType::Number}},
      {"Subtraction",         PortType::Number,  2,
       {PortType::Number, PortType::Number}},
      {"Multiplication",      PortType::Number,  2,
       {PortType::Number, PortType::Number}},

      // Sync Join (2-port). PortTypes are part of the JSON; we cover number-only
      // joins here. Output is still a single port-0 number stream for the
      // generator's purposes (port-1 is left wired internally — fuzz target
      // is the join's o1 emission cadence under asymmetric inputs).
      {"Join",                PortType::Number,  2,
       {PortType::Number, PortType::Number}},

      // Linear with 2 coefficients == 2-port weighted sum.
      {"Linear",              PortType::Number,  2,
       {PortType::Number, PortType::Number}, false, false, true},

      // Resamplers — single number in, multi-emit number out.
      {"ResamplerHermite",    PortType::Number,  1, {PortType::Number},
       false, false, false, false, true},
      {"ResamplerConstant",   PortType::Number,  1, {PortType::Number},
       false, false, false, false, true},
  };
  return ops;
}

// -----------------------------------------------------------------------------
// Fuzz configuration. Every parameter is deterministic given `seed`.
// -----------------------------------------------------------------------------

struct FuzzConfig {
  std::uint64_t seed = 42;
  int num_pipelines = 200;
  int min_ops = 3;
  int max_ops = 12;
  int ticks_per_test = 200;
  bool include_resampler = true;
  // 1% probability of a NaN/Inf injection per scalar input.
  double nan_inf_probability = 0.01;
  // Probability per tick that the timestamp jumps forward by >1 (creates the
  // gap-pattern resamplers care about).
  double timestamp_gap_probability = 0.15;
};

struct FuzzStats {
  int generated = 0;
  int skipped_invalid = 0;
  int skipped_jit_unsupported = 0;
  int tested = 0;
  int passed = 0;
  int failed = 0;
  std::vector<std::string> failure_summaries;
};

// -----------------------------------------------------------------------------
// JSON pipeline assembler. Builds an in-memory graph, then serialises to JSON.
// -----------------------------------------------------------------------------

struct GraphNode {
  std::string id;
  std::string type;
  PortType out_type{PortType::Number};
  std::string params_json;  // additional JSON body, e.g. ,"window_size":3
};

struct GraphEdge {
  std::string from_id;
  std::string from_port;  // "o1", "o2"
  std::string to_id;
  std::string to_port;    // "i1", "i2", ...
};

struct GeneratedPipeline {
  std::string json;
  // Number of output ports on the final Output op. Used to size the input.
  int output_arity = 1;
};

class PipelineGenerator {
 public:
  PipelineGenerator(const FuzzConfig& cfg, std::uint64_t seed)
      : cfg_(cfg), rng_(seed) {}

  // Generate one pipeline. Returns std::nullopt-style empty json on failure.
  GeneratedPipeline generate() {
    nodes_.clear();
    edges_.clear();
    producers_number_.clear();
    producers_boolean_.clear();
    next_id_ = 0;

    // Entry: scalar number Input.
    GraphNode input{"input", "Input", PortType::Number,
                    R"(,"portTypes":["number"])"};
    nodes_.push_back(input);
    producers_number_.push_back(input.id);

    const int target_ops = rand_int(cfg_.min_ops, cfg_.max_ops);

    // Build up to target_ops operators (besides Input and Output).
    int attempts = 0;
    const int max_attempts = target_ops * 4;
    while (static_cast<int>(nodes_.size()) - 1 < target_ops &&
           attempts < max_attempts) {
      ++attempts;
      const auto& spec = pick_op_spec();
      if (!cfg_.include_resampler && spec.needs_interval) {
        continue;
      }
      if (!can_satisfy_inputs(spec)) {
        continue;
      }
      add_op_node(spec);
    }

    // Output op: pick 1..3 number producers as input ports.
    const int output_arity = std::min<int>(
        3, std::max<int>(1, rand_int(1, 3)));
    int actual_arity = std::min<int>(output_arity,
                                     static_cast<int>(producers_number_.size()));
    if (actual_arity < 1) {
      // Should not happen — Input is always a number producer.
      actual_arity = 1;
    }

    // Compose Output op JSON.
    std::vector<std::string> output_port_types(actual_arity, "\"number\"");
    std::string port_types_arr = "[";
    for (int i = 0; i < actual_arity; ++i) {
      if (i) port_types_arr += ",";
      port_types_arr += output_port_types[i];
    }
    port_types_arr += "]";

    GraphNode output_node;
    output_node.id = "output";
    output_node.type = "Output";
    output_node.out_type = PortType::Number;
    output_node.params_json = ",\"portTypes\":" + port_types_arr;
    nodes_.push_back(output_node);

    // Wire output ports — pick distinct (or repeating) number producers.
    for (int i = 0; i < actual_arity; ++i) {
      const std::string& src =
          producers_number_[rand_int(0, static_cast<int>(
                                            producers_number_.size()) - 1)];
      edges_.push_back({src, "o1", "output", "i" + std::to_string(i + 1)});
    }

    GeneratedPipeline result;
    result.output_arity = actual_arity;
    result.json = serialize(actual_arity);
    return result;
  }

 private:
  // RNG helpers.
  int rand_int(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng_);
  }
  double rand_double(double lo, double hi) {
    std::uniform_real_distribution<double> d(lo, hi);
    return d(rng_);
  }

  const OpSpec& pick_op_spec() {
    const auto& ops = curated_ops();
    return ops[rand_int(0, static_cast<int>(ops.size()) - 1)];
  }

  bool can_satisfy_inputs(const OpSpec& spec) const {
    for (const auto& t : spec.input_types) {
      const auto& pool = (t == PortType::Number)
                             ? producers_number_
                             : producers_boolean_;
      if (pool.empty()) return false;
    }
    return true;
  }

  std::string pick_producer(PortType t) {
    const auto& pool = (t == PortType::Number)
                          ? producers_number_
                          : producers_boolean_;
    return pool[rand_int(0, static_cast<int>(pool.size()) - 1)];
  }

  void add_op_node(const OpSpec& spec) {
    GraphNode node;
    node.id = "op" + std::to_string(next_id_++);
    node.type = spec.type_str;
    node.out_type = spec.output_type;

    // Build params suffix.
    std::ostringstream params;
    if (spec.needs_window) {
      const int w = rand_int(2, 7);
      params << ",\"window_size\":" << w;
    }
    if (spec.needs_scalar) {
      const double v = rand_double(-5.0, 5.0);
      params << ",\"value\":" << v;
    }
    if (spec.needs_interval) {
      const int iv = rand_int(1, 5);
      params << ",\"interval\":" << iv;
    }
    if (spec.needs_coefficients) {
      params << ",\"coefficients\":[";
      for (int i = 0; i < spec.num_inputs; ++i) {
        if (i) params << ",";
        params << rand_double(-2.0, 2.0);
      }
      params << "]";
    }
    if (spec.needs_num_ports) {
      params << ",\"numPorts\":" << spec.num_inputs;
    }
    // Join requires a portTypes array.
    if (std::string(spec.type_str) == "Join") {
      params << ",\"portTypes\":[";
      for (int i = 0; i < spec.num_inputs; ++i) {
        if (i) params << ",";
        params << "\"number\"";
      }
      params << "]";
    }
    node.params_json = params.str();
    nodes_.push_back(node);

    // Connect inputs.
    for (int i = 0; i < spec.num_inputs; ++i) {
      const std::string src = pick_producer(spec.input_types[i]);
      edges_.push_back({src, "o1", node.id, "i" + std::to_string(i + 1)});
    }

    // Register as a producer.
    if (spec.output_type == PortType::Number) {
      producers_number_.push_back(node.id);
    } else {
      producers_boolean_.push_back(node.id);
    }
  }

  std::string serialize(int output_arity) const {
    std::ostringstream js;
    js << "{\"entryOperator\":\"input\",\"output\":{\"output\":[";
    for (int i = 0; i < output_arity; ++i) {
      if (i) js << ",";
      js << "\"o" << (i + 1) << "\"";
    }
    js << "]},\"operators\":[";
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
      if (i) js << ",";
      const auto& n = nodes_[i];
      js << "{\"id\":\"" << n.id << "\",\"type\":\"" << n.type << "\""
         << n.params_json << "}";
    }
    js << "],\"connections\":[";
    for (std::size_t i = 0; i < edges_.size(); ++i) {
      if (i) js << ",";
      const auto& e = edges_[i];
      js << "{\"from\":\"" << e.from_id << "\",\"to\":\"" << e.to_id
         << "\",\"fromPort\":\"" << e.from_port << "\",\"toPort\":\""
         << e.to_port << "\"}";
    }
    js << "]}";
    return js.str();
  }

  FuzzConfig cfg_;
  std::mt19937_64 rng_;
  std::vector<GraphNode> nodes_;
  std::vector<GraphEdge> edges_;
  std::vector<std::string> producers_number_;
  std::vector<std::string> producers_boolean_;
  int next_id_ = 0;
};

// -----------------------------------------------------------------------------
// Input generator. Scalar number stream with optional NaN/Inf injection and
// timestamp gaps (the latter is important for ResamplerHermite/Constant).
// -----------------------------------------------------------------------------

inline std::vector<TickInput> generate_scalar_inputs(
    const FuzzConfig& cfg, std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> v_dist(-100.0, 100.0);
  std::uniform_real_distribution<double> prob(0.0, 1.0);
  std::uniform_int_distribution<int> gap_dist(2, 5);
  std::uniform_int_distribution<int> nan_choice(0, 2);

  std::vector<TickInput> out;
  out.reserve(cfg.ticks_per_test);
  std::int64_t t = 1;
  for (int i = 0; i < cfg.ticks_per_test; ++i) {
    double v = v_dist(rng);
    if (prob(rng) < cfg.nan_inf_probability) {
      const int which = nan_choice(rng);
      if (which == 0) v = std::numeric_limits<double>::quiet_NaN();
      else if (which == 1) v = std::numeric_limits<double>::infinity();
      else v = -std::numeric_limits<double>::infinity();
    }
    out.push_back({t, {v}});
    if (prob(rng) < cfg.timestamp_gap_probability) {
      t += gap_dist(rng);
    } else {
      t += 1;
    }
  }
  return out;
}

// -----------------------------------------------------------------------------
// Drive helper — same drain/capture used by parity_helper, but tolerates JIT
// build failures and value divergences without aborting the SCENARIO via
// REQUIRE. Returns either a clean run or a structured FuzzFailure that
// describes the first divergent emission.
// -----------------------------------------------------------------------------

struct FuzzFailure {
  enum class Kind { CountMismatch, TimestampMismatch, ValueMismatch };
  Kind kind = Kind::ValueMismatch;
  std::size_t emission_index = 0;
  std::int64_t jit_t = 0;
  std::int64_t interp_t = 0;
  std::size_t jit_count = 0;
  std::size_t interp_count = 0;
  std::size_t value_index = 0;
  double jit_value = 0.0;
  double interp_value = 0.0;
  std::uint64_t jit_bits = 0;
  std::uint64_t interp_bits = 0;
};

// Returns true if the pipeline is JIT-eligible (using_jit() == true) AND the
// program builds cleanly under both modes. On compile failure returns false
// (caller should skip this pipeline, not count it as a divergence).
inline bool pipeline_uses_jit(const std::string& json) {
  Program::set_force_interpreter_for_testing(false);
  try {
    Program prog(json);
    const bool ok = prog.using_jit();
    return ok;
  } catch (...) {
    return false;
  }
}

inline bool drive_jit_vs_interpreter(
    const std::string& program_json,
    const std::vector<TickInput>& inputs,
    const std::function<void(Program&, const TickInput&)>& drive,
    FuzzFailure* failure_out) {
  std::vector<ParityEmission> jit_emissions;
  std::vector<ParityEmission> interp_emissions;

  try {
    Program::set_force_interpreter_for_testing(false);
    {
      Program prog(program_json);
      jit_emissions = drive_and_capture(prog, inputs, drive);
    }
    Program::set_force_interpreter_for_testing(true);
    {
      Program prog(program_json);
      interp_emissions = drive_and_capture(prog, inputs, drive);
    }
    Program::set_force_interpreter_for_testing(false);
  } catch (...) {
    Program::set_force_interpreter_for_testing(false);
    throw;
  }

  if (jit_emissions.size() != interp_emissions.size()) {
    if (failure_out) {
      failure_out->kind = FuzzFailure::Kind::CountMismatch;
      failure_out->jit_count = jit_emissions.size();
      failure_out->interp_count = interp_emissions.size();
    }
    return false;
  }
  for (std::size_t i = 0; i < jit_emissions.size(); ++i) {
    const auto& a = jit_emissions[i];
    const auto& b = interp_emissions[i];
    if (a.t != b.t || a.values.size() != b.values.size()) {
      if (failure_out) {
        failure_out->kind = FuzzFailure::Kind::TimestampMismatch;
        failure_out->emission_index = i;
        failure_out->jit_t = a.t;
        failure_out->interp_t = b.t;
        failure_out->jit_count = a.values.size();
        failure_out->interp_count = b.values.size();
      }
      return false;
    }
    for (std::size_t k = 0; k < a.values.size(); ++k) {
      if (dbits(a.values[k]) != dbits(b.values[k])) {
        if (failure_out) {
          failure_out->kind = FuzzFailure::Kind::ValueMismatch;
          failure_out->emission_index = i;
          failure_out->jit_t = a.t;
          failure_out->interp_t = b.t;
          failure_out->value_index = k;
          failure_out->jit_value = a.values[k];
          failure_out->interp_value = b.values[k];
          failure_out->jit_bits = dbits(a.values[k]);
          failure_out->interp_bits = dbits(b.values[k]);
        }
        return false;
      }
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
// Failure dump helpers — log to /tmp so the user can re-run a single repro.
// -----------------------------------------------------------------------------

inline void dump_pipeline_json(std::uint64_t seed, int pipeline_index,
                               const std::string& json) {
  std::ostringstream path;
  path << "/tmp/fuzz_failure_" << seed << "_p" << pipeline_index << ".json";
  std::ofstream f(path.str());
  if (f.is_open()) f << json;
}

inline void dump_inputs(std::uint64_t seed, int pipeline_index,
                        const std::vector<TickInput>& inputs) {
  std::ostringstream path;
  path << "/tmp/fuzz_failure_" << seed << "_p" << pipeline_index
       << "_input.txt";
  std::ofstream f(path.str());
  if (!f.is_open()) return;
  for (const auto& tick : inputs) {
    f << tick.t;
    for (double v : tick.values) f << " " << v;
    f << "\n";
  }
}

inline std::string format_failure_summary(std::uint64_t seed,
                                          int pipeline_index,
                                          const FuzzFailure& fail) {
  std::ostringstream oss;
  oss << "seed=" << seed << " pipeline=" << pipeline_index << " ";
  switch (fail.kind) {
    case FuzzFailure::Kind::CountMismatch:
      oss << "count_mismatch jit=" << fail.jit_count
          << " interp=" << fail.interp_count;
      break;
    case FuzzFailure::Kind::TimestampMismatch:
      oss << "timestamp_mismatch at emission " << fail.emission_index
          << " jit_t=" << fail.jit_t << " interp_t=" << fail.interp_t
          << " jit_n=" << fail.jit_count << " interp_n=" << fail.interp_count;
      break;
    case FuzzFailure::Kind::ValueMismatch:
      oss << "value_mismatch at emission " << fail.emission_index
          << " value_idx=" << fail.value_index
          << " t=" << fail.jit_t
          << " jit=" << fail.jit_value
          << " interp=" << fail.interp_value
          << std::hex << " jit_bits=0x" << fail.jit_bits
          << " interp_bits=0x" << fail.interp_bits;
      break;
  }
  return oss.str();
}

// -----------------------------------------------------------------------------
// Top-level fuzz driver.
// -----------------------------------------------------------------------------

inline FuzzStats run_fuzz(const FuzzConfig& cfg) {
  FuzzStats stats;
  // Reset the per-seed report so re-runs do not concatenate to old findings.
  {
    std::ostringstream path;
    path << "/tmp/fuzz_report_" << cfg.seed << ".txt";
    std::ofstream f(path.str(), std::ios::trunc);
  }
  std::mt19937_64 master_rng(cfg.seed);
  std::uniform_int_distribution<std::uint64_t> child_seed_dist(
      1, std::numeric_limits<std::uint64_t>::max());

  // Optional: skip a list of pipeline indices known to crash LLVM with
  // report_fatal_error so a full fuzz run can complete after such a finding.
  // Comma-separated list parsed from env var RTBOT_FUZZ_SKIP.
  std::vector<int> skip_indices;
  if (const char* s = std::getenv("RTBOT_FUZZ_SKIP")) {
    std::string buf;
    for (const char* p = s;; ++p) {
      if (*p == ',' || *p == '\0') {
        if (!buf.empty()) {
          try { skip_indices.push_back(std::stoi(buf)); } catch (...) {}
          buf.clear();
        }
        if (*p == '\0') break;
      } else {
        buf.push_back(*p);
      }
    }
  }
  auto is_skipped = [&](int idx) {
    for (int s : skip_indices) if (s == idx) return true;
    return false;
  };

  for (int i = 0; i < cfg.num_pipelines; ++i) {
    const std::uint64_t pipeline_seed = child_seed_dist(master_rng);
    const std::uint64_t input_seed = child_seed_dist(master_rng);

    if (is_skipped(i)) {
      ++stats.skipped_invalid;
      continue;
    }

    PipelineGenerator gen(cfg, pipeline_seed);
    GeneratedPipeline pipeline;
    try {
      pipeline = gen.generate();
    } catch (...) {
      ++stats.skipped_invalid;
      continue;
    }
    ++stats.generated;

    // Persist the pipeline index trail to /tmp so an LLVM abort during the
    // next compile is attributable to the JSON written on this iteration.
    {
      std::ostringstream path;
      path << "/tmp/fuzz_last_pipeline_" << cfg.seed << ".json";
      std::ofstream f(path.str());
      if (f.is_open()) {
        f << "// pipeline_index=" << i << "\n" << pipeline.json;
      }
    }

    if (!pipeline_uses_jit(pipeline.json)) {
      ++stats.skipped_jit_unsupported;
      continue;
    }

    auto inputs = generate_scalar_inputs(cfg, input_seed);
    auto drive = [](Program& p, const TickInput& tick) {
      p.send(tick.t, tick.values[0]);
    };

    FuzzFailure failure;
    bool ok = false;
    try {
      ok = drive_jit_vs_interpreter(pipeline.json, inputs, drive, &failure);
    } catch (...) {
      // A throw during run is a JIT/runtime crash — record it as a failure.
      ++stats.tested;
      ++stats.failed;
      dump_pipeline_json(cfg.seed, i, pipeline.json);
      dump_inputs(cfg.seed, i, inputs);
      std::ostringstream oss;
      oss << "seed=" << cfg.seed << " pipeline=" << i << " exception_thrown";
      stats.failure_summaries.push_back(oss.str());
      continue;
    }

    ++stats.tested;
    if (ok) {
      ++stats.passed;
    } else {
      ++stats.failed;
      dump_pipeline_json(cfg.seed, i, pipeline.json);
      dump_inputs(cfg.seed, i, inputs);
      const std::string summary = format_failure_summary(cfg.seed, i, failure);
      stats.failure_summaries.push_back(summary);
      // Persist incrementally so an LLVM abort later in the run does not
      // discard the in-memory stats vector.
      std::ostringstream path;
      path << "/tmp/fuzz_report_" << cfg.seed << ".txt";
      std::ofstream f(path.str(), std::ios::app);
      if (f.is_open()) f << summary << "\n";
    }
  }
  return stats;
}

}  // namespace rtbot::test::fuzz

#endif  // RTBOT_JIT_TEST_FUZZ_HELPER_H
