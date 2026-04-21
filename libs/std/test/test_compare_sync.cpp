#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/Collector.h"
#include "rtbot/std/CompareSync.h"

using namespace rtbot;

SCENARIO("CompareSyncGT emits true when i1 > i2 at same timestamp", "[compare_sync]") {
  SECTION("Basic comparisons") {
    auto cmp = std::make_shared<CompareSyncGT>("cmp1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);
    REQUIRE(cmp->type_name() == "CompareSyncGT");

    cmp->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{7.0}), 1);
    cmp->receive_data(create_message<NumberData>(3, NumberData{9.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{9.0}), 1);
    cmp->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 3);

    auto* m0 = dynamic_cast<const Message<BooleanData>*>(out[0].get());
    REQUIRE(m0->time == 1);
    REQUIRE(m0->data.value == true);   // 10 > 5

    auto* m1 = dynamic_cast<const Message<BooleanData>*>(out[1].get());
    REQUIRE(m1->time == 2);
    REQUIRE(m1->data.value == false);  // 3 > 7

    auto* m2 = dynamic_cast<const Message<BooleanData>*>(out[2].get());
    REQUIRE(m2->time == 3);
    REQUIRE(m2->data.value == false);  // 9 > 9
  }
}

SCENARIO("CompareSyncLT emits true when i1 < i2", "[compare_sync]") {
  SECTION("Basic comparison") {
    auto cmp = std::make_shared<CompareSyncLT>("cmp1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);
    REQUIRE(cmp->type_name() == "CompareSyncLT");

    cmp->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == true);
  }
}

SCENARIO("CompareSyncGTE emits true when i1 >= i2", "[compare_sync]") {
  SECTION("Equal and greater") {
    auto cmp = std::make_shared<CompareSyncGTE>("cmp1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);
    REQUIRE(cmp->type_name() == "CompareSyncGTE");

    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{6.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(3, NumberData{4.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{5.0}), 1);
    cmp->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 3);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == true);   // 5 >= 5
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[1].get())->data.value == true);   // 6 >= 5
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[2].get())->data.value == false);  // 4 >= 5
  }
}

SCENARIO("CompareSyncLTE emits true when i1 <= i2", "[compare_sync]") {
  SECTION("Equal and less") {
    auto cmp = std::make_shared<CompareSyncLTE>("cmp1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);
    REQUIRE(cmp->type_name() == "CompareSyncLTE");

    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(3, NumberData{7.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{5.0}), 1);
    cmp->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 3);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == true);   // 5 <= 5
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[1].get())->data.value == true);   // 3 <= 5
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[2].get())->data.value == false);  // 7 <= 5
  }
}

SCENARIO("CompareSyncEQ with tolerance", "[compare_sync]") {
  SECTION("Within and outside tolerance") {
    auto cmp = std::make_shared<CompareSyncEQ>("cmp1", 0.1);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);
    REQUIRE(cmp->type_name() == "CompareSyncEQ");
    REQUIRE(cmp->get_tolerance() == 0.1);

    cmp->receive_data(create_message<NumberData>(1, NumberData{5.05}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.2}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
    cmp->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 2);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[1].get())->data.value == false);
  }

  SECTION("Zero tolerance — exact match") {
    auto cmp = std::make_shared<CompareSyncEQ>("cmp1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);

    cmp->receive_data(create_message<NumberData>(1, NumberData{7.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{7.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{7.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{7.1}), 1);
    cmp->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 2);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[1].get())->data.value == false);
  }
}

SCENARIO("CompareSyncNEQ with tolerance", "[compare_sync]") {
  SECTION("Difference larger than tolerance") {
    auto cmp = std::make_shared<CompareSyncNEQ>("cmp1", 0.1);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);
    REQUIRE(cmp->type_name() == "CompareSyncNEQ");

    cmp->receive_data(create_message<NumberData>(1, NumberData{5.05}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.2}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
    cmp->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 2);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == false);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[1].get())->data.value == true);
  }
}

SCENARIO("CompareSync handles out-of-order arrival via timestamp sync", "[compare_sync]") {
  SECTION("i2 arrives before i1 — blocks until paired") {
    auto cmp = std::make_shared<CompareSyncGT>("cmp1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);

    cmp->receive_data(create_message<NumberData>(1, NumberData{3.0}), 1);
    cmp->execute();
    REQUIRE(col->get_data_queue(0).empty());  // waiting for i1 at t=1

    cmp->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    cmp->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 1);
    auto* m = dynamic_cast<const Message<BooleanData>*>(out[0].get());
    REQUIRE(m->time == 1);
    REQUIRE(m->data.value == true);  // 10 > 3
  }

  SECTION("Mismatched timestamp on i1 is dropped during sync") {
    auto cmp = std::make_shared<CompareSyncGT>("cmp1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    cmp->connect(col, 0, 0);

    cmp->receive_data(create_message<NumberData>(1, NumberData{99.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{50.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{40.0}), 0);
    cmp->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 1);
    auto* m = dynamic_cast<const Message<BooleanData>*>(out[0].get());
    REQUIRE(m->time == 2);
    REQUIRE(m->data.value == false);  // 40 > 50 = false
  }
}

SCENARIO("CompareSync serialization roundtrip", "[compare_sync][State]") {
  SECTION("CompareSyncGT collect and restore") {
    auto cmp = std::make_shared<CompareSyncGT>("cmp1");
    cmp->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->execute();

    auto state = cmp->collect();
    auto restored = std::make_shared<CompareSyncGT>("cmp1");
    auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    restored->connect(rcol, 0, 0);
    restored->restore_data_from_json(state);
    REQUIRE(*restored == *cmp);

    restored->receive_data(create_message<NumberData>(2, NumberData{1.0}), 0);
    restored->receive_data(create_message<NumberData>(2, NumberData{2.0}), 1);
    restored->execute();

    auto& out = rcol->get_data_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == false);
  }

  SECTION("CompareSyncEQ with tolerance collect and restore") {
    auto cmp = std::make_shared<CompareSyncEQ>("cmp1", 0.5);
    cmp->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{3.2}), 1);
    cmp->execute();

    auto state = cmp->collect();
    auto restored = std::make_shared<CompareSyncEQ>("cmp1", 0.5);
    restored->restore_data_from_json(state);
    REQUIRE(*restored == *cmp);
  }
}

