#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <map>
#include <typeindex>
#include <unordered_map>

#include "rtbot/Input.h"

using namespace rtbot;

SCENARIO("Input operator handles single number port", "[input]") {
  GIVEN("An input operator with one number port") {
    auto input = make_number_input("input1");

    WHEN("Receiving a number message") {
      input->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      input->execute();

      THEN("Message is forwarded") {
        const auto& output = input->get_output_queue(0);
        REQUIRE(!output.empty());
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 42.0);
      }
    }

    WHEN("Receiving messages with decreasing timestamps") {
      input->receive_data(create_message<NumberData>(2, NumberData{42.0}), 0);
      input->receive_data(create_message<NumberData>(1, NumberData{24.0}), 0);
      input->execute();

      THEN("Only the first message is forwarded") {
        REQUIRE(input->get_last_sent_time(0) == 2);
        const auto& output = input->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg->data.value == 42.0);
      }
    }
  }
}

SCENARIO("Input operator handles multiple types", "[input]") {
  GIVEN("An input operator with different types of ports") {
    auto input = std::make_unique<Input>(
        "mixed_input", std::vector<std::string>{Input::NUMBER_PORT, Input::BOOLEAN_PORT, Input::VECTOR_NUMBER_PORT});

    WHEN("Receiving different types of messages") {
      input->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      input->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
      input->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 2.0, 3.0}}), 2);
      input->execute();

      THEN("Messages are forwarded with correct types") {
        // Check number port
        {
          const auto& output = input->get_output_queue(0);
          REQUIRE(!output.empty());
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->data.value == 42.0);
        }

        // Check boolean port
        {
          const auto& output = input->get_output_queue(1);
          REQUIRE(!output.empty());
          const auto* msg = dynamic_cast<const Message<BooleanData>*>(output.front().get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->data.value == true);
        }

        // Check vector port
        {
          const auto& output = input->get_output_queue(2);
          REQUIRE(!output.empty());
          const auto* msg = dynamic_cast<const Message<VectorNumberData>*>(output.front().get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->data.values == std::vector<double>{1.0, 2.0, 3.0});
        }
      }

      THEN("Port types are correctly identified") {
        REQUIRE(input->get_data_port_type(0) == std::type_index(typeid(NumberData)));
        REQUIRE(input->get_data_port_type(1) == std::type_index(typeid(BooleanData)));
        REQUIRE(input->get_data_port_type(2) == std::type_index(typeid(VectorNumberData)));
      }

      THEN("Port type names are preserved") {
        const auto& types = input->get_port_types();
        REQUIRE(types.size() == 3);
        REQUIRE(types[0] == Input::NUMBER_PORT);
        REQUIRE(types[1] == Input::BOOLEAN_PORT);
        REQUIRE(types[2] == Input::VECTOR_NUMBER_PORT);
      }
    }

    WHEN("Sending message to wrong port type") {
      THEN("Type mismatch is detected") {
        REQUIRE_THROWS_AS(input->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0),
                          std::runtime_error);
      }
    }
  }
}

SCENARIO("Input operator validates port type strings", "[input]") {
  SECTION("Invalid port type string") {
    REQUIRE_THROWS_AS(std::make_unique<Input>("invalid", std::vector<std::string>{"invalid_type"}), std::runtime_error);
  }
}

SCENARIO("Input operator factory functions work correctly", "[input]") {
  SECTION("Number input factory") {
    auto input = make_number_input("num_input");
    REQUIRE(input->num_data_ports() == 1);
    REQUIRE(input->get_data_port_type(0) == std::type_index(typeid(NumberData)));
    REQUIRE(input->get_port_types()[0] == Input::NUMBER_PORT);
  }

  SECTION("Boolean input factory") {
    auto input = make_boolean_input("bool_input");
    REQUIRE(input->num_data_ports() == 1);
    REQUIRE(input->get_data_port_type(0) == std::type_index(typeid(BooleanData)));
    REQUIRE(input->get_port_types()[0] == Input::BOOLEAN_PORT);
  }

  SECTION("Vector number input factory") {
    auto input = make_vector_number_input("vec_num_input");
    REQUIRE(input->num_data_ports() == 1);
    REQUIRE(input->get_data_port_type(0) == std::type_index(typeid(VectorNumberData)));
    REQUIRE(input->get_port_types()[0] == Input::VECTOR_NUMBER_PORT);
  }
}

SCENARIO("Input operator handles state serialization", "[input]") {
  GIVEN("An input operator with multiple types and processed messages") {
    auto input =
        std::make_unique<Input>("mixed_input", std::vector<std::string>{Input::NUMBER_PORT, Input::BOOLEAN_PORT});

    input->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
    input->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);
    input->execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = input->collect();

      // Create new operator with same configuration
      auto restored =
          std::make_unique<Input>("mixed_input", std::vector<std::string>{Input::NUMBER_PORT, Input::BOOLEAN_PORT});

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("State is correctly preserved") {
        REQUIRE(restored->get_last_sent_time(0) == 1);
        REQUIRE(restored->get_last_sent_time(1) == 2);
        REQUIRE(restored->has_sent(0));
        REQUIRE(restored->has_sent(1));
      }

      AND_WHEN("New messages are received") {
        restored->receive_data(create_message<NumberData>(3, NumberData{84.0}), 0);
        restored->execute();

        THEN("Messages are processed based on restored state") {
          const auto& output = restored->get_output_queue(0);
          REQUIRE(output.size() == 1);
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
          REQUIRE(msg->time == 3);
          REQUIRE(msg->data.value == 84.0);
        }
      }
    }

    WHEN("Restoring with mismatched configuration") {
      Bytes state = input->collect();

      // Create new operator with different configuration
      auto mismatched = std::make_unique<Input>(
          "mixed_input", std::vector<std::string>{Input::BOOLEAN_PORT, Input::NUMBER_PORT});  // Wrong order

      THEN("Type mismatch is detected") {
        auto it = state.cbegin();
        REQUIRE_THROWS_AS(mismatched->restore(it), std::runtime_error);
      }
    }
  }
}

SCENARIO("Input operator port configuration is accessible", "[input]") {
  GIVEN("An input operator with multiple ports") {
    auto input = std::make_unique<Input>(
        "config_test", std::vector<std::string>{Input::NUMBER_PORT, Input::BOOLEAN_PORT, Input::VECTOR_NUMBER_PORT});

    THEN("Port configuration can be retrieved") {
      const auto& config = input->get_port_types();
      REQUIRE(config.size() == 3);
      REQUIRE(config[0] == Input::NUMBER_PORT);
      REQUIRE(config[1] == Input::BOOLEAN_PORT);
      REQUIRE(config[2] == Input::VECTOR_NUMBER_PORT);
    }

    THEN("Port configuration matches actual port types") {
      const auto& config = input->get_port_types();
      REQUIRE(config.size() == input->num_data_ports());

      std::vector<std::pair<std::string, const std::type_info*>> expected_types = {
          {Input::NUMBER_PORT, &typeid(NumberData)},
          {Input::BOOLEAN_PORT, &typeid(BooleanData)},
          {Input::VECTOR_NUMBER_PORT, &typeid(VectorNumberData)},
          {Input::VECTOR_BOOLEAN_PORT, &typeid(VectorBooleanData)}};

      for (size_t i = 0; i < config.size(); i++) {
        auto it = std::find_if(expected_types.begin(), expected_types.end(),
                               [&](const auto& pair) { return pair.first == config[i]; });
        REQUIRE(it != expected_types.end());
        REQUIRE(input->get_data_port_type(i) == *it->second);
      }
    }
  }
}