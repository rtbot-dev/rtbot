// Reproducible FusedExpression / FusedExpressionVector benchmark harness.
//
// Emits CSV with throughput and a SHA256 output-stream hash so runs can be
// compared both on performance AND on output-stream equality across builds.
//
// Two operators are measured per case:
//   - FE  = FusedExpression (N scalar input ports)
//   - FEV = FusedExpressionVector (1 vector input port)
//
// Each case is run across a sweep of chunk sizes (messages queued per port
// before execute()). chunk=1 is the realistic per-message streaming pattern
// (active_lanes=1 in the batched evaluator); larger chunks simulate upstream
// backlog and let the batched path see active_lanes > 1. The sweep gives a
// single-shot characterization of how the evaluator scales with backlog.
//
// Output CSV columns: name,driver,chunk,total_ns,throughput_msg_per_s,output_sha256.
// The output_sha256 column is independent of chunk — any divergence signals
// a batching-correctness bug.
//
// Note (2026-04-17): rtbot has empirically seen -O3 underperform the default
// -c opt on some workloads. This target is intentionally built without a
// hardcoded -O3 so the Bazel compilation mode (fastbuild / opt / dbg) drives
// the optimization level. Run with `bazel run -c opt //apps/benchmark:fused_bench`
// for the canonical comparison.

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/fuse/FusedAuxArgs.h"
#include "rtbot/fuse/FusedExpression.h"
#include "rtbot/fuse/FusedExpressionVector.h"

namespace {

using namespace rtbot;
using namespace rtbot::fused_op;

// --- Inline SHA256 (copy of the test-side implementation). --------------------
class Sha256 {
 public:
  Sha256()
      : h_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
           0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19},
        buffer_{}, bitlen_(0), buflen_(0) {}
  void update(double v) {
    update(reinterpret_cast<const std::uint8_t*>(&v), sizeof(double));
  }
  void update(const std::uint8_t* bytes, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
      buffer_[buflen_++] = bytes[i];
      if (buflen_ == 64) { transform(); bitlen_ += 512; buflen_ = 0; }
    }
  }
  std::string finalize() {
    std::uint64_t total_bits = bitlen_ + std::uint64_t(buflen_) * 8;
    buffer_[buflen_++] = 0x80;
    if (buflen_ > 56) {
      while (buflen_ < 64) buffer_[buflen_++] = 0;
      transform(); buflen_ = 0;
    }
    while (buflen_ < 56) buffer_[buflen_++] = 0;
    for (int i = 7; i >= 0; --i)
      buffer_[buflen_++] = static_cast<std::uint8_t>(total_bits >> (i * 8));
    transform();
    static const char kHex[] = "0123456789abcdef";
    std::string out; out.reserve(64);
    for (std::uint32_t w : h_) {
      for (int i = 3; i >= 0; --i) {
        std::uint8_t b = static_cast<std::uint8_t>(w >> (i * 8));
        out.push_back(kHex[b >> 4]); out.push_back(kHex[b & 0xF]);
      }
    }
    return out;
  }

 private:
  static std::uint32_t rotr(std::uint32_t x, std::uint32_t n) { return (x >> n) | (x << (32 - n)); }
  void transform() {
    static const std::uint32_t kK[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::uint32_t m[64];
    for (int i = 0, j = 0; i < 16; ++i, j += 4) {
      m[i] = (std::uint32_t(buffer_[j]) << 24) | (std::uint32_t(buffer_[j+1]) << 16)
           | (std::uint32_t(buffer_[j+2]) << 8) | std::uint32_t(buffer_[j+3]);
    }
    for (int i = 16; i < 64; ++i) {
      std::uint32_t s0 = rotr(m[i-15],7)^rotr(m[i-15],18)^(m[i-15]>>3);
      std::uint32_t s1 = rotr(m[i-2],17)^rotr(m[i-2],19)^(m[i-2]>>10);
      m[i] = m[i-16] + s0 + m[i-7] + s1;
    }
    std::uint32_t a=h_[0],b=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],hh=h_[7];
    for (int i = 0; i < 64; ++i) {
      std::uint32_t S1 = rotr(e,6)^rotr(e,11)^rotr(e,25);
      std::uint32_t ch = (e & f) ^ (~e & g);
      std::uint32_t t1 = hh + S1 + ch + kK[i] + m[i];
      std::uint32_t S0 = rotr(a,2)^rotr(a,13)^rotr(a,22);
      std::uint32_t maj = (a&b)^(a&c)^(b&c);
      std::uint32_t t2 = S0 + maj;
      hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    h_[0]+=a;h_[1]+=b;h_[2]+=c;h_[3]+=d;h_[4]+=e;h_[5]+=f;h_[6]+=g;h_[7]+=hh;
  }
  std::array<std::uint32_t, 8> h_;
  std::array<std::uint8_t, 64> buffer_;
  std::uint64_t bitlen_;
  std::size_t buflen_;
};

