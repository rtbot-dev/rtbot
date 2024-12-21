#include <catch2/catch.hpp>

#include "rtbot/std/Variable.h"

using namespace rtbot;

SCENARIO("Variable operator handles basic operations", "[variable]") {
  GIVEN("A Variable operator with default value") {
    auto var = make_variable("var1", 42.0);

    WHEN("Querying before any data") {
      var->receive_control(create_message<NumberData>(1, NumberData{0.0}), 0);
      var->execute();

      THEN("Default value is not returned because we don't know its range end") {
        const auto& output = var->get_output_queue(0);
        REQUIRE(output.size() == 0);
      }
    }

    WHEN("Receiving data and querying") {
      // Setup data points
      var->receive_data(create_message<NumberData>(10, NumberData{100.0}), 0);
      var->receive_data(create_message<NumberData>(20, NumberData{200.0}), 0);
      var->execute();

      // Query at various times
      var->clear_all_output_ports();
      var->receive_control(create_message<NumberData>(5, NumberData{0.0}), 0);   // Before first
      var->receive_control(create_message<NumberData>(10, NumberData{0.0}), 0);  // At first
      var->receive_control(create_message<NumberData>(15, NumberData{0.0}), 0);  // Between
      var->receive_control(create_message<NumberData>(20, NumberData{0.0}), 0);  // At second
      var->receive_control(create_message<NumberData>(25, NumberData{0.0}), 0);  // After last
      var->execute();

      THEN("Correct values are returned for each query") {
        const auto& output = var->get_output_queue(0);
        REQUIRE(output.size() == 4);

        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1->time == 5);
        REQUIRE(msg1->data.value == 42.0);  // Default value before first data point

        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg2->time == 10);
        REQUIRE(msg2->data.value == 100.0);

        const auto* msg3 = dynamic_cast<const Message<NumberData>*>(output[2].get());
        REQUIRE(msg3->time == 15);
        REQUIRE(msg3->data.value == 100.0);

        const auto* msg4 = dynamic_cast<const Message<NumberData>*>(output[3].get());
        REQUIRE(msg4->time == 20);
        REQUIRE(msg4->data.value == 200.0);
      }
    }
  }
}

SCENARIO("Variable operator handles state serialization", "[variable]") {
  GIVEN("A Variable with non-trivial state") {
    auto var = make_variable("var1", 42.0);

    // Add some data
    var->receive_data(create_message<NumberData>(10, NumberData{100.0}), 0);
    var->receive_data(create_message<NumberData>(20, NumberData{200.0}), 0);
    var->execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = var->collect();

      // Create new operator
      auto restored = make_variable("var1", 42.0);

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      // Test with same queries
      var->clear_all_output_ports();
      restored->clear_all_output_ports();

      for (auto t : {5, 10, 15, 20, 25}) {
        var->receive_control(create_message<NumberData>(t, NumberData{0.0}), 0);
        restored->receive_control(create_message<NumberData>(t, NumberData{0.0}), 0);
      }

      var->execute();
      restored->execute();

      THEN("Original and restored operators produce identical results") {
        const auto& orig_output = var->get_output_queue(0);
        const auto& rest_output = restored->get_output_queue(0);

        REQUIRE(orig_output.size() == rest_output.size());

        for (size_t i = 0; i < orig_output.size(); i++) {
          const auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output[i].get());
          const auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output[i].get());

          REQUIRE(orig_msg->time == rest_msg->time);
          REQUIRE(orig_msg->data.value == rest_msg->data.value);
        }
      }
    }
  }
}

