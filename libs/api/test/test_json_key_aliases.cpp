// test_json_key_aliases.cpp
//
// Regression tests for cross-parser JSON key unification (RB-495). Each of
// these scenarios drives the framework parser (OperatorJson::read_op) with
// both the canonical and legacy alias forms and asserts that the resulting
// operators are equivalent.

#include <catch2/catch.hpp>

#include <nlohmann/json.hpp>

#include "rtbot/OperatorJson.h"
#include "rtbot/std/Difference.h"
#include "rtbot/std/FilterScalar.h"
#include "rtbot/std/FiniteImpulseResponse.h"
#include "rtbot/std/InfiniteImpulseResponse.h"
#include "rtbot/std/ResamplerHermite.h"

using namespace rtbot;

SCENARIO("FiniteImpulseResponse accepts both 'coefficients' and 'coeff'",
         "[operator-json][key-aliases][fir]") {
  GIVEN("Two equivalent FIR JSON forms (canonical and legacy)") {
    const std::string canonical = R"({
      "type": "FiniteImpulseResponse", "id": "fir1",
      "coefficients": [0.1, 0.2, 0.4, 0.2, 0.1]
    })";
    const std::string legacy = R"({
      "type": "FiniteImpulseResponse", "id": "fir1",
      "coeff": [0.1, 0.2, 0.4, 0.2, 0.1]
    })";

    auto op_canonical = OperatorJson::read_op(canonical);
    auto op_legacy = OperatorJson::read_op(legacy);

    THEN("Both produce a FiniteImpulseResponse with identical coefficients") {
      auto fir_c = std::dynamic_pointer_cast<FiniteImpulseResponse>(op_canonical);
      auto fir_l = std::dynamic_pointer_cast<FiniteImpulseResponse>(op_legacy);
      REQUIRE(fir_c != nullptr);
      REQUIRE(fir_l != nullptr);
      REQUIRE(fir_c->get_coefficients() == fir_l->get_coefficients());
    }
  }
}

SCENARIO("InfiniteImpulseResponse accepts b/a, b_coeffs/a_coeffs and b_coefficients/a_coefficients",
         "[operator-json][key-aliases][iir]") {
  const std::vector<double> b = {0.2, 0.4, 0.2};
  const std::vector<double> a = {-0.5, 0.1};

  GIVEN("Three equivalent IIR JSON forms") {
    const std::string canonical = R"({
      "type": "InfiniteImpulseResponse", "id": "iir1",
      "b_coefficients": [0.2, 0.4, 0.2],
      "a_coefficients": [-0.5, 0.1]
    })";
    const std::string mid_legacy = R"({
      "type": "InfiniteImpulseResponse", "id": "iir1",
      "b_coeffs": [0.2, 0.4, 0.2],
      "a_coeffs": [-0.5, 0.1]
    })";
    const std::string schema_legacy = R"({
      "type": "InfiniteImpulseResponse", "id": "iir1",
      "b": [0.2, 0.4, 0.2],
      "a": [-0.5, 0.1]
    })";

    auto op_c = std::dynamic_pointer_cast<InfiniteImpulseResponse>(
        OperatorJson::read_op(canonical));
    auto op_m = std::dynamic_pointer_cast<InfiniteImpulseResponse>(
        OperatorJson::read_op(mid_legacy));
    auto op_s = std::dynamic_pointer_cast<InfiniteImpulseResponse>(
        OperatorJson::read_op(schema_legacy));

    THEN("All three produce identical IIR operators") {
      REQUIRE(op_c != nullptr);
      REQUIRE(op_m != nullptr);
      REQUIRE(op_s != nullptr);
      REQUIRE(op_c->get_b_coeffs() == b);
      REQUIRE(op_c->get_a_coeffs() == a);
      REQUIRE(op_m->get_b_coeffs() == b);
      REQUIRE(op_m->get_a_coeffs() == a);
      REQUIRE(op_s->get_b_coeffs() == b);
      REQUIRE(op_s->get_a_coeffs() == a);
    }
  }
}

