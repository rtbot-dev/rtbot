#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/std/CompareSync.h"

using namespace rtbot;

SCENARIO("CompareSyncGT emits true when i1 > i2 at same timestamp", "[compare_sync]") {
  SECTION("Basic comparisons") {
    auto cmp = make_compare_sync_gt("cmp1");
    REQUIRE(cmp->type_name() == "CompareSyncGT");

    cmp->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{7.0}), 1);
    cmp->receive_data(create_message<NumberData>(3, NumberData{9.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{9.0}), 1);
    cmp->execute();

    auto& out = cmp->get_output_queue(0);
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
    auto cmp = make_compare_sync_lt("cmp1");
    REQUIRE(cmp->type_name() == "CompareSyncLT");

    cmp->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->execute();

    auto& out = cmp->get_output_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == true);
  }
}

SCENARIO("CompareSyncGTE emits true when i1 >= i2", "[compare_sync]") {
  SECTION("Equal and greater") {
    auto cmp = make_compare_sync_gte("cmp1");
    REQUIRE(cmp->type_name() == "CompareSyncGTE");

    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{6.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(3, NumberData{4.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{5.0}), 1);
    cmp->execute();

    auto& out = cmp->get_output_queue(0);
    REQUIRE(out.size() == 3);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == true);   // 5 >= 5
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[1].get())->data.value == true);   // 6 >= 5
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[2].get())->data.value == false);  // 4 >= 5
  }
}

SCENARIO("CompareSyncLTE emits true when i1 <= i2", "[compare_sync]") {
  SECTION("Equal and less") {
    auto cmp = make_compare_sync_lte("cmp1");
    REQUIRE(cmp->type_name() == "CompareSyncLTE");

    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
    cmp->receive_data(create_message<NumberData>(3, NumberData{7.0}), 0);
    cmp->receive_data(create_message<NumberData>(3, NumberData{5.0}), 1);
    cmp->execute();

    auto& out = cmp->get_output_queue(0);
    REQUIRE(out.size() == 3);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == true);   // 5 <= 5
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[1].get())->data.value == true);   // 3 <= 5
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[2].get())->data.value == false);  // 7 <= 5
  }
}

SCENARIO("CompareSyncEQ with tolerance", "[compare_sync]") {
  SECTION("Within and outside tolerance") {
    auto cmp = make_compare_sync_eq("cmp1", 0.1);
    REQUIRE(cmp->type_name() == "CompareSyncEQ");
    REQUIRE(cmp->get_tolerance() == 0.1);

    // |5.05 - 5.0| = 0.05 <= 0.1 → true
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.05}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    // |5.2 - 5.0| = 0.2 > 0.1 → false
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.2}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
    cmp->execute();

    auto& out = cmp->get_output_queue(0);
    REQUIRE(out.size() == 2);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[1].get())->data.value == false);
  }

  SECTION("Zero tolerance — exact match") {
    auto cmp = make_compare_sync_eq("cmp1");  // default tolerance 0.0
    cmp->receive_data(create_message<NumberData>(1, NumberData{7.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{7.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{7.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{7.1}), 1);
    cmp->execute();

    auto& out = cmp->get_output_queue(0);
    REQUIRE(out.size() == 2);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == true);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[1].get())->data.value == false);
  }
}

SCENARIO("CompareSyncNEQ with tolerance", "[compare_sync]") {
  SECTION("Difference larger than tolerance") {
    auto cmp = make_compare_sync_neq("cmp1", 0.1);
    REQUIRE(cmp->type_name() == "CompareSyncNEQ");

    // |5.05 - 5.0| = 0.05 <= 0.1 → false (within tolerance, so NOT NEQ)
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.05}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    // |5.2 - 5.0| = 0.2 > 0.1 → true (outside tolerance, so NEQ)
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.2}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
    cmp->execute();

    auto& out = cmp->get_output_queue(0);
    REQUIRE(out.size() == 2);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == false);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[1].get())->data.value == true);
  }
}