SCENARIO("Variable operator handles message arrival order variations", "[variable]") {
  GIVEN("A Variable operator without default value") {
    auto var = make_variable("var1");

    WHEN("Control messages arrive before any data") {
      var->receive_control(create_message<NumberData>(10, NumberData{0.0}), 0);
      var->receive_control(create_message<NumberData>(20, NumberData{0.0}), 0);
      var->execute();

      THEN("No output is produced as there's no known value range") {
        const auto& output = var->get_output_queue(0);
        REQUIRE(output.empty());
      }
    }

    WHEN("Data arrives, making previous queries resolvable") {
      // First send queries
      var->receive_control(create_message<NumberData>(10, NumberData{0.0}), 0);
      var->receive_control(create_message<NumberData>(20, NumberData{0.0}), 0);
      var->execute();

      // Then send data
      var->receive_data(create_message<NumberData>(5, NumberData{100.0}), 0);
      var->receive_data(create_message<NumberData>(35, NumberData{200.0}), 0);
      var->execute();

      THEN("Previously pending queries are resolved") {
        const auto& output = var->get_output_queue(0);
        REQUIRE(output.size() == 2);

        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1->time == 10);
        REQUIRE(msg1->data.value == 100.0);

        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg2->time == 20);
        REQUIRE(msg2->data.value == 100.0);
      }
    }

    WHEN("Data arrives, making previous queries resolvable 2") {
      // First send queries
      var->receive_control(create_message<NumberData>(10, NumberData{0.0}), 0);
      var->receive_control(create_message<NumberData>(20, NumberData{0.0}), 0);
      var->execute();

      // Then send data
      var->receive_data(create_message<NumberData>(15, NumberData{100.0}), 0);
      var->receive_data(create_message<NumberData>(35, NumberData{200.0}), 0);
      var->execute();

      THEN("Previously pending queries are resolved") {
        const auto& output = var->get_output_queue(0);
        REQUIRE(output.size() == 2);

        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1->time == 10);
        REQUIRE(msg1->data.value == 0.0);

        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg2->time == 20);
        REQUIRE(msg2->data.value == 100.0);
      }
    }
  }

  GIVEN("A Variable operator with interleaved message arrival") {
    auto var = make_variable("var1", 42.0);

    WHEN("Messages arrive in mixed order") {
      // First data point
      var->receive_data(create_message<NumberData>(10, NumberData{100.0}), 0);
      var->execute();

      // Query before and at first point
      var->receive_control(create_message<NumberData>(5, NumberData{0.0}), 0);
      var->receive_control(create_message<NumberData>(10, NumberData{0.0}), 0);
      var->execute();

      // Second data point
      var->receive_data(create_message<NumberData>(20, NumberData{200.0}), 0);
      var->execute();

      // More queries
      var->receive_control(create_message<NumberData>(15, NumberData{0.0}), 0);
      var->receive_control(create_message<NumberData>(25, NumberData{0.0}), 0);
      var->execute();

      THEN("All queries are resolved correctly") {
        const auto& output = var->get_output_queue(0);
        REQUIRE(output.size() == 3);

        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1->time == 5);
        REQUIRE(msg1->data.value == 42.0);  // Default value

        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg2->time == 10);
        REQUIRE(msg2->data.value == 100.0);

        const auto* msg3 = dynamic_cast<const Message<NumberData>*>(output[2].get());
        REQUIRE(msg3->time == 15);
        REQUIRE(msg3->data.value == 100.0);
      }
    }
  }

  GIVEN("A Variable operator handling edge cases") {
    auto var = make_variable("var1", 42.0);

    WHEN("Multiple data points arrive at the same timestamp") {
      var->receive_data(create_message<NumberData>(10, NumberData{100.0}), 0);
      var->execute();

      var->receive_control(create_message<NumberData>(10, NumberData{0.0}), 0);
      var->execute();

      THEN("The last value is used") {
        const auto& output = var->get_output_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 10);
        REQUIRE(msg->data.value == 100.0);
      }
    }
  }

  GIVEN("A Variable operator that gets reset") {
    auto var = make_variable("var1", 42.0);

    // Setup initial state
    var->receive_data(create_message<NumberData>(10, NumberData{100.0}), 0);
    var->receive_control(create_message<NumberData>(15, NumberData{0.0}), 0);
    var->execute();

    WHEN("The operator is reset") {
      var->reset();

      // Send new data
      var->receive_data(create_message<NumberData>(20, NumberData{200.0}), 0);
      var->execute();

      var->receive_control(create_message<NumberData>(15, NumberData{0.0}), 0);
      var->execute();

      THEN("Previous state is cleared and new queries use default value") {
        const auto& output = var->get_output_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 15);
        REQUIRE(msg->data.value == 42.0);  // Default value used
      }
    }
  }
}