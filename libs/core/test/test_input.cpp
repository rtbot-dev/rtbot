#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <map>
#include <typeindex>
#include <unordered_map>

#include "rtbot/Input.h"
#include "rtbot/PortType.h"

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

     WHEN("Receiving a max_size_per_port() + 1 messages, only max_size_per_port() are forwarded") {
      for (int i = 0; i < input->max_size_per_port() + 1; i++) {
        input->receive_data(create_message<NumberData>(i, NumberData{i * 2.0}), 0);
      }
      input->execute();

      THEN("only 11000 are forwarded") {
        const auto& output = input->get_output_queue(0);
        REQUIRE(output.size() == input->max_size_per_port());
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());        
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 2.0);
      }
    }

    WHEN("Receiving messages with decreasing timestamps") {
      input->receive_data(create_message<NumberData>(2, NumberData{42.0}), 0);
      input->receive_data(create_message<NumberData>(1, NumberData{24.0}), 0);
      input->execute();

      THEN("Only the first message is forwarded") {        
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
        "mixed_input", std::vector<std::string>{PortType::NUMBER, PortType::BOOLEAN, PortType::VECTOR_NUMBER});

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
        REQUIRE(types[0] == PortType::NUMBER);
        REQUIRE(types[1] == PortType::BOOLEAN);
        REQUIRE(types[2] == PortType::VECTOR_NUMBER);
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
    REQUIRE(input->get_port_types()[0] == PortType::NUMBER);
  }

  SECTION("Boolean input factory") {
    auto input = make_boolean_input("bool_input");
    REQUIRE(input->num_data_ports() == 1);
    REQUIRE(input->get_data_port_type(0) == std::type_index(typeid(BooleanData)));
    REQUIRE(input->get_port_types()[0] == PortType::BOOLEAN);
  }

  SECTION("Vector number input factory") {
    auto input = make_vector_number_input("vec_num_input");
    REQUIRE(input->num_data_ports() == 1);
    REQUIRE(input->get_data_port_type(0) == std::type_index(typeid(VectorNumberData)));
    REQUIRE(input->get_port_types()[0] == PortType::VECTOR_NUMBER);
  }
}

SCENARIO("Input operator handles state serialization", "[input][State]") {
  GIVEN("An input operator with multiple types and processed messages") {
    auto input = std::make_unique<Input>("mixed_input", std::vector<std::string>{PortType::NUMBER, PortType::BOOLEAN});

    input->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
    input->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);
    input->execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = input->collect();

      // Create new operator with same configuration
      auto restored =
          std::make_unique<Input>("mixed_input", std::vector<std::string>{PortType::NUMBER, PortType::BOOLEAN});

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("State is correctly preserved") {
        REQUIRE(*restored == *input);        
      }

      AND_WHEN("New messages are received") {
        restored->clear_all_output_ports();
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
          "mixed_input", std::vector<std::string>{PortType::BOOLEAN, PortType::NUMBER});

      THEN("Type mismatch is detected") {
        auto it = state.cbegin();
        mismatched->restore(it);
        REQUIRE(*input != *mismatched);
      }
    }
  }
}

SCENARIO("Input operator port configuration is accessible", "[input]") {
  GIVEN("An input operator with multiple ports") {
    auto input = std::make_unique<Input>(
        "config_test", std::vector<std::string>{PortType::NUMBER, PortType::BOOLEAN, PortType::VECTOR_NUMBER});

    THEN("Port configuration can be retrieved") {
      const auto& config = input->get_port_types();
      REQUIRE(config.size() == 3);
      REQUIRE(config[0] == PortType::NUMBER);
      REQUIRE(config[1] == PortType::BOOLEAN);
      REQUIRE(config[2] == PortType::VECTOR_NUMBER);
    }

    THEN("Port configuration matches actual port types") {
      const auto& config = input->get_port_types();
      REQUIRE(config.size() == input->num_data_ports());

      std::vector<std::pair<std::string, const std::type_info*>> expected_types = {
          {PortType::NUMBER, &typeid(NumberData)},
          {PortType::BOOLEAN, &typeid(BooleanData)},
          {PortType::VECTOR_NUMBER, &typeid(VectorNumberData)},
          {PortType::VECTOR_BOOLEAN, &typeid(VectorBooleanData)}};

      for (size_t i = 0; i < config.size(); i++) {
        auto it = std::find_if(expected_types.begin(), expected_types.end(),
                               [&](const auto& pair) { return pair.first == config[i]; });
        REQUIRE(it != expected_types.end());
        REQUIRE(input->get_data_port_type(i) == *it->second);
      }
    }
  }
}

SCENARIO("Input operator handles concurrent messages correctly", "[input]") {
  GIVEN("An input operator with multiple ports") {
    auto input = std::make_unique<Input>(
        "multi_input", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER, PortType::NUMBER});

    WHEN("Receiving concurrent messages with same timestamp") {
      // Send messages to different ports with same timestamp
      input->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      input->receive_data(create_message<NumberData>(1, NumberData{2.0}), 1);
      input->receive_data(create_message<NumberData>(1, NumberData{3.0}), 2);
      input->execute();

      THEN("All messages are forwarded correctly") {
        const auto& output0 = input->get_output_queue(0);
        const auto& output1 = input->get_output_queue(1);
        const auto& output2 = input->get_output_queue(2);

        REQUIRE(output0.size() == 1);
        REQUIRE(output1.size() == 1);
        REQUIRE(output2.size() == 1);

        const auto* msg0 = dynamic_cast<const Message<NumberData>*>(output0[0].get());
        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output1[0].get());
        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output2[0].get());

        REQUIRE(msg0->time == 1);
        REQUIRE(msg0->data.value == 1.0);
        REQUIRE(msg1->time == 1);
        REQUIRE(msg1->data.value == 2.0);
        REQUIRE(msg2->time == 1);
        REQUIRE(msg2->data.value == 3.0);

        AND_WHEN("Receiving more messages with mixed timestamps") {
          input->clear_all_output_ports();
          input->receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
          input->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
          input->receive_data(create_message<NumberData>(1, NumberData{6.0}), 2);  // Old timestamp
          input->execute();

          THEN("Only messages with increasing timestamps per port are forwarded") {
            REQUIRE(output0.size() == 1);
            REQUIRE(output1.size() == 1);
            REQUIRE(output2.empty());  // Old timestamp message dropped

            const auto* new_msg0 = dynamic_cast<const Message<NumberData>*>(output0[0].get());
            const auto* new_msg1 = dynamic_cast<const Message<NumberData>*>(output1[0].get());

            REQUIRE(new_msg0->time == 2);
            REQUIRE(new_msg0->data.value == 4.0);
            REQUIRE(new_msg1->time == 2);
            REQUIRE(new_msg1->data.value == 5.0);
          }
        }
      }
    }
  }
}