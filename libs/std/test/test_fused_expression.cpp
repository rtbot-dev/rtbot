#include <catch2/catch.hpp>
#include <cmath>
#include <memory>
#include <type_traits>

#include "rtbot/std/FusedExpression.h"

using namespace rtbot;
using namespace fused_op;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static auto fe(std::string id, size_t num_ports, size_t num_outputs,
               std::vector<double> bytecode,
               std::vector<double> constants = {}) {
  return make_fused_expression(std::move(id), num_ports, num_outputs,
                               std::move(bytecode), std::move(constants));
}

static void feed(FusedExpression& op, timestamp_t t,
                 std::vector<double> values) {
  for (size_t i = 0; i < values.size(); ++i) {
    op.receive_data(create_message<NumberData>(t, NumberData{values[i]}), i);
  }
  op.execute();
}

static std::vector<double> output_values(FusedExpression& op, size_t idx = 0) {
  auto& q = op.get_output_queue(0);
  if (q.empty()) return {};
  auto* msg = dynamic_cast<const Message<VectorNumberData>*>(q[idx].get());
  if (!msg) return {};
  return *msg->data.values;
}

// =========================================================================
// Basic arithmetic
// =========================================================================

SCENARIO("FusedExpression basic arithmetic", "[fused_expression]") {
  SECTION("Addition: a + b") {
    // bytecode: INPUT 0, INPUT 1, ADD, END
    auto op = fe("fe1", 2, 1, {INPUT, 0, INPUT, 1, ADD, END});
    feed(*op, 1, {3.0, 7.0});

    auto& q = op->get_output_queue(0);
    REQUIRE(q.size() == 1);
    auto vals = output_values(*op);
    REQUIRE(vals.size() == 1);
    REQUIRE(vals[0] == Approx(10.0));
  }

  SECTION("Subtraction: a - b") {
    auto op = fe("fe1", 2, 1, {INPUT, 0, INPUT, 1, SUB, END});
    feed(*op, 1, {10.0, 3.0});

    auto vals = output_values(*op);
    REQUIRE(vals.size() == 1);
    REQUIRE(vals[0] == Approx(7.0));
  }

  SECTION("Multiplication: a * b") {
    auto op = fe("fe1", 2, 1, {INPUT, 0, INPUT, 1, MUL, END});
    feed(*op, 1, {4.0, 5.0});

    auto vals = output_values(*op);
    REQUIRE(vals.size() == 1);
    REQUIRE(vals[0] == Approx(20.0));
  }

  SECTION("Division: a / b") {
    auto op = fe("fe1", 2, 1, {INPUT, 0, INPUT, 1, DIV, END});
    feed(*op, 1, {15.0, 3.0});

    auto vals = output_values(*op);
    REQUIRE(vals.size() == 1);
    REQUIRE(vals[0] == Approx(5.0));
  }

  SECTION("Power: a^b") {
    auto op = fe("fe1", 2, 1, {INPUT, 0, INPUT, 1, POW, END});
    feed(*op, 1, {2.0, 10.0});

    auto vals = output_values(*op);
    REQUIRE(vals.size() == 1);
    REQUIRE(vals[0] == Approx(1024.0));
  }
}

// =========================================================================
// Unary math functions
// =========================================================================

