#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/std/CompareScalar.h"

using namespace rtbot;

SCENARIO("CompareGT produces boolean output for every message", "[compare_scalar]") {
  SECTION("Basic comparisons") {
    auto cmp = make_compare_gt("cmp1", 30.0);

    REQUIRE(cmp->type_name() == "CompareGT");
    REQUIRE(cmp->get_value() == 30.0);

    cmp->receive_data(create_message<NumberData>(1, NumberData{31.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{29.0}), 0);
    cmp->execute();

    auto& output = cmp->get_output_queue(0);
    REQUIRE(output.size() == 3);

    auto* msg0 = dynamic_cast<const Message<BooleanData>*>(output[0].get());
    REQUIRE(msg0->time == 1);
    REQUIRE(msg0->data.value == true);

    auto* msg1 = dynamic_cast<const Message<BooleanData>*>(output[1].get());
    REQUIRE(msg1->time == 2);
    REQUIRE(msg1->data.value == false);

    auto* msg2 = dynamic_cast<const Message<BooleanData>*>(output[2].get());
    REQUIRE(msg2->time == 3);
    REQUIRE(msg2->data.value == false);
  }
}

SCENARIO("CompareLT produces boolean output", "[compare_scalar]") {
  SECTION("Basic comparisons") {
    auto cmp = make_compare_lt("cmp1", 30.0);

    REQUIRE(cmp->type_name() == "CompareLT");

    cmp->receive_data(create_message<NumberData>(1, NumberData{29.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{31.0}), 0);
    cmp->execute();

    auto& output = cmp->get_output_queue(0);
    REQUIRE(output.size() == 3);

    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == false);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[2].get())->data.value == false);
  }
}

SCENARIO("CompareGTE produces boolean output", "[compare_scalar]") {
  SECTION("Basic comparisons") {
    auto cmp = make_compare_gte("cmp1", 30.0);

    REQUIRE(cmp->type_name() == "CompareGTE");

    cmp->receive_data(create_message<NumberData>(1, NumberData{31.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{29.0}), 0);
    cmp->execute();

    auto& output = cmp->get_output_queue(0);
    REQUIRE(output.size() == 3);

    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[2].get())->data.value == false);
  }
}

SCENARIO("CompareLTE produces boolean output", "[compare_scalar]") {
  SECTION("Basic comparisons") {
    auto cmp = make_compare_lte("cmp1", 30.0);

    REQUIRE(cmp->type_name() == "CompareLTE");

    cmp->receive_data(create_message<NumberData>(1, NumberData{29.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{31.0}), 0);
    cmp->execute();

    auto& output = cmp->get_output_queue(0);
    REQUIRE(output.size() == 3);

    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[2].get())->data.value == false);
  }
}

SCENARIO("CompareEQ with tolerance", "[compare_scalar]") {
  SECTION("Within tolerance") {
    auto cmp = make_compare_eq("cmp1", 30.0, 0.01);

    REQUIRE(cmp->type_name() == "CompareEQ");
    REQUIRE(cmp->get_value() == 30.0);
    REQUIRE(cmp->get_tolerance() == 0.01);

    cmp->receive_data(create_message<NumberData>(1, NumberData{30.001}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.1}), 0);
    cmp->execute();

    auto& output = cmp->get_output_queue(0);
    REQUIRE(output.size() == 2);

    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == false);
  }

  SECTION("Zero tolerance — exact match") {
    auto cmp = make_compare_eq("cmp1", 5.0, 0.0);

    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.1}), 0);
    cmp->execute();

    auto& output = cmp->get_output_queue(0);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == false);
  }
}

SCENARIO("CompareNEQ with tolerance", "[compare_scalar]") {
  SECTION("Outside tolerance") {
    auto cmp = make_compare_neq("cmp1", 30.0, 0.01);

    REQUIRE(cmp->type_name() == "CompareNEQ");

    cmp->receive_data(create_message<NumberData>(1, NumberData{30.001}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.1}), 0);
    cmp->execute();

    auto& output = cmp->get_output_queue(0);
    REQUIRE(output.size() == 2);

    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == false);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == true);
  }
}

SCENARIO("CompareScalar serialization roundtrip", "[compare_scalar][State]") {
  SECTION("CompareGT collect and restore") {
    auto cmp = make_compare_gt("cmp1", 30.0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{31.0}), 0);
    cmp->execute();

    Bytes state = cmp->collect();
    auto restored = make_compare_gt("cmp1", 30.0);
    auto it = state.cbegin();
    restored->restore(it);

    REQUIRE(*restored == *cmp);

    restored->clear_all_output_ports();
    restored->receive_data(create_message<NumberData>(2, NumberData{25.0}), 0);
    restored->execute();

    auto& output = restored->get_output_queue(0);
    REQUIRE(output.size() == 1);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == false);
  }

  SECTION("CompareEQ collect and restore") {
    auto cmp = make_compare_eq("cmp1", 10.0, 0.5);
    cmp->receive_data(create_message<NumberData>(1, NumberData{10.3}), 0);
    cmp->execute();

    Bytes state = cmp->collect();
    auto restored = make_compare_eq("cmp1", 10.0, 0.5);
    auto it = state.cbegin();
    restored->restore(it);

    REQUIRE(*restored == *cmp);
  }
}
