#include <catch2/catch.hpp>

#include "rtbot/Collector.h"
#include "rtbot/std/Count.h"

using namespace rtbot;

SCENARIO("Count operator handles basic counting", "[Count]") {
  GIVEN("A Count operator") {
    auto counter = std::make_shared<Count<NumberData>>("counter");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    counter->connect(col, 0, 0);

    WHEN("Receiving messages of different types") {
      counter->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      counter->receive_data(create_message<NumberData>(2, NumberData{37}), 0);
      counter->receive_data(create_message<NumberData>(3, NumberData{-2.2}), 0);
      counter->execute();

      THEN("Counts messages regardless of type") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 3);

        for (size_t i = 0; i < output.size(); i++) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
          REQUIRE(msg->time == i + 1);
          REQUIRE(msg->data.value == i + 1);
        }
      }
    }
  }
}

SCENARIO("Count operator handles state serialization", "[Count][State]") {
  GIVEN("A Count operator with some history") {
    auto counter = std::make_shared<Count<BooleanData>>("counter");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    counter->connect(col, 0, 0);

    // Process initial messages
    counter->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
    counter->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
    counter->execute();

    WHEN("State is serialized and restored") {
      auto state = counter->collect();
      auto restored = std::make_shared<Count<BooleanData>>("counter");
      auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);
      restored->restore_data_from_json(state);

      THEN("Continues counting from previous state") {
        REQUIRE(*counter == *restored);
        restored->receive_data(create_message<BooleanData>(3, BooleanData{true}), 0);
        restored->execute();

        const auto& output = rcol->get_data_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == 3.0);
      }
    }
  }
}