SCENARIO("CompareSync handles out-of-order arrival via timestamp sync", "[compare_sync]") {
  SECTION("i2 arrives before i1 — blocks until paired") {
    auto cmp = make_compare_sync_gt("cmp1");

    cmp->receive_data(create_message<NumberData>(1, NumberData{3.0}), 1);
    cmp->execute();
    REQUIRE(cmp->get_output_queue(0).empty());  // waiting for i1 at t=1

    cmp->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    cmp->execute();

    auto& out = cmp->get_output_queue(0);
    REQUIRE(out.size() == 1);
    auto* m = dynamic_cast<const Message<BooleanData>*>(out[0].get());
    REQUIRE(m->time == 1);
    REQUIRE(m->data.value == true);  // 10 > 3
  }

  SECTION("Mismatched timestamp on i1 is dropped during sync") {
    auto cmp = make_compare_sync_gt("cmp1");

    // i1 at t=1 (stale), i2 at t=2, then i1 at t=2
    cmp->receive_data(create_message<NumberData>(1, NumberData{99.0}), 0);
    cmp->receive_data(create_message<NumberData>(2, NumberData{50.0}), 1);
    cmp->receive_data(create_message<NumberData>(2, NumberData{40.0}), 0);
    cmp->execute();

    auto& out = cmp->get_output_queue(0);
    REQUIRE(out.size() == 1);
    auto* m = dynamic_cast<const Message<BooleanData>*>(out[0].get());
    REQUIRE(m->time == 2);
    REQUIRE(m->data.value == false);  // 40 > 50 = false
  }
}

SCENARIO("CompareSync serialization roundtrip", "[compare_sync][State]") {
  SECTION("CompareSyncGT collect and restore") {
    auto cmp = make_compare_sync_gt("cmp1");
    cmp->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    cmp->execute();

    Bytes state = cmp->collect();
    auto restored = make_compare_sync_gt("cmp1");
    auto it = state.cbegin();
    restored->restore(it);
    REQUIRE(*restored == *cmp);

    restored->clear_all_output_ports();
    restored->receive_data(create_message<NumberData>(2, NumberData{1.0}), 0);
    restored->receive_data(create_message<NumberData>(2, NumberData{2.0}), 1);
    restored->execute();

    auto& out = restored->get_output_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<BooleanData>*>(out[0].get())->data.value == false);
  }

  SECTION("CompareSyncEQ with tolerance collect and restore") {
    auto cmp = make_compare_sync_eq("cmp1", 0.5);
    cmp->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
    cmp->receive_data(create_message<NumberData>(1, NumberData{3.2}), 1);
    cmp->execute();

    Bytes state = cmp->collect();
    auto restored = make_compare_sync_eq("cmp1", 0.5);
    auto it = state.cbegin();
    restored->restore(it);
    REQUIRE(*restored == *cmp);
  }
}

SCENARIO("CompareSync operators respect custom maxSizePerPort", "[compare_sync]") {
  GIVEN("A CompareSyncGT operator with max_size_per_port set to 3") {
    size_t limit = 3;
    auto cmp = make_compare_sync_gt("cmp1", limit);

    REQUIRE(cmp->max_size_per_port() == limit);

    WHEN("More messages than the limit are queued before execute") {
      for (int i = 0; i < (int)limit + 2; i++) {
        cmp->receive_data(create_message<NumberData>(i, NumberData{10.0}), 0);
        cmp->receive_data(create_message<NumberData>(i, NumberData{5.0}), 1);
      }
      cmp->execute();

      THEN("Only limit results are emitted") {
        const auto& output = cmp->get_output_queue(0);
        REQUIRE(output.size() == limit);

        // All results should be true (10 > 5)
        for (size_t i = 0; i < limit; i++) {
          const auto* msg = dynamic_cast<const Message<BooleanData>*>(output[i].get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->data.value == true);
        }
      }
    }
  }
}
