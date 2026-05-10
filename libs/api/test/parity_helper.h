#ifndef RTBOT_API_TEST_PARITY_HELPER_H
#define RTBOT_API_TEST_PARITY_HELPER_H

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "rtbot/Program.h"

namespace rtbot::test {

// One captured emission from Program::drain_into.
struct ParityEmission {
  std::int64_t t{0};
  std::int32_t port_id{0};
  std::vector<double> values;
};

// One element of test input. The user-supplied drive callback decides how to
// translate this into a Program::send / send_vector / receive_batch call.
struct TickInput {
  std::int64_t t{0};
  std::vector<double> values;
};

// Bit pattern of a double, for distinguishing NaN encodings and signed zero.
inline std::uint64_t dbits(double v) noexcept {
  std::uint64_t u = 0;
  std::memcpy(&u, &v, sizeof(double));
  return u;
}

// Drive the program through `inputs` via the user-supplied callback, draining
// after each tick into the emissions vector. Hooks the drain via drain_into so
// emission ordering matches the JIT/interpreter wire format.
inline std::vector<ParityEmission> drive_and_capture(
    Program& prog,
    const std::vector<TickInput>& inputs,
    const std::function<void(Program&, const TickInput&)>& drive) {
  std::vector<ParityEmission> out;
  for (const auto& tick : inputs) {
    drive(prog, tick);
    prog.drain_into([&](std::int64_t t, const double* v, std::size_t n) {
      ParityEmission e;
      e.t = t;
      e.port_id = 0;  // drain_into does not surface port_id today; both modes
                      // give 0 and the helper would compare like-for-like even
                      // if a future port_id surfaced symmetrically.
      e.values.assign(v, v + n);
      out.push_back(std::move(e));
    });
  }
  return out;
}

// Run the same JSON program through BOTH interpreter and JIT, drive with the
// same inputs via the user-supplied callback, capture every emission, and
// REQUIRE bit-exact equality.
//
// Usage:
//   GIVEN("a vector pipeline") {
//     std::vector<TickInput> input = ...;
//     run_jit_vs_interpreter_parity(json, input,
//       [](Program& p, const TickInput& tick) {
//         p.send_vector(tick.t, tick.values.data(), tick.values.size());
//       });
//   }
//
// On mismatch the first divergence's index, timestamps, port ids, and value
// bit patterns are reported via CAPTURE() before the REQUIRE fails.
inline void run_jit_vs_interpreter_parity(
    const std::string& program_json,
    const std::vector<TickInput>& inputs,
    std::function<void(Program&, const TickInput&)> drive) {
  std::vector<ParityEmission> jit_emissions;
  std::vector<ParityEmission> interp_emissions;

  // JIT pass first. force_interpreter_flag_ is off by default; if env-var
  // RTBOT_DISABLE_JIT is set we still get a useful interpreter-vs-interpreter
  // run, the helper will REQUIRE equality regardless.
  Program::set_force_interpreter_for_testing(false);
  {
    Program prog(program_json);
    jit_emissions = drive_and_capture(prog, inputs, drive);
  }

  // Interpreter pass: force the runtime flag on so the JIT path is skipped
  // for every Program built while it is set.
  Program::set_force_interpreter_for_testing(true);
  {
    Program prog(program_json);
    REQUIRE_FALSE(prog.using_jit());
    interp_emissions = drive_and_capture(prog, inputs, drive);
  }
  Program::set_force_interpreter_for_testing(false);

  // Compare emission counts first, then per-emission bit-exact values.
  REQUIRE(jit_emissions.size() == interp_emissions.size());

  const std::size_t n = jit_emissions.size();
  for (std::size_t i = 0; i < n; ++i) {
    const auto& a = jit_emissions[i];
    const auto& b = interp_emissions[i];
    if (a.t != b.t || a.port_id != b.port_id ||
        a.values.size() != b.values.size()) {
      CAPTURE(i, a.t, b.t, a.port_id, b.port_id, a.values.size(),
              b.values.size());
      REQUIRE(a.t == b.t);
      REQUIRE(a.port_id == b.port_id);
      REQUIRE(a.values.size() == b.values.size());
    }
    for (std::size_t k = 0; k < a.values.size(); ++k) {
      const std::uint64_t ab = dbits(a.values[k]);
      const std::uint64_t bb = dbits(b.values[k]);
      if (ab != bb) {
        std::ostringstream oss;
        oss << "first divergence at emission " << i << " value " << k
            << ": jit=0x" << std::hex << ab << " interp=0x" << bb
            << std::dec << " (jit=" << a.values[k] << " interp=" << b.values[k]
            << " t=" << a.t << ")";
        FAIL(oss.str());
      }
    }
  }
}

}  // namespace rtbot::test

#endif  // RTBOT_API_TEST_PARITY_HELPER_H
