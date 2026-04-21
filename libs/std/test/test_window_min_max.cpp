#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/Collector.h"
#include "rtbot/std/WindowMinMax.h"

using namespace rtbot;

SCENARIO("WindowMinMax MIN mode — roadmap test case", "[window_min_max]") {
  SECTION("window_size=3, input=[5,3,7,2,8,1,4] → [3,2,2,1,1]") {
    auto wmm = make_window_min_max("w1", 3, "min");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    wmm->connect(col, 0, 0);
    REQUIRE(wmm->type_name() == "WindowMinMax");

    std::vector<double> inputs = {5, 3, 7, 2, 8, 1, 4};
    for (size_t i = 0; i < inputs.size(); ++i) {
      wmm->receive_data(create_message<NumberData>(
          static_cast<timestamp_t>(i + 1), NumberData{inputs[i]}), 0);
    }
    wmm->execute();

    auto& out = col->get_data_queue(0);
    // No output for first 2 messages (warmup), then 5 outputs
    REQUIRE(out.size() == 5);

    std::vector<double> expected = {3, 2, 2, 1, 1};
    for (size_t i = 0; i < expected.size(); ++i) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(out[i].get());
      REQUIRE(msg->time == static_cast<timestamp_t>(i + 3));
      REQUIRE(msg->data.value == Approx(expected[i]));
    }
  }
}

SCENARIO("WindowMinMax MAX mode — roadmap test case", "[window_min_max]") {
  SECTION("window_size=3, input=[5,3,7,2,8,1,4] → [7,7,8,8,8]") {
    auto wmm = make_window_min_max("w1", 3, "max");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    wmm->connect(col, 0, 0);

    std::vector<double> inputs = {5, 3, 7, 2, 8, 1, 4};
    for (size_t i = 0; i < inputs.size(); ++i) {
      wmm->receive_data(create_message<NumberData>(
          static_cast<timestamp_t>(i + 1), NumberData{inputs[i]}), 0);
    }
    wmm->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 5);

    std::vector<double> expected = {7, 7, 8, 8, 8};
    for (size_t i = 0; i < expected.size(); ++i) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(out[i].get());
      REQUIRE(msg->time == static_cast<timestamp_t>(i + 3));
      REQUIRE(msg->data.value == Approx(expected[i]));
    }
  }
}

SCENARIO("WindowMinMax warmup — no output until window full", "[window_min_max]") {
  SECTION("window_size=5, only 4 messages → no output") {
    auto wmm = make_window_min_max("w1", 5, "min");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    wmm->connect(col, 0, 0);

    for (int i = 1; i <= 4; ++i) {
      wmm->receive_data(
          create_message<NumberData>(static_cast<timestamp_t>(i), NumberData{static_cast<double>(i)}), 0);
    }
    wmm->execute();

    REQUIRE(col->get_data_queue(0).empty());
  }

  SECTION("window_size=1 — emit on every message") {
    auto wmm = make_window_min_max("w1", 1, "min");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    wmm->connect(col, 0, 0);

    wmm->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    wmm->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
    wmm->receive_data(create_message<NumberData>(3, NumberData{7.0}), 0);
    wmm->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 3);
    // window=1: each value is its own min
    std::vector<double> expected = {5, 3, 7};
    for (size_t i = 0; i < 3; ++i) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(out[i].get());
      REQUIRE(msg->data.value == Approx(expected[i]));
    }
  }
}

SCENARIO("WindowMinMax serialization roundtrip", "[window_min_max][State]") {
  SECTION("Partial window → collect → restore → continue") {
    auto wmm = make_window_min_max("w1", 3, "min");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    wmm->connect(col, 0, 0);

    // Feed 2 values (window not yet full)
    wmm->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    wmm->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
    wmm->execute();
    REQUIRE(col->get_data_queue(0).empty());

    auto state = wmm->collect();
    auto restored = make_window_min_max("w1", 3, "min");
    auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
    restored->connect(rcol, 0, 0);
    restored->restore_data_from_json(state);

    // Feed 3rd value → window full → min of [5,3,7]=3
    restored->receive_data(create_message<NumberData>(3, NumberData{7.0}), 0);
    restored->execute();

    auto& out = rcol->get_data_queue(0);
    REQUIRE(out.size() == 1);
    const auto* msg = dynamic_cast<const Message<NumberData>*>(out[0].get());
    REQUIRE(msg->data.value == Approx(3.0));
  }

  SECTION("Full window → collect → restore → correct sliding") {
    auto wmm = make_window_min_max("w1", 3, "max");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    wmm->connect(col, 0, 0);

    // Feed 3 values (window full, first output at pos=2)
    wmm->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    wmm->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
    wmm->receive_data(create_message<NumberData>(3, NumberData{7.0}), 0);
    wmm->execute();

    auto& out1 = col->get_data_queue(0);
    REQUIRE(out1.size() == 1);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out1[0].get())->data.value == Approx(7.0));

    auto state = wmm->collect();
    auto restored = make_window_min_max("w1", 3, "max");
    auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
    restored->connect(rcol, 0, 0);
    restored->restore_data_from_json(state);

    // Feed [2]: window=[3,7,2] → max=7
    restored->receive_data(create_message<NumberData>(4, NumberData{2.0}), 0);
    restored->execute();

    auto& out2 = rcol->get_data_queue(0);
    REQUIRE(out2.size() == 1);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out2[0].get())->data.value == Approx(7.0));
  }
}
