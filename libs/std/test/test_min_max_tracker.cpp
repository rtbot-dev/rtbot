#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/std/MinMaxTracker.h"

using namespace rtbot;

SCENARIO("MinTracker tracks all-time running minimum", "[min_max_tracker]") {
  SECTION("Decreasing then increasing sequence") {
    auto mn = make_min_tracker("mn1");
    REQUIRE(mn->type_name() == "MinTracker");

    // Input [5, 3, 7, 2, 8] → min should be [5, 3, 3, 2, 2]
    std::vector<double> inputs = {5, 3, 7, 2, 8};
    for (size_t i = 0; i < inputs.size(); i++) {
      mn->receive_data(
          create_message<NumberData>(static_cast<timestamp_t>(i + 1),
                                     NumberData{inputs[i]}),
          0);
    }
    mn->execute();

    auto& out = mn->get_output_queue(0);
    REQUIRE(out.size() == 5);

    std::vector<double> expected = {5, 3, 3, 2, 2};
    for (size_t i = 0; i < expected.size(); i++) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(out[i].get());
      REQUIRE(msg->time == static_cast<timestamp_t>(i + 1));
      REQUIRE(msg->data.value == Approx(expected[i]));
    }
  }

  SECTION("First message establishes initial minimum") {
    auto mn = make_min_tracker("mn1");
    mn->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
    mn->execute();

    auto& out = mn->get_output_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(42.0));
  }

  SECTION("Monotonically increasing — min stays at first value") {
    auto mn = make_min_tracker("mn1");
    mn->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    mn->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    mn->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
    mn->execute();

    auto& out = mn->get_output_queue(0);
    REQUIRE(out.size() == 3);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(1.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[1].get())->data.value == Approx(1.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[2].get())->data.value == Approx(1.0));
  }
}

SCENARIO("MaxTracker tracks all-time running maximum", "[min_max_tracker]") {
  SECTION("Increasing then decreasing sequence") {
    auto mx = make_max_tracker("mx1");
    REQUIRE(mx->type_name() == "MaxTracker");

    // Input [5, 3, 7, 2, 8] → max should be [5, 5, 7, 7, 8]
    std::vector<double> inputs = {5, 3, 7, 2, 8};
    for (size_t i = 0; i < inputs.size(); i++) {
      mx->receive_data(
          create_message<NumberData>(static_cast<timestamp_t>(i + 1),
                                     NumberData{inputs[i]}),
          0);
    }
    mx->execute();

    auto& out = mx->get_output_queue(0);
    REQUIRE(out.size() == 5);

    std::vector<double> expected = {5, 5, 7, 7, 8};
    for (size_t i = 0; i < expected.size(); i++) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(out[i].get());
      REQUIRE(msg->time == static_cast<timestamp_t>(i + 1));
      REQUIRE(msg->data.value == Approx(expected[i]));
    }
  }

  SECTION("Monotonically decreasing — max stays at first value") {
    auto mx = make_max_tracker("mx1");
    mx->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    mx->receive_data(create_message<NumberData>(2, NumberData{5.0}), 0);
    mx->receive_data(create_message<NumberData>(3, NumberData{1.0}), 0);
    mx->execute();

    auto& out = mx->get_output_queue(0);
    REQUIRE(out.size() == 3);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(10.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[1].get())->data.value == Approx(10.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[2].get())->data.value == Approx(10.0));
  }
}

SCENARIO("MinTracker serialization roundtrip", "[min_max_tracker][State]") {
  SECTION("Collect and restore mid-stream") {
    auto mn = make_min_tracker("mn1");
    mn->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    mn->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
    mn->execute();

    REQUIRE(mn->get_current_min() == Approx(3.0));

    auto state = mn->collect();
    auto restored = make_min_tracker("mn1");
    restored->restore_data_from_json(state);
    REQUIRE(*restored == *mn);
    REQUIRE(restored->get_current_min() == Approx(3.0));

    restored->clear_all_output_ports();
    restored->receive_data(create_message<NumberData>(3, NumberData{7.0}), 0);
    restored->execute();

    auto& out = restored->get_output_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(3.0));
  }
}

SCENARIO("MaxTracker serialization roundtrip", "[min_max_tracker][State]") {
  SECTION("Collect and restore mid-stream") {
    auto mx = make_max_tracker("mx1");
    mx->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    mx->receive_data(create_message<NumberData>(2, NumberData{9.0}), 0);
    mx->execute();

    REQUIRE(mx->get_current_max() == Approx(9.0));

    auto state = mx->collect();
    auto restored = make_max_tracker("mx1");
    restored->restore_data_from_json(state);
    REQUIRE(*restored == *mx);
    REQUIRE(restored->get_current_max() == Approx(9.0));

    restored->clear_all_output_ports();
    restored->receive_data(create_message<NumberData>(3, NumberData{2.0}), 0);
    restored->execute();

    auto& out = restored->get_output_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(9.0));
  }
}