SCENARIO("FusedExpression unary math functions", "[fused_expression]") {
  SECTION("ABS") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, ABS, END});
    feed(*op, 1, {-42.0});
    REQUIRE(output_values(*op)[0] == Approx(42.0));
  }

  SECTION("SQRT") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, SQRT, END});
    feed(*op, 1, {25.0});
    REQUIRE(output_values(*op)[0] == Approx(5.0));
  }

  SECTION("LOG (natural)") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, LOG, END});
    feed(*op, 1, {std::exp(1.0)});
    REQUIRE(output_values(*op)[0] == Approx(1.0));
  }

  SECTION("LOG10") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, LOG10, END});
    feed(*op, 1, {1000.0});
    REQUIRE(output_values(*op)[0] == Approx(3.0));
  }

  SECTION("EXP") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, EXP, END});
    feed(*op, 1, {0.0});
    REQUIRE(output_values(*op)[0] == Approx(1.0));
  }

  SECTION("SIN") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, SIN, END});
    feed(*op, 1, {M_PI / 2.0});
    REQUIRE(output_values(*op)[0] == Approx(1.0));
  }

  SECTION("COS") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, COS, END});
    feed(*op, 1, {0.0});
    REQUIRE(output_values(*op)[0] == Approx(1.0));
  }

  SECTION("TAN") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, TAN, END});
    feed(*op, 1, {M_PI / 4.0});
    REQUIRE(output_values(*op)[0] == Approx(1.0));
  }

  SECTION("SIGN positive") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, SIGN, END});
    feed(*op, 1, {42.0});
    REQUIRE(output_values(*op)[0] == Approx(1.0));
  }

  SECTION("SIGN negative") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, SIGN, END});
    feed(*op, 1, {-42.0});
    REQUIRE(output_values(*op)[0] == Approx(-1.0));
  }

  SECTION("SIGN zero") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, SIGN, END});
    feed(*op, 1, {0.0});
    REQUIRE(output_values(*op)[0] == Approx(0.0));
  }

  SECTION("FLOOR") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, FLOOR, END});
    feed(*op, 1, {3.7});
    REQUIRE(output_values(*op)[0] == Approx(3.0));
  }

  SECTION("CEIL") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, CEIL, END});
    feed(*op, 1, {3.2});
    REQUIRE(output_values(*op)[0] == Approx(4.0));
  }

  SECTION("ROUND") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, ROUND, END});
    feed(*op, 1, {3.5});
    REQUIRE(output_values(*op)[0] == Approx(4.0));
  }

  SECTION("NEG") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, NEG, END});
    feed(*op, 1, {7.0});
    REQUIRE(output_values(*op)[0] == Approx(-7.0));
  }
}

// =========================================================================
// Constants
// =========================================================================

SCENARIO("FusedExpression constants", "[fused_expression]") {
  SECTION("Constant in arithmetic: a + 100") {
    // bytecode: INPUT 0, CONST 0, ADD, END
    // constants: [100.0]
    auto op = fe("fe1", 1, 1, {INPUT, 0, CONST, 0, ADD, END}, {100.0});
    feed(*op, 1, {42.0});
    REQUIRE(output_values(*op)[0] == Approx(142.0));
  }

  SECTION("Multiple constants: a * 2 + 10") {
    // bytecode: INPUT 0, CONST 0, MUL, CONST 1, ADD, END
    // constants: [2.0, 10.0]
    auto op = fe("fe1", 1, 1, {INPUT, 0, CONST, 0, MUL, CONST, 1, ADD, END},
                 {2.0, 10.0});
    feed(*op, 1, {5.0});
    REQUIRE(output_values(*op)[0] == Approx(20.0));
  }
}

// =========================================================================
// Multi-output expressions
// =========================================================================

SCENARIO("FusedExpression multi-output", "[fused_expression]") {
  SECTION("Two outputs from same inputs: (a+b, a-b)") {
    // bytecode: INPUT 0, INPUT 1, ADD, END, INPUT 0, INPUT 1, SUB, END
    auto op = fe("fe1", 2, 2,
                 {INPUT, 0, INPUT, 1, ADD, END, INPUT, 0, INPUT, 1, SUB, END});
    feed(*op, 1, {10.0, 3.0});

    auto vals = output_values(*op);
    REQUIRE(vals.size() == 2);
    REQUIRE(vals[0] == Approx(13.0));
    REQUIRE(vals[1] == Approx(7.0));
  }

  SECTION("Three outputs: (a, b, a*b)") {
    auto op = fe("fe1", 2, 3,
                 {INPUT, 0, END, INPUT, 1, END, INPUT, 0, INPUT, 1, MUL, END});
    feed(*op, 1, {4.0, 5.0});

    auto vals = output_values(*op);
    REQUIRE(vals.size() == 3);
    REQUIRE(vals[0] == Approx(4.0));
    REQUIRE(vals[1] == Approx(5.0));
    REQUIRE(vals[2] == Approx(20.0));
  }
}

// =========================================================================
// Complex expressions
// =========================================================================