SCENARIO("EqualTo and NotEqualTo accept both 'tolerance' and 'epsilon'",
         "[operator-json][key-aliases][filter-scalar]") {
  GIVEN("EqualTo with canonical 'tolerance' vs legacy 'epsilon'") {
    auto op_t = std::dynamic_pointer_cast<EqualTo>(OperatorJson::read_op(
        R"({"type":"EqualTo","id":"eq1","value":7.5,"tolerance":0.5})"));
    auto op_e = std::dynamic_pointer_cast<EqualTo>(OperatorJson::read_op(
        R"({"type":"EqualTo","id":"eq1","value":7.5,"epsilon":0.5})"));

    THEN("Both produce identical EqualTo operators") {
      REQUIRE(op_t != nullptr);
      REQUIRE(op_e != nullptr);
      REQUIRE(op_t->get_value() == Approx(op_e->get_value()));
      REQUIRE(op_t->get_epsilon() == Approx(op_e->get_epsilon()));
      REQUIRE(op_t->get_epsilon() == Approx(0.5));
    }
  }

  GIVEN("NotEqualTo with canonical 'tolerance' vs legacy 'epsilon'") {
    auto op_t = std::dynamic_pointer_cast<NotEqualTo>(OperatorJson::read_op(
        R"({"type":"NotEqualTo","id":"neq1","value":3.0,"tolerance":0.25})"));
    auto op_e = std::dynamic_pointer_cast<NotEqualTo>(OperatorJson::read_op(
        R"({"type":"NotEqualTo","id":"neq1","value":3.0,"epsilon":0.25})"));

    THEN("Both produce identical NotEqualTo operators") {
      REQUIRE(op_t != nullptr);
      REQUIRE(op_e != nullptr);
      REQUIRE(op_t->get_value() == Approx(op_e->get_value()));
      REQUIRE(op_t->get_epsilon() == Approx(op_e->get_epsilon()));
      REQUIRE(op_t->get_epsilon() == Approx(0.25));
    }
  }
}

SCENARIO("ResamplerHermite honours optional t0 from JSON",
         "[operator-json][key-aliases][resampler-hermite]") {
  GIVEN("ResamplerHermite JSON with and without t0") {
    auto op_no_t0 = std::dynamic_pointer_cast<ResamplerHermite>(
        OperatorJson::read_op(
            R"({"type":"ResamplerHermite","id":"rh1","interval":5})"));
    auto op_with_t0 = std::dynamic_pointer_cast<ResamplerHermite>(
        OperatorJson::read_op(
            R"({"type":"ResamplerHermite","id":"rh1","interval":5,"t0":100})"));

    THEN("Only the second carries a t0 value") {
      REQUIRE(op_no_t0 != nullptr);
      REQUIRE(op_with_t0 != nullptr);
      REQUIRE_FALSE(op_no_t0->get_t0().has_value());
      REQUIRE(op_with_t0->get_t0().has_value());
      REQUIRE(op_with_t0->get_t0().value() == 100);
    }
  }
}

SCENARIO("ResamplerConstant t0 round-trips as an integer timestamp",
         "[operator-json][key-aliases][resampler-constant]") {
  GIVEN("ResamplerConstant JSON with a large integer t0") {
    // The schema declares t0 as an integer; verify the framework parser does
    // not silently truncate large timestamp values through a float round-trip.
    const std::int64_t big_t0 = 1'000'000'000'000'000'000LL;
    const std::string json = R"({
      "type":"ResamplerConstant","id":"rc1","interval":5,
      "t0":1000000000000000000
    })";
    auto op = OperatorJson::read_op(json);

    THEN("Round-trip preserves the exact integer t0") {
      auto written = nlohmann::json::parse(OperatorJson::write_op(op));
      REQUIRE(written.contains("t0"));
      REQUIRE(written["t0"].get<std::int64_t>() == big_t0);
    }
  }
}

SCENARIO("Difference honours optional use_oldest_time from JSON",
         "[operator-json][key-aliases][difference]") {
  GIVEN("Difference JSON with and without use_oldest_time") {
    auto op_default = std::dynamic_pointer_cast<Difference>(
        OperatorJson::read_op(R"({"type":"Difference","id":"d1"})"));
    auto op_true = std::dynamic_pointer_cast<Difference>(
        OperatorJson::read_op(
            R"({"type":"Difference","id":"d1","use_oldest_time":true})"));
    auto op_false = std::dynamic_pointer_cast<Difference>(
        OperatorJson::read_op(
            R"({"type":"Difference","id":"d1","use_oldest_time":false})"));

    THEN("Default matches use_oldest_time=true and the false form is honoured") {
      REQUIRE(op_default != nullptr);
      REQUIRE(op_true != nullptr);
      REQUIRE(op_false != nullptr);
      REQUIRE(op_default->get_use_oldest_time() == true);
      REQUIRE(op_true->get_use_oldest_time() == true);
      REQUIRE(op_false->get_use_oldest_time() == false);
    }
  }
}
