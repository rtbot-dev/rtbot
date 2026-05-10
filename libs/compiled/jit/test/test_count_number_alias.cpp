// test_count_number_alias.cpp
//
// Bit-exact parity test for the JIT "CountNumber" -> OpKind::Count alias.
// Asserts that a program declaring its counter as "CountNumber" produces the
// identical emit stream as one declaring it as "Count".

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "rtbot/compiled/jit/JitCompiler.h"
#include "rtbot/compiled/jit/JitCompiledProgram.h"

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

struct Sample { std::int64_t t; double v; };
struct Emit   { std::int64_t t; double v; };

std::vector<Sample> make_inputs() {
  std::vector<Sample> out;
  out.reserve(20);
  std::mt19937_64 rng(0xC0FFEE17ULL);
  std::uniform_real_distribution<double> dist(-50.0, 50.0);
  for (std::int64_t i = 1; i <= 20; ++i) {
    out.push_back({i, dist(rng)});
  }
  return out;
}

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

std::string make_program(const std::string& counter_type) {
  std::string j;
  j  = R"({"title":"cn","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":["o1"]},)";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  j += R"({"id":"cnt","type":")" + counter_type + R"("},)";
  j += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  j += R"("connections":[)";
  j += R"({"from":"in","to":"cnt","fromPort":"o1","toPort":"i1"},)";
  j += R"({"from":"cnt","to":"out","fromPort":"o1","toPort":"i1"})";
  j += R"(]})";
  return j;
}

}  // namespace

SCENARIO("JIT CountNumber alias matches Count bit-exactly",
         "[jit][count_number][alias]") {
  const auto inputs = make_inputs();
  auto out_count = run_jit(make_program("Count"),       inputs);
  auto out_cnum  = run_jit(make_program("CountNumber"), inputs);

  REQUIRE(out_count.size() == inputs.size());
  REQUIRE(out_cnum.size()  == inputs.size());
  for (std::size_t i = 0; i < out_count.size(); ++i) {
    INFO("idx " << i << " count.t=" << out_count[i].t
                << " cn.t=" << out_cnum[i].t
                << " count.v=" << out_count[i].v
                << " cn.v=" << out_cnum[i].v);
    REQUIRE(out_count[i].t == out_cnum[i].t);
    REQUIRE(dbits(out_count[i].v) == dbits(out_cnum[i].v));
  }

  // Sanity: emitted values are 1.0, 2.0, ..., 20.0.
  for (std::size_t i = 0; i < out_count.size(); ++i) {
    REQUIRE(out_count[i].v == static_cast<double>(i + 1));
  }
}
