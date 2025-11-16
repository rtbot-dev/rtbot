#include <catch2/catch.hpp>

#include "rtbot/std/CumulativeSum.h"

using namespace rtbot;

SCENARIO("CumulativeSum handles basic operations", "[CumulativeSum]") {
  GIVEN("A CumulativeSum operator") {
    auto sum = std::make_unique<CumulativeSum>("sum1");

    WHEN("Processing single message") {
      sum->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      sum->execute();

      THEN("Sum is correctly calculated") {
        REQUIRE(sum->get_sum() == 10.0);
        const auto& output = sum->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 10.0);
      }
    }

    WHEN("Processing multiple messages") {
      sum->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      sum->receive_data(create_message<NumberData>(5, NumberData{20.0}), 0);
      sum->receive_data(create_message<NumberData>(10, NumberData{30.0}), 0);
      sum->execute();

      THEN("Cumulative sum is correct") {
        REQUIRE(sum->get_sum() == 60.0);

        const auto& output = sum->get_output_queue(0);
        REQUIRE(output.size() == 3);

        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1->time == 1);
        REQUIRE(msg1->data.value == 10.0);

        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg2->time == 5);
        REQUIRE(msg2->data.value == 30.0);

        const auto* msg3 = dynamic_cast<const Message<NumberData>*>(output[2].get());
        REQUIRE(msg3->time == 10);
        REQUIRE(msg3->data.value == 60.0);
      }
    }
  }
}

SCENARIO("CumulativeSum handles numerical stability", "[CumulativeSum]") {
  GIVEN("A CumulativeSum operator with large and small values") {
    auto sum = std::make_unique<CumulativeSum>("sum1");

    WHEN("Adding large and small numbers") {
      sum->receive_data(create_message<NumberData>(1, NumberData{1e15}), 0);
      sum->receive_data(create_message<NumberData>(2, NumberData{1e-15}), 0);
      sum->receive_data(create_message<NumberData>(3, NumberData{-1e15}), 0);
      sum->execute();

      THEN("Maintains numerical precision") { REQUIRE(std::abs(sum->get_sum() - 1e-15) < 1e-10); }
    }
  }
}

SCENARIO("CumulativeSum handles state serialization", "[CumulativeSum]") {
  GIVEN("A CumulativeSum operator with processed messages") {
    auto sum = std::make_unique<CumulativeSum>("sum1");
    sum->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    sum->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
    sum->execute();

    WHEN("State is serialized and restored") {
      Bytes state = sum->collect();
      auto restored = std::make_unique<CumulativeSum>("sum1");
      auto it = state.cbegin();
      restored->restore(it);

      THEN("State is preserved correctly") {
        REQUIRE(restored->get_sum() == 30.0);
        REQUIRE(restored->get_sum() == sum->get_sum());
        REQUIRE(*restored == *sum);

        AND_WHEN("New messages are processed") {
          restored->receive_data(create_message<NumberData>(3, NumberData{40.0}), 0);
          restored->execute();

          THEN("Continues from restored state") { REQUIRE(restored->get_sum() == 70.0); }
        }
      }
    }
  }
}

SCENARIO("CumulativeSum handles error conditions", "[CumulativeSum]") {
  GIVEN("A CumulativeSum operator") {
    auto sum = std::make_unique<CumulativeSum>("sum1");

    WHEN("Receiving wrong message type") {
      THEN("Throws type mismatch error") {
        REQUIRE_THROWS_AS(sum->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
      }
    }
  }
}