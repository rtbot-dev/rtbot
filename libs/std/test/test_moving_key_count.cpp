#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/Collector.h"
#include "rtbot/std/MovingKeyCount.h"

using namespace rtbot;

SCENARIO("MovingKeyCount counts key occurrences in a sliding window", "[moving_key_count]") {
  SECTION("Basic window counting") {
    auto mkc = std::make_shared<MovingKeyCount>("mkc1", 5);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    mkc->connect(col, 0, 0);
    REQUIRE(mkc->type_name() == "MovingKeyCount");
    REQUIRE(mkc->get_window_size() == 5);

    std::vector<double> keys = {1, 2, 1, 1, 2, 3, 1, 2, 1, 1};
    for (size_t i = 0; i < keys.size(); i++) {
      mkc->receive_data(
          create_message<NumberData>(static_cast<timestamp_t>(i + 1),
                                     NumberData{keys[i]}),
          0);
    }
    mkc->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 10);

    std::vector<double> expected = {1, 1, 2, 3, 2, 1, 3, 2, 2, 3};
    for (size_t i = 0; i < expected.size(); i++) {
      const auto* msg =
          dynamic_cast<const Message<NumberData>*>(out[i].get());
      REQUIRE(msg->time == static_cast<timestamp_t>(i + 1));
      REQUIRE(msg->data.value == Approx(expected[i]));
    }
  }
}

SCENARIO("MovingKeyCount emits current key count, not window total", "[moving_key_count]") {
  SECTION("Same key every message") {
    auto mkc = std::make_shared<MovingKeyCount>("mkc1", 3);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    mkc->connect(col, 0, 0);

    mkc->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
    mkc->receive_data(create_message<NumberData>(2, NumberData{42.0}), 0);
    mkc->receive_data(create_message<NumberData>(3, NumberData{42.0}), 0);
    mkc->receive_data(create_message<NumberData>(4, NumberData{42.0}), 0);
    mkc->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 4);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(1.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[1].get())->data.value == Approx(2.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[2].get())->data.value == Approx(3.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[3].get())->data.value == Approx(3.0));
  }

  SECTION("Key drops to zero after eviction") {
    auto mkc = std::make_shared<MovingKeyCount>("mkc1", 2);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    mkc->connect(col, 0, 0);

    mkc->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    mkc->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    mkc->receive_data(create_message<NumberData>(3, NumberData{2.0}), 0);
    mkc->receive_data(create_message<NumberData>(4, NumberData{1.0}), 0);
    mkc->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 4);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(1.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[1].get())->data.value == Approx(1.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[2].get())->data.value == Approx(2.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[3].get())->data.value == Approx(1.0));
  }
}

SCENARIO("MovingKeyCount window_size=1 always returns 1", "[moving_key_count]") {
  SECTION("Every message is its own window") {
    auto mkc = std::make_shared<MovingKeyCount>("mkc1", 1);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    mkc->connect(col, 0, 0);

    mkc->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    mkc->receive_data(create_message<NumberData>(2, NumberData{5.0}), 0);
    mkc->receive_data(create_message<NumberData>(3, NumberData{9.0}), 0);
    mkc->execute();

    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 3);
    for (size_t i = 0; i < 3; i++) {
      REQUIRE(dynamic_cast<const Message<NumberData>*>(out[i].get())->data.value == Approx(1.0));
    }
  }
}

SCENARIO("MovingKeyCount serialization roundtrip", "[moving_key_count][State]") {
  SECTION("Collect and restore mid-stream") {
    auto mkc = std::make_shared<MovingKeyCount>("mkc1", 3);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    mkc->connect(col, 0, 0);

    mkc->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    mkc->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    mkc->receive_data(create_message<NumberData>(3, NumberData{1.0}), 0);
    mkc->execute();

    auto state = mkc->collect();
    auto restored = std::make_shared<MovingKeyCount>("mkc1", 3);
    auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    restored->connect(rcol, 0, 0);
    restored->restore_data_from_json(state);
    REQUIRE(*restored == *mkc);

    // Continue feeding
    restored->receive_data(create_message<NumberData>(4, NumberData{1.0}), 0);
    restored->execute();

    auto& out = rcol->get_data_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(2.0));
  }
}

SCENARIO("MovingKeyCount rejects window_size=0", "[moving_key_count]") {
  SECTION("Constructor throws") {
    REQUIRE_THROWS_AS(std::make_shared<MovingKeyCount>("mkc1", 0), std::runtime_error);
  }
}
