#include <catch2/catch.hpp>

#include "rtbot/Collector.h"
#include "rtbot/std/BooleanToNumber.h"

using namespace rtbot;

SCENARIO("BooleanToNumber operator converts boolean values to numbers", "[boolean_to_number]") {
  GIVEN("A BooleanToNumber operator") {
    auto b2n = std::make_shared<BooleanToNumber>("b2n1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    b2n->connect(col, 0, 0);

    WHEN("Receiving a true message") {
      b2n->receive_data(create_message<BooleanData>(10, BooleanData{true}), 0);
      b2n->execute();

      THEN("Output is NumberData{1.0} with preserved timestamp") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 10);
        REQUIRE(msg->data.value == 1.0);
      }
    }

    WHEN("Receiving a false message") {
      b2n->receive_data(create_message<BooleanData>(20, BooleanData{false}), 0);
      b2n->execute();

      THEN("Output is NumberData{0.0} with preserved timestamp") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 20);
        REQUIRE(msg->data.value == 0.0);
      }
    }

    WHEN("Receiving multiple messages in sequence") {
      b2n->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
      b2n->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
      b2n->receive_data(create_message<BooleanData>(3, BooleanData{true}), 0);
      b2n->receive_data(create_message<BooleanData>(4, BooleanData{false}), 0);
      b2n->receive_data(create_message<BooleanData>(5, BooleanData{true}), 0);
      b2n->execute();

      THEN("All 5 messages are converted correctly in order") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 5);

        auto it = output.begin();
        std::vector<std::pair<timestamp_t, double>> expected = {
            {1, 1.0}, {2, 0.0}, {3, 1.0}, {4, 0.0}, {5, 1.0}};

        for (const auto& [exp_time, exp_value] : expected) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>((*it).get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == exp_time);
          REQUIRE(msg->data.value == exp_value);
          ++it;
        }
      }
    }
  }
}

SCENARIO("BooleanToNumber operator handles state serialization", "[boolean_to_number][State]") {
  GIVEN("A BooleanToNumber operator with processed messages") {
    auto b2n = std::make_shared<BooleanToNumber>("b2n1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    b2n->connect(col, 0, 0);

    // Add some messages
    b2n->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
    b2n->receive_data(create_message<BooleanData>(3, BooleanData{false}), 0);
    b2n->execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      auto state = b2n->collect();

      // Create new operator
      auto restored = std::make_shared<BooleanToNumber>("b2n1");
      auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);

      // Restore state
      restored->restore_data_from_json(state);

      THEN("Behavior is preserved") {
        REQUIRE(*restored == *b2n);
        restored->receive_data(create_message<BooleanData>(5, BooleanData{true}), 0);
        restored->execute();

        const auto& output = rcol->get_data_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 5);
        REQUIRE(msg->data.value == 1.0);
      }
    }
  }
}

SCENARIO("BooleanToNumber operator validates message types", "[boolean_to_number]") {
  GIVEN("A BooleanToNumber operator") {
    auto b2n = std::make_shared<BooleanToNumber>("b2n1");

    THEN("It rejects invalid message types") {
      REQUIRE_THROWS_AS(b2n->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0),
                        std::runtime_error);
    }
  }
}
