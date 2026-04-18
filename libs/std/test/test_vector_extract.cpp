#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/Collector.h"
#include "rtbot/std/VectorExtract.h"

using namespace rtbot;

SCENARIO("VectorExtract extracts single field from vector", "[vector_extract]") {
  SECTION("Extract middle element") {
    auto ext = make_vector_extract("ext1", 1);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    ext->connect(col, 0, 0);

    REQUIRE(ext->type_name() == "VectorExtract");
    REQUIRE(ext->get_index() == 1);

    ext->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{10.0, 20.0, 30.0}}), 0);
    ext->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(msg->time == 1);
    REQUIRE(msg->data.value == 20.0);
  }

  SECTION("Extract first element") {
    auto ext = make_vector_extract("ext1", 0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    ext->connect(col, 0, 0);

    ext->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{5.0}}), 0);
    ext->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(msg->time == 1);
    REQUIRE(msg->data.value == 5.0);
  }

  SECTION("Multiple messages") {
    auto ext = make_vector_extract("ext1", 2);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    ext->connect(col, 0, 0);

    ext->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{10.0, 20.0, 30.0}}), 0);
    ext->receive_data(create_message<VectorNumberData>(2, VectorNumberData{{40.0, 50.0, 60.0}}), 0);
    ext->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == 2);

    auto* msg0 = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(msg0->time == 1);
    REQUIRE(msg0->data.value == 30.0);

    auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[1].get());
    REQUIRE(msg1->time == 2);
    REQUIRE(msg1->data.value == 60.0);
  }
}

SCENARIO("VectorExtract validation", "[vector_extract]") {
  SECTION("Negative index throws") {
    REQUIRE_THROWS_AS(make_vector_extract("ext1", -1), std::runtime_error);
  }

  SECTION("Out of bounds at runtime throws") {
    auto ext = make_vector_extract("ext1", 5);
    ext->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{10.0, 20.0}}), 0);
    REQUIRE_THROWS_AS(ext->execute(), std::runtime_error);
  }
}

SCENARIO("VectorExtract serialization roundtrip", "[vector_extract][State]") {
  SECTION("Collect and restore") {
    auto ext = make_vector_extract("ext1", 1);
    ext->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{10.0, 20.0, 30.0}}), 0);
    ext->execute();

    auto state = ext->collect();
    auto restored = make_vector_extract("ext1", 1);
    auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
    restored->connect(rcol, 0, 0);
    restored->restore_data_from_json(state);

    REQUIRE(*restored == *ext);

    restored->receive_data(create_message<VectorNumberData>(2, VectorNumberData{{40.0, 50.0, 60.0}}), 0);
    restored->execute();

    auto& output = rcol->get_data_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(msg->time == 2);
    REQUIRE(msg->data.value == 50.0);
  }
}
