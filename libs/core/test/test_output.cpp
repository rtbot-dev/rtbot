#include <catch2/catch.hpp>

#include "rtbot/Output.h"

using namespace rtbot;

SCENARIO("Output operator handles single port configurations", "[output]") {
  SECTION("Number output") {
    auto output = make_number_output("out1");

    REQUIRE(output->num_data_ports() == 1);
    REQUIRE(output->num_output_ports() == 1);
    REQUIRE(output->get_port_types().size() == 1);
    REQUIRE(output->get_port_types()[0] == PortType::NUMBER);

    WHEN("Receiving a number message") {
      output->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      output->execute();

      THEN("Message is forwarded") {
        const auto& out_queue = output->get_output_queue(0);
        REQUIRE(out_queue.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(out_queue.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 42.0);
      }
    }
  }

  SECTION("Boolean output") {
    auto output = make_boolean_output("out1");

    WHEN("Receiving a boolean message") {
      output->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
      output->execute();

      THEN("Message is forwarded") {
        const auto& out_queue = output->get_output_queue(0);
        REQUIRE(out_queue.size() == 1);
        const auto* msg = dynamic_cast<const Message<BooleanData>*>(out_queue.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == true);
      }
    }
  }

  SECTION("Vector output") {
    auto output = make_vector_number_output("out1");

    WHEN("Receiving a vector message") {
      output->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 2.0, 3.0}}), 0);
      output->execute();

      THEN("Message is forwarded") {
        const auto& out_queue = output->get_output_queue(0);
        REQUIRE(out_queue.size() == 1);
        const auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out_queue.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.values == std::vector<double>{1.0, 2.0, 3.0});
      }
    }
  }
}

SCENARIO("Output operator handles multiple port configurations", "[output]") {
  GIVEN("A multi-port output operator") {
    auto output = std::make_unique<Output>(
        "out1", std::vector<std::string>{PortType::NUMBER, PortType::BOOLEAN, PortType::VECTOR_NUMBER});

    REQUIRE(output->num_data_ports() == 3);
    REQUIRE(output->num_output_ports() == 3);

    WHEN("Receiving messages on different ports") {
      output->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      output->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
      output->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 2.0}}), 2);
      output->execute();

      THEN("Messages are forwarded to correct ports") {
        // Check number port
        {
          const auto& out_queue = output->get_output_queue(0);
          REQUIRE(out_queue.size() == 1);
          const auto* msg = dynamic_cast<const Message<NumberData>*>(out_queue.front().get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->data.value == 42.0);
        }

        // Check boolean port
        {
          const auto& out_queue = output->get_output_queue(1);
          REQUIRE(out_queue.size() == 1);
          const auto* msg = dynamic_cast<const Message<BooleanData>*>(out_queue.front().get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->data.value == true);
        }

        // Check vector port
        {
          const auto& out_queue = output->get_output_queue(2);
          REQUIRE(out_queue.size() == 1);
          const auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out_queue.front().get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->data.values == std::vector<double>{1.0, 2.0});
        }
      }
    }
  }
}

SCENARIO("Output operator validates configuration", "[output]") {
  SECTION("Empty port types") {
    REQUIRE_THROWS_AS(std::make_unique<Output>("out1", std::vector<std::string>{}), std::runtime_error);
  }

  SECTION("Invalid port type") {
    REQUIRE_THROWS_AS(std::make_unique<Output>("out1", std::vector<std::string>{"invalid_type"}), std::runtime_error);
  }
}

SCENARIO("Output operator preserves message order", "[output]") {
  GIVEN("A number output operator") {
    auto output = make_number_output("out1");

    WHEN("Receiving multiple messages") {
      output->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      output->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      output->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      output->execute();

      THEN("Messages are forwarded in order") {
        const auto& out_queue = output->get_output_queue(0);
        REQUIRE(out_queue.size() == 3);

        auto it = out_queue.begin();
        for (int i = 0; i < 3; ++i) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(it->get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == i + 1);
          REQUIRE(msg->data.value == static_cast<double>(i + 1));
          ++it;
        }
      }
    }
  }
}

SCENARIO("Output operator handles type mismatches", "[output]") {
  GIVEN("A number output operator") {
    auto output = make_number_output("out1");

    WHEN("Receiving wrong message type") {
      THEN("Error is thrown") {
        REQUIRE_THROWS_AS(output->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0),
                          std::runtime_error);
      }
    }
  }
}