// --- Benchmark cases. ---------------------------------------------------------
struct Case {
  std::string name;
  std::size_t num_inputs;
  std::size_t num_outputs;
  std::vector<double> bytecode;
  std::vector<double> constants;
  std::vector<double> state_init;
  std::vector<double> coefficients;
  std::size_t num_messages;
  std::uint64_t seed;
};

struct Result {
  std::string name;
  std::string driver;          // "FE" or "FEV"
  std::size_t chunk;           // messages queued before each execute()
  double total_ns;
  double throughput_msg_per_s;
  std::string output_sha256;
};

std::vector<std::vector<double>> gen_inputs(std::size_t num_inputs,
                                              std::size_t num_messages,
                                              std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);
  std::vector<std::vector<double>> out(num_messages,
                                         std::vector<double>(num_inputs));
  for (auto& m : out) for (auto& v : m) v = dist(rng);
  return out;
}

Result run_fe(const Case& c, std::size_t chunk) {
  auto inputs = gen_inputs(c.num_inputs, c.num_messages, c.seed);
  auto op = make_fused_expression(c.name + "_fe", c.num_inputs, c.num_outputs,
                                    c.bytecode, c.constants, c.coefficients,
                                    c.state_init);
  auto col = make_vector_number_collector(c.name + "_fe_col");
  op->connect(col, 0, 0);

  auto t0 = std::chrono::steady_clock::now();
  std::size_t queued = 0;
  for (std::size_t t = 0; t < c.num_messages; ++t) {
    for (std::size_t p = 0; p < c.num_inputs; ++p) {
      op->receive_data(create_message<NumberData>(
                           static_cast<std::int64_t>(t + 1),
                           NumberData{inputs[t][p]}),
                       p);
    }
    if (++queued >= chunk) {
      op->execute();
      queued = 0;
    }
  }
  if (queued > 0) op->execute();
  auto t1 = std::chrono::steady_clock::now();

  Sha256 h;
  auto& q = col->get_data_queue(0);
  for (std::size_t t = 0; t < q.size(); ++t) {
    const auto* m = static_cast<const Message<VectorNumberData>*>(q[t].get());
    for (double v : *m->data.values) h.update(v);
  }

  Result r;
  r.name = c.name;
  r.driver = "FE";
  r.chunk = chunk;
  r.total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
  r.throughput_msg_per_s = double(c.num_messages) * 1e9 / r.total_ns;
  r.output_sha256 = h.finalize();
  return r;
}

Result run_fev(const Case& c, std::size_t chunk) {
  auto inputs = gen_inputs(c.num_inputs, c.num_messages, c.seed);
  auto op = make_fused_expression_vector(c.name + "_fev", c.num_outputs,
                                           c.bytecode, c.constants,
                                           c.coefficients, c.state_init);
  auto col = make_vector_number_collector(c.name + "_fev_col");
  op->connect(col, 0, 0);

  auto t0 = std::chrono::steady_clock::now();
  std::size_t queued = 0;
  for (std::size_t t = 0; t < c.num_messages; ++t) {
    auto v = std::make_shared<std::vector<double>>(inputs[t]);
    op->receive_data(create_message<VectorNumberData>(
                         static_cast<std::int64_t>(t + 1),
                         VectorNumberData(std::move(v))),
                     0);
    if (++queued >= chunk) {
      op->execute();
      queued = 0;
    }
  }
  if (queued > 0) op->execute();
  auto t1 = std::chrono::steady_clock::now();

  Sha256 h;
  auto& q = col->get_data_queue(0);
  for (std::size_t t = 0; t < q.size(); ++t) {
    const auto* m = static_cast<const Message<VectorNumberData>*>(q[t].get());
    for (double v : *m->data.values) h.update(v);
  }

  Result r;
  r.name = c.name;
  r.driver = "FEV";
  r.chunk = chunk;
  r.total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
  r.throughput_msg_per_s = double(c.num_messages) * 1e9 / r.total_ns;
  r.output_sha256 = h.finalize();
  return r;
}

}  // namespace