SCENARIO("FusedExpression complex expressions", "[fused_expression]") {
  SECTION("Nested: (a + b) * (a - b)") {
    // (a+b)*(a-b) = a^2 - b^2
    // bytecode: INPUT 0, INPUT 1, ADD, INPUT 0, INPUT 1, SUB, MUL, END
    auto op = fe("fe1", 2, 1,
                 {INPUT, 0, INPUT, 1, ADD, INPUT, 0, INPUT, 1, SUB, MUL, END});
    feed(*op, 1, {5.0, 3.0});

    REQUIRE(output_values(*op)[0] == Approx(16.0));  // 25 - 9
  }

  SECTION("Chained unary: SQRT(ABS(a))") {
    auto op = fe("fe1", 1, 1, {INPUT, 0, ABS, SQRT, END});
    feed(*op, 1, {-16.0});
    REQUIRE(output_values(*op)[0] == Approx(4.0));
  }

  SECTION("Mixed binary/unary: ABS(a - b) + SQRT(c)") {
    // bytecode: INPUT 0, INPUT 1, SUB, ABS, INPUT 2, SQRT, ADD, END
    auto op = fe("fe1", 3, 1,
                 {INPUT, 0, INPUT, 1, SUB, ABS, INPUT, 2, SQRT, ADD, END});
    feed(*op, 1, {3.0, 8.0, 25.0});

    REQUIRE(output_values(*op)[0] == Approx(10.0));  // |3-8| + sqrt(25) = 5+5
  }
}

// =========================================================================
// Timestamp synchronization (inherited from VectorCompose/Join)
// =========================================================================

