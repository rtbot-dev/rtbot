#include <catch2/catch.hpp>

#include "rtbot/OperatorJson.h"
#include "rtbot/std/FusedExpressionVector.h"

using namespace rtbot;
using namespace fused_op;

SCENARIO("FusedExpressionVector evaluates bytecode on vector input", "[fused_expression_vector]") {
  auto op = make_fused_expression_vector(
      "fev1", 1, {INPUT, 0, INPUT, 1, ADD, END}, {});

  op->receive_data(
      create_message<VectorNumberData>(1, VectorNumberData{{2.0, 3.5}}), 0);
  op->execute();

  auto& q = op->get_output_queue(0);
  REQUIRE(q.size() == 1);
  auto* msg = dynamic_cast<const Message<VectorNumberData>*>(q[0].get());
  REQUIRE(msg != nullptr);
  REQUIRE(msg->time == 1);
  REQUIRE(msg->data.values->size() == 1);
  REQUIRE((*msg->data.values)[0] == Approx(5.5));
}

SCENARIO("FusedExpressionVector supports multiple outputs", "[fused_expression_vector]") {
  auto op = make_fused_expression_vector(
      "fev2", 2,
      {INPUT, 0, END, INPUT, 1, INPUT, 2, MUL, END}, {});

  op->receive_data(
      create_message<VectorNumberData>(10, VectorNumberData{{4.0, 1.5, 2.0}}),
      0);
  op->execute();

  auto& q = op->get_output_queue(0);
  REQUIRE(q.size() == 1);
  auto* msg = dynamic_cast<const Message<VectorNumberData>*>(q[0].get());
  REQUIRE(msg != nullptr);
  REQUIRE(msg->data.values->size() == 2);
  REQUIRE((*msg->data.values)[0] == Approx(4.0));
  REQUIRE((*msg->data.values)[1] == Approx(3.0));
}

SCENARIO("FusedExpressionVector JSON roundtrip", "[fused_expression_vector]") {
  auto original = make_fused_expression_vector(
      "fev_json", 2,
      {INPUT, 0, END, INPUT, 1, CONST, 0, ADD, END},
      {10.0},
      {0.0, 0.0});

  auto encoded = OperatorJson::write_op(original);
  auto restored = OperatorJson::read_op(encoded);
  auto* fev = dynamic_cast<FusedExpressionVector*>(restored.get());

  REQUIRE(fev != nullptr);
  REQUIRE(fev->type_name() == "FusedExpressionVector");
  REQUIRE(fev->get_num_outputs() == 2);
  REQUIRE(fev->get_bytecode() == std::vector<double>{
                                     INPUT, 0, END, INPUT, 1, CONST, 0, ADD, END});
  REQUIRE(fev->get_constants() == std::vector<double>{10.0});
  REQUIRE(fev->get_state_init() == std::vector<double>{0.0, 0.0});
}