int main() {
  std::vector<Case> cases;

  // Pure arithmetic — exercises the dispatch hot path.
  Case pure_arith;
  pure_arith.name = "pure_arithmetic";
  pure_arith.num_inputs = 2;
  pure_arith.num_outputs = 1;
  pure_arith.bytecode = {INPUT, 0, INPUT, 1, ADD, CONST, 0, MUL, END};
  pure_arith.constants = {1.5};
  pure_arith.state_init = {};
  pure_arith.num_messages = 1'000'000;
  pure_arith.seed = 0xBEEF01;
  cases.push_back(pure_arith);

  // IMS-like projection (same shape as the golden).
  Case ims;
  ims.name = "ims_like";
  ims.num_inputs = 4;
  ims.num_outputs = 5;
  ims.bytecode = {INPUT, 0, END,
                  INPUT, 1, END,
                  INPUT, 2, ABS, SQRT, END,
                  INPUT, 2, INPUT, 3, INPUT, 3, MUL, SUB, ABS, SQRT, END,
                  INPUT, 2, INPUT, 3, ABS, CONST, 0, ADD, DIV, END};
  ims.constants = {1.0e-9};
  ims.state_init = {};
  ims.num_messages = 1'000'000;
  ims.seed = 0xBEEF02;
  cases.push_back(ims);

  // Finance-like with CUMSUM / COUNT / STATE_LOAD.
  Case fin;
  fin.name = "finance_avg";
  fin.num_inputs = 2;
  fin.num_outputs = 3;
  fin.bytecode = {INPUT, 0, CUMSUM, 0, END,
                  COUNT, 2, END,
                  STATE_LOAD, 0, STATE_LOAD, 2, DIV, END};
  fin.constants = {};
  fin.state_init = {0.0, 0.0, 0.0};
  fin.num_messages = 1'000'000;
  fin.seed = 0xBEEF03;
  cases.push_back(fin);

  // Windowed: a single MA(W=50) — Tier-1 opcode routed through the scalar
  // fallback path until batched support lands. Gives a baseline throughput
  // number for the windowed-opcode path so regressions are visible.
  Case ma50;
  ma50.name = "ma_window_50";
  ma50.num_inputs = 1;
  ma50.num_outputs = 1;
  // MA_UPDATE carries its window size inline; pack_bytecode auto-allocates
  // state and builds the internal aux_args side table.
  ma50.bytecode = {INPUT, 0, MA_UPDATE, 50, END};
  ma50.constants = {};
  ma50.state_init = {};
  ma50.coefficients = {};
  ma50.num_messages = 1'000'000;
  ma50.seed = 0xBEEF04;
  cases.push_back(ma50);

  // Chunk sizes to sweep. 1 = realistic per-message execute (active_lanes=1);
  // kBatch (8) = exactly one full batch per execute; larger values simulate
  // deeper backlog.
  const std::vector<std::size_t> chunks = {1, 2, 4, 8, 16, 64, 1000};

  std::printf("name,driver,chunk,total_ns,throughput_msg_per_s,output_sha256\n");
  for (const auto& c : cases) {
    for (std::size_t chunk : chunks) {
      auto rfe = run_fe(c, chunk);
      std::printf("%s,%s,%zu,%.0f,%.1f,%s\n",
                  rfe.name.c_str(), rfe.driver.c_str(), rfe.chunk,
                  rfe.total_ns, rfe.throughput_msg_per_s,
                  rfe.output_sha256.c_str());
      auto rfev = run_fev(c, chunk);
      std::printf("%s,%s,%zu,%.0f,%.1f,%s\n",
                  rfev.name.c_str(), rfev.driver.c_str(), rfev.chunk,
                  rfev.total_ns, rfev.throughput_msg_per_s,
                  rfev.output_sha256.c_str());
    }
  }
  return 0;
}
