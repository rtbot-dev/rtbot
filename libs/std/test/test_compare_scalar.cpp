#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/Collector.h"
#include "rtbot/std/CompareScalar.h"

using namespace rtbot;

SCENARIO("CompareGT produces boolean output for every message", "[compare_scalar]") {
  SECTION("Basic comparisons") {
    auto cmp = std::make_shared<CompareGT>("cmp1", 30.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);

    REQUIRE(cmp->type_name() == "CompareGT");
    REQUIRE(cmp->get_value() == 30.0);

    cmp->receive_data(create_message<NumberData>(1, NumberData{31.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{29.0}), 0);
    cmp->execute();

    auto& output = col->get_data_queue(0);
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
    auto cmp = std::make_shared<CompareLT>("cmp1", 30.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);

    REQUIRE(cmp->type_name() == "CompareLT");

    cmp->receive_data(create_message<NumberData>(1, NumberData{29.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{31.0}), 0);
    cmp->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == 3);

    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == false);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[2].get())->data.value == false);
  }
}

SCENARIO("CompareGTE produces boolean output", "[compare_scalar]") {
  SECTION("Basic comparisons") {
    auto cmp = std::make_shared<CompareGTE>("cmp1", 30.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);

    REQUIRE(cmp->type_name() == "CompareGTE");

    cmp->receive_data(create_message<NumberData>(1, NumberData{31.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{29.0}), 0);
    cmp->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == 3);

    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[2].get())->data.value == false);
  }
}

SCENARIO("CompareLTE produces boolean output", "[compare_scalar]") {
  SECTION("Basic comparisons") {
    auto cmp = std::make_shared<CompareLTE>("cmp1", 30.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);

    REQUIRE(cmp->type_name() == "CompareLTE");

    cmp->receive_data(create_message<NumberData>(1, NumberData{29.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{31.0}), 0);
    cmp->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == 3);

    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[2].get())->data.value == false);
  }
}

SCENARIO("CompareEQ with tolerance", "[compare_scalar]") {
  SECTION("Within tolerance") {
    auto cmp = std::make_shared<CompareEQ>("cmp1", 30.0, 0.01);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);

    REQUIRE(cmp->type_name() == "CompareEQ");
    REQUIRE(cmp->get_value() == 30.0);
    REQUIRE(cmp->get_tolerance() == 0.01);

    cmp->receive_data(create_message<NumberData>(1, NumberData{30.001}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.1}), 0);
    cmp->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == 2);

    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == false);
  }

  SECTION("Zero tolerance — exact match") {
    auto cmp = std::make_shared<CompareEQ>("cmp1", 5.0, 0.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);

    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.1}), 0);
    cmp->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == false);
  }
}

SCENARIO("CompareNEQ with tolerance", "[compare_scalar]") {
  SECTION("Outside tolerance") {
    auto cmp = std::make_shared<CompareNEQ>("cmp1", 30.0, 0.01);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);

    REQUIRE(cmp->type_name() == "CompareNEQ");

    cmp->receive_data(create_message<NumberData>(1, NumberData{30.001}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{30.1}), 0);
    cmp->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == 2);

    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == false);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[1].get())->data.value == true);
  }
}

SCENARIO("CompareScalar serialization roundtrip", "[compare_scalar][State]") {
  SECTION("CompareGT collect and restore") {
    auto cmp = std::make_shared<CompareGT>("cmp1", 30.0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{31.0}), 0);
    cmp->execute();

    auto state = cmp->collect();
    auto restored = std::make_shared<CompareGT>("cmp1", 30.0);
    auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    restored->connect(rcol, 0, 0);
    restored->restore_data_from_json(state);

    REQUIRE(*restored == *cmp);

    restored->receive_data(create_message<NumberData>(2, NumberData{25.0}), 0);
    restored->execute();

    auto& output = rcol->get_data_queue(0);
    REQUIRE(output.size() == 1);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(output[0].get())->data.value == false);
  }

  SECTION("CompareEQ collect and restore") {
    auto cmp = std::make_shared<CompareEQ>("cmp1", 10.0, 0.5);
    cmp->receive_data(create_message<NumberData>(1, NumberData{10.3}), 0);
    cmp->execute();

    auto state = cmp->collect();
    auto restored = std::make_shared<CompareEQ>("cmp1", 10.0, 0.5);
    restored->restore_data_from_json(state);

    REQUIRE(*restored == *cmp);
  }
}
