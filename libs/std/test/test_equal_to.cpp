#include <catch2/catch.hpp>

#include "rtbot/std/EqualTo.h"

using namespace rtbot;

SCENARIO("EqualTo operator handles exact matches", "[equal_to]") {
  GIVEN("An EqualTo operator with value 42.0") {
    auto equal_to = std::make_unique<EqualTo>("eq1", 42.0);

    WHEN("Receiving matching value") {
      equal_to->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      equal_to->execute();

      THEN("Message is forwarded") {
        const auto& output = equal_to->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 42.0);
      }
    }

    WHEN("Receiving non-matching value") {
      equal_to->receive_data(create_message<NumberData>(1, NumberData{43.0}), 0);
      equal_to->execute();

      THEN("No message is forwarded") { REQUIRE(equal_to->get_output_queue(0).empty()); }
    }
  }
}

SCENARIO("EqualTo operator handles tolerance", "[equal_to]") {
  GIVEN("An EqualTo operator with value 10.0 and tolerance 0.1") {
    auto equal_to = std::make_unique<EqualTo>("eq1", 10.0, 0.1);

    WHEN("Receiving values within tolerance") {
      equal_to->receive_data(create_message<NumberData>(1, NumberData{9.95}), 0);
      equal_to->receive_data(create_message<NumberData>(2, NumberData{10.05}), 0);
      equal_to->execute();

      THEN("Messages are forwarded") {
        const auto& output = equal_to->get_output_queue(0);
        REQUIRE(output.size() == 2);
      }
    }

    WHEN("Receiving values outside tolerance") {
      equal_to->receive_data(create_message<NumberData>(1, NumberData{9.8}), 0);
      equal_to->receive_data(create_message<NumberData>(2, NumberData{10.2}), 0);
      equal_to->execute();

      THEN("No messages are forwarded") { REQUIRE(equal_to->get_output_queue(0).empty()); }
    }
  }
}

SCENARIO("EqualTo operator handles type checking", "[equal_to]") {
  GIVEN("An EqualTo operator") {
    auto equal_to = std::make_unique<EqualTo>("eq1", 1.0);

    WHEN("Receiving wrong message type") {
      THEN("Type error is thrown") {
        REQUIRE_THROWS_AS(equal_to->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0),
                          std::runtime_error);
      }
    }
  }
}

SCENARIO("EqualTo operator handles negative tolerance", "[equal_to]") {
  GIVEN("An EqualTo operator with negative tolerance") {
    auto equal_to = std::make_unique<EqualTo>("eq1", 5.0, -0.1);

    WHEN("Receiving values within absolute tolerance") {
      equal_to->receive_data(create_message<NumberData>(1, NumberData{4.95}), 0);
      equal_to->receive_data(create_message<NumberData>(2, NumberData{5.05}), 0);
      equal_to->execute();

      THEN("Messages are forwarded") {
        const auto& output = equal_to->get_output_queue(0);
        REQUIRE(output.size() == 2);
      }
    }
  }
}