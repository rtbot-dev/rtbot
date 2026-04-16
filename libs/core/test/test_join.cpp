#include <catch2/catch.hpp>

#include "rtbot/Collector.h"
#include "rtbot/Join.h"
#include "rtbot/PortType.h"

using namespace rtbot;

SCENARIO("Join operator handles basic synchronization", "[join]") {
  GIVEN("A binary join for number type") {
    auto join = std::make_shared<Join>("join1", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER});
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number", "number"});
    join->connect(col, 0, 0);
    join->connect(col, 1, 1);

    WHEN("Receiving synchronized messages") {
      join->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      join->receive_data(create_message<NumberData>(1, NumberData{24.0}), 1);
      join->execute();

      THEN("Both messages are forwarded") {
        const auto& output0 = col->get_data_queue(0);
        const auto& output1 = col->get_data_queue(1);

        REQUIRE(output0.size() == 1);
        REQUIRE(output1.size() == 1);

        const auto* msg0 = dynamic_cast<const Message<NumberData>*>(output0.front().get());
        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output1.front().get());

        REQUIRE(msg0->time == 1);
        REQUIRE(msg0->data.value == 42.0);
        REQUIRE(msg1->time == 1);
        REQUIRE(msg1->data.value == 24.0);
      }
    }

    WHEN("Receiving unsynchronized messages") {
      join->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      join->receive_data(create_message<NumberData>(2, NumberData{24.0}), 1);
      join->execute();

      THEN("No messages are forwarded") {
        REQUIRE(col->get_data_queue(0).empty());
        REQUIRE(col->get_data_queue(1).empty());
      }
    }

    WHEN("Receiving messages in reverse order") {
      join->receive_data(create_message<NumberData>(2, NumberData{24.0}), 1);
      join->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      join->receive_data(create_message<NumberData>(2, NumberData{84.0}), 0);
      join->execute();

      THEN("Messages are synchronized correctly") {
        const auto& output0 = col->get_data_queue(0);
        const auto& output1 = col->get_data_queue(1);

        REQUIRE(output0.size() == 1);
        REQUIRE(output1.size() == 1);

        const auto* msg0 = dynamic_cast<const Message<NumberData>*>(output0.front().get());
        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output1.front().get());

        REQUIRE(msg0->time == 2);
        REQUIRE(msg0->data.value == 84.0);
        REQUIRE(msg1->time == 2);
        REQUIRE(msg1->data.value == 24.0);
      }
    }

  }
}

SCENARIO("Join operator handles multiple types", "[join]") {
  GIVEN("A join with different port types") {
    auto join = std::make_shared<Join>(
        "join1", std::vector<std::string>{PortType::NUMBER, PortType::BOOLEAN, PortType::VECTOR_NUMBER});
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number", "boolean", "vector_number"});
    join->connect(col, 0, 0);
    join->connect(col, 1, 1);
    join->connect(col, 2, 2);

    WHEN("Receiving synchronized messages of different types") {
      join->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      join->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
      join->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 2.0}}), 2);
      join->execute();

      THEN("All messages are forwarded with correct types") {
        {
          const auto& output = col->get_data_queue(0);
          REQUIRE(!output.empty());
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->data.value == 42.0);
        }
        {
          const auto& output = col->get_data_queue(1);
          REQUIRE(!output.empty());
          const auto* msg = dynamic_cast<const Message<BooleanData>*>(output.front().get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->data.value == true);
        }
        {
          const auto& output = col->get_data_queue(2);
          REQUIRE(!output.empty());
          const auto* msg = dynamic_cast<const Message<VectorNumberData>*>(output.front().get());
          REQUIRE(msg != nullptr);
          REQUIRE(*msg->data.values == std::vector<double>{1.0, 2.0});
        }
      }
    }

    WHEN("Sending message to wrong port type and port indexes") {
      THEN("Type mismatch is detected") {
        REQUIRE_THROWS_AS(join->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
        REQUIRE_THROWS_AS(join->receive_control(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
      }
    }
  }
}

SCENARIO("Join operator handles state serialization", "[join][State]") {
  GIVEN("A join with processed messages") {
    auto join = std::make_shared<Join>("join1", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER});
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number", "number"});
    join->connect(col, 0, 0);
    join->connect(col, 1, 1);

    join->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
    join->receive_data(create_message<NumberData>(2, NumberData{24.0}), 1);
    join->execute();

    WHEN("State is serialized and restored") {
      auto state = join->collect();

      auto restored = std::make_shared<Join>("join1", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER});
      auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"number", "number"});
      restored->connect(rcol, 0, 0);
      restored->connect(rcol, 1, 1);

      restored->restore_data_from_json(state);

      AND_WHEN("New synchronized messages are received") {
        REQUIRE(*restored == *join);
        restored->receive_data(create_message<NumberData>(3, NumberData{84.0}), 0);
        restored->receive_data(create_message<NumberData>(3, NumberData{48.0}), 1);
        restored->execute();

        THEN("Messages are processed correctly") {
          const auto& output0 = rcol->get_data_queue(0);
          const auto& output1 = rcol->get_data_queue(1);

          REQUIRE(output0.size() == 1);
          REQUIRE(output1.size() == 1);

          const auto* msg0 = dynamic_cast<const Message<NumberData>*>(output0.front().get());
          const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output1.front().get());

          REQUIRE(msg0->time == 3);
          REQUIRE(msg0->data.value == 84.0);
          REQUIRE(msg1->time == 3);
          REQUIRE(msg1->data.value == 48.0);
        }
      }
    }
  }
}

SCENARIO("Join operator validates port configuration", "[join]") {
  SECTION("Minimum number of ports") {
    REQUIRE_THROWS_AS(std::make_unique<Join>("join1", std::vector<std::string>{PortType::NUMBER}), std::runtime_error);
  }

  SECTION("Invalid port type") {
    REQUIRE_THROWS_AS(std::make_unique<Join>("join1", std::vector<std::string>{"invalid_type", "invalid_type"}),
                      std::runtime_error);
  }
}

SCENARIO("Join operator factory functions work correctly", "[join]") {
  SECTION("Number join factory") {
    auto join = make_number_join("join1", 2);
    REQUIRE(join->num_data_ports() == 2);
    REQUIRE(join->get_data_port_type(0) == std::type_index(typeid(NumberData)));
    REQUIRE(join->get_data_port_type(1) == std::type_index(typeid(NumberData)));
  }

  SECTION("Boolean join factory") {
    auto join = make_boolean_join("join1", 3);
    REQUIRE(join->num_data_ports() == 3);
    for (size_t i = 0; i < 3; ++i) {
      REQUIRE(join->get_data_port_type(i) == std::type_index(typeid(BooleanData)));
    }
  }
}

SCENARIO("Join operator handles cleanup of old messages", "[join]") {
  GIVEN("A binary join with accumulated messages") {
    auto join = std::make_shared<Join>("join1", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER});
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number", "number"});
    join->connect(col, 0, 0);
    join->connect(col, 1, 1);

    join->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    join->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    join->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
    join->receive_data(create_message<NumberData>(2, NumberData{20.0}), 1);
    join->execute();

    WHEN("A synchronized pair is processed") {
      col->reset();
      join->receive_data(create_message<NumberData>(3, NumberData{30.0}), 1);
      join->execute();

      THEN("Old messages are cleaned up") {
        col->reset();
        join->receive_data(create_message<NumberData>(4, NumberData{4.0}), 0);
        join->execute();

        REQUIRE(col->get_data_queue(0).empty());
        REQUIRE(col->get_data_queue(1).empty());
      }
    }
  }
}
