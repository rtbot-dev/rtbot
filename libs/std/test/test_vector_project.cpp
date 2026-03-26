#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/std/VectorProject.h"

using namespace rtbot;

SCENARIO("VectorProject selects subset of fields", "[vector_project]") {
  SECTION("Select two fields") {
    auto proj = make_vector_project("proj1", {0, 2});

    REQUIRE(proj->type_name() == "VectorProject");

    proj->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{10.0, 20.0, 30.0, 40.0}}), 0);
    proj->execute();

    auto& output = proj->get_output_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(output[0].get());
    REQUIRE(msg->time == 1);
    REQUIRE(msg->data.values.size() == 2);
    REQUIRE(msg->data.values[0] == 10.0);
    REQUIRE(msg->data.values[1] == 30.0);
  }

  SECTION("Reorder fields") {
    auto proj = make_vector_project("proj1", {2, 0, 1});

    proj->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{10.0, 20.0, 30.0}}), 0);
    proj->execute();

    auto& output = proj->get_output_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(output[0].get());
    REQUIRE(msg->data.values.size() == 3);
    REQUIRE(msg->data.values[0] == 30.0);
    REQUIRE(msg->data.values[1] == 10.0);
    REQUIRE(msg->data.values[2] == 20.0);
  }

  SECTION("Single index") {
    auto proj = make_vector_project("proj1", {1});

    proj->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{10.0, 20.0, 30.0}}), 0);
    proj->execute();

    auto& output = proj->get_output_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(output[0].get());
    REQUIRE(msg->data.values.size() == 1);
    REQUIRE(msg->data.values[0] == 20.0);
  }
}

SCENARIO("VectorProject validation", "[vector_project]") {
  SECTION("Empty indices throws") {
    REQUIRE_THROWS_AS(make_vector_project("proj1", {}), std::runtime_error);
  }

  SECTION("Negative index throws") {
    REQUIRE_THROWS_AS(make_vector_project("proj1", {0, -1}), std::runtime_error);
  }

  SECTION("Out of bounds at runtime throws") {
    auto proj = make_vector_project("proj1", {0, 5});
    proj->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{10.0, 20.0}}), 0);
    REQUIRE_THROWS_AS(proj->execute(), std::runtime_error);
  }
}

SCENARIO("VectorProject serialization roundtrip", "[vector_project][State]") {
  SECTION("Collect and restore") {
    auto proj = make_vector_project("proj1", {0, 2});
    proj->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{10.0, 20.0, 30.0}}), 0);
    proj->execute();

    auto state = proj->collect();
    auto restored = make_vector_project("proj1", {0, 2});
    restored->restore_data_from_json(state);

    REQUIRE(*restored == *proj);
  }
}