SCENARIO("FusedExpression timestamp sync", "[fused_expression]") {
  SECTION("Matching timestamps produce output") {
    auto op = fe("fe1", 2, 1, {INPUT, 0, INPUT, 1, ADD, END});

    op->receive_data(create_message<NumberData>(100, NumberData{5.0}), 0);
    op->receive_data(create_message<NumberData>(100, NumberData{3.0}), 1);
    op->execute();

    auto& q = op->get_output_queue(0);
    REQUIRE(q.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(q[0].get());
    REQUIRE(msg->time == 100);
    REQUIRE((*msg->data.values)[0] == Approx(8.0));
  }

  SECTION("Mismatched timestamps — stale dropped, sync on later") {
    auto op = fe("fe1", 2, 1, {INPUT, 0, INPUT, 1, ADD, END});

    // Port 0: t=1, Port 1: t=2 → t=1 on port 0 is stale, dropped
    op->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    op->receive_data(create_message<NumberData>(2, NumberData{20.0}), 1);
    // Now port 0 sends t=2 → sync happens
    op->receive_data(create_message<NumberData>(2, NumberData{30.0}), 0);
    op->execute();

    auto& q = op->get_output_queue(0);
    REQUIRE(q.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(q[0].get());
    REQUIRE(msg->time == 2);
    REQUIRE((*msg->data.values)[0] == Approx(50.0));  // 30 + 20
  }

  SECTION("Multiple synced messages in sequence") {
    auto op = fe("fe1", 2, 1, {INPUT, 0, INPUT, 1, MUL, END});

    for (int t = 1; t <= 3; ++t) {
      op->receive_data(
          create_message<NumberData>(t, NumberData{static_cast<double>(t)}), 0);
      op->receive_data(
          create_message<NumberData>(t, NumberData{static_cast<double>(t + 1)}),
          1);
    }
    op->execute();

    auto& q = op->get_output_queue(0);
    REQUIRE(q.size() == 3);
    // t=1: 1*2=2, t=2: 2*3=6, t=3: 3*4=12
    for (int i = 0; i < 3; ++i) {
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(q[i].get());
      double expected = (i + 1) * (i + 2);
      REQUIRE((*msg->data.values)[0] == Approx(expected));
    }
  }
}

// =========================================================================
// Validation
// =========================================================================

SCENARIO("FusedExpression validation", "[fused_expression]") {
  SECTION("Zero outputs throws") {
    REQUIRE_THROWS_AS(fe("fe1", 2, 0, {}), std::runtime_error);
  }

  SECTION("END count mismatch throws") {
    // num_outputs=2 but only 1 END in bytecode
    REQUIRE_THROWS_AS(fe("fe1", 2, 2, {INPUT, 0, END}), std::runtime_error);
  }

  SECTION("END count exceeds num_outputs throws") {
    // num_outputs=1 but 2 END markers
    REQUIRE_THROWS_AS(fe("fe1", 1, 1, {INPUT, 0, END, INPUT, 0, END}),
                      std::runtime_error);
  }

  SECTION("Const index equal to END opcode value does not miscount") {
    // Regression: constant index 20 matches fused_op::END (20.0).
    // Validation must walk opcodes respecting argument structure.
    // Build 21 constants so that one CONST argument == 20.
    std::vector<double> constants(21, 1.0);
    // Bytecode: CONST 20 (index=20, same double value as END), END
    std::vector<double> bytecode = {CONST, 20, END};
    REQUIRE_NOTHROW(fe("fe1", 1, 1, bytecode, constants));

    // Also verify it evaluates correctly
    auto op = fe("fe1", 1, 1, bytecode, constants);
    feed(*op, 1, {99.0});  // input value ignored — expression is just const[20]
    auto vals = output_values(*op);
    REQUIRE(vals.size() == 1);
    REQUIRE(vals[0] == Approx(1.0));
  }

  SECTION("Input index equal to END opcode value does not miscount") {
    // 21 input ports, INPUT 20 references port index 20
    std::vector<double> bytecode = {INPUT, 20, END};
    // Need 21 ports
    auto op = make_fused_expression("fe1", 21, 1, bytecode, {});
    REQUIRE(op->get_num_outputs() == 1);
  }
}

// =========================================================================
// Type hierarchy
// =========================================================================

SCENARIO("FusedExpression type hierarchy", "[fused_expression]") {
  SECTION("Inherits from VectorCompose and Join") {
    STATIC_REQUIRE(std::is_base_of_v<VectorCompose, FusedExpression>);
    STATIC_REQUIRE(std::is_base_of_v<Join, FusedExpression>);

    auto op = fe("fe1", 2, 1, {INPUT, 0, INPUT, 1, ADD, END});
    REQUIRE(op->type_name() == "FusedExpression");
    REQUIRE(op->get_num_ports() == 2);
    REQUIRE(op->get_num_outputs() == 1);
  }
}

// =========================================================================
// Parameter accessors
// =========================================================================

SCENARIO("FusedExpression parameter accessors", "[fused_expression]") {
  SECTION("Bytecode and constants are retrievable") {
    std::vector<double> bytecode = {INPUT, 0, INPUT, 1, MUL, CONST, 0, ADD, END};
    std::vector<double> constants = {42.0};
    auto op = fe("fe1", 2, 1, bytecode, constants);

    REQUIRE(op->type_name() == "FusedExpression");
    REQUIRE(op->get_num_ports() == 2);
    REQUIRE(op->get_num_outputs() == 1);
    REQUIRE(op->get_bytecode() == bytecode);
    REQUIRE(op->get_constants() == constants);
  }

  SECTION("Empty constants") {
    std::vector<double> bytecode = {INPUT, 0, INPUT, 1, ADD, END};
    auto op = fe("fe1", 2, 1, bytecode);
    REQUIRE(op->get_constants().empty());
  }
}

// =========================================================================
// State save/restore roundtrip
// =========================================================================

SCENARIO("FusedExpression state save/restore", "[fused_expression][State]") {
  SECTION("Collect and restore with buffered state") {
    auto op = fe("fe1", 2, 1, {INPUT, 0, INPUT, 1, ADD, END});

    // Send to only one port — creates buffered state
    op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);

    auto state = op->collect();
    auto restored = fe("fe1", 2, 1, {INPUT, 0, INPUT, 1, ADD, END});
    restored->restore_data_from_json(state);

    REQUIRE(*restored == *op);

    // Complete the sync on restored
    restored->receive_data(create_message<NumberData>(1, NumberData{10.0}), 1);
    restored->execute();

    auto& output = restored->get_output_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg =
        dynamic_cast<const Message<VectorNumberData>*>(output[0].get());
    REQUIRE(msg->time == 1);
    REQUIRE((*msg->data.values)[0] == Approx(15.0));
  }
}
