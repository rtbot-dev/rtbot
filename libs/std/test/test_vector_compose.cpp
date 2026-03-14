#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/std/VectorCompose.h"

using namespace rtbot;

SCENARIO("VectorCompose combines inputs into single vector", "[vector_compose]") {
  SECTION("Two scalar inputs") {
    auto comp = make_vector_compose("comp1", 2);

    REQUIRE(comp->type_name() == "VectorCompose");
    REQUIRE(comp->get_num_ports() == 2);

    comp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    comp->receive_data(create_message<NumberData>(1, NumberData{10.0}), 1);
    comp->execute();

    auto& output = comp->get_output_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(output[0].get());
    REQUIRE(msg->time == 1);
    REQUIRE(msg->data.values.size() == 2);
    REQUIRE(msg->data.values[0] == 5.0);
    REQUIRE(msg->data.values[1] == 10.0);
  }

  SECTION("Scalar composition preserves port order") {
    auto comp = make_vector_compose("comp1", 2);

    comp->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    comp->receive_data(create_message<NumberData>(1, NumberData{3.0}), 1);
    comp->execute();

    auto& output = comp->get_output_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(output[0].get());
    REQUIRE(msg->data.values.size() == 2);
    REQUIRE(msg->data.values[0] == 1.0);
    REQUIRE(msg->data.values[1] == 3.0);
  }

  SECTION("Three ports") {
    auto comp = make_vector_compose("comp1", 3);

    comp->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    comp->receive_data(create_message<NumberData>(1, NumberData{2.0}), 1);
    comp->receive_data(create_message<NumberData>(1, NumberData{3.0}), 2);
    comp->execute();

    auto& output = comp->get_output_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(output[0].get());
    REQUIRE(msg->data.values.size() == 3);
    REQUIRE(msg->data.values[0] == 1.0);
    REQUIRE(msg->data.values[1] == 2.0);
    REQUIRE(msg->data.values[2] == 3.0);
  }

  SECTION("Timestamp synchronization — mismatched timestamps drop stale") {
    auto comp = make_vector_compose("comp1", 2);

    // Port 0 gets t=1, port 1 gets t=2 first — t=1 on port 0 should be dropped
    comp->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    comp->receive_data(create_message<NumberData>(2, NumberData{20.0}), 1);
    // Now send t=2 on port 0 to match
    comp->receive_data(create_message<NumberData>(2, NumberData{30.0}), 0);
    comp->execute();

    auto& output = comp->get_output_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(output[0].get());
    REQUIRE(msg->time == 2);
    REQUIRE(msg->data.values[0] == 30.0);
    REQUIRE(msg->data.values[1] == 20.0);
  }
}

SCENARIO("VectorCompose validation", "[vector_compose]") {
  SECTION("Zero ports throws") {
    REQUIRE_THROWS_AS(make_vector_compose("comp1", 0), std::runtime_error);
  }
  SECTION("One port is valid (single-element vector)") {
    REQUIRE_NOTHROW(make_vector_compose("comp1", 1));
  }
}

SCENARIO("VectorCompose serialization roundtrip", "[vector_compose][State]") {
  SECTION("Collect and restore with buffered state") {
    auto comp = make_vector_compose("comp1", 2);

    // Send to only one port — creates buffered state
    comp->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);

    Bytes state = comp->collect();
    auto restored = make_vector_compose("comp1", 2);
    auto it = state.cbegin();
    restored->restore(it);

    REQUIRE(*restored == *comp);

    // Complete the sync on restored
    restored->receive_data(create_message<NumberData>(1, NumberData{10.0}), 1);
    restored->execute();

    auto& output = restored->get_output_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(output[0].get());
    REQUIRE(msg->time == 1);
    REQUIRE(msg->data.values[0] == 5.0);
    REQUIRE(msg->data.values[1] == 10.0);
  }
}
