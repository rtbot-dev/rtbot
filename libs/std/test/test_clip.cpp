#include <catch2/catch.hpp>

#include "rtbot/std/Clip.h"

using namespace rtbot;

SCENARIO("Clip operator constrains values", "[clip]") {
  GIVEN("Clip with both bounds") {
    Clip clip("clip", -1.0, 1.0);

    WHEN("values within range arrive") {
      clip.receive_data(create_message<NumberData>(1, NumberData{0.5}), 0);
      clip.execute();
      const auto& output = clip.get_output_queue(0);
      REQUIRE(output.size() == 1);
      const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
      REQUIRE(msg != nullptr);
      REQUIRE(msg->data.value == Approx(0.5));
    }

    WHEN("value exceeds upper bound") {
      clip.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      clip.execute();
      const auto& output = clip.get_output_queue(0);
      REQUIRE(output.size() == 1);
      const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
      REQUIRE(msg->data.value == Approx(1.0));
    }

    WHEN("value below lower bound") {
      clip.receive_data(create_message<NumberData>(1, NumberData{-5.0}), 0);
      clip.execute();
      const auto& output = clip.get_output_queue(0);
      REQUIRE(output.size() == 1);
      const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
      REQUIRE(msg->data.value == Approx(-1.0));
    }
  }

  GIVEN("Clip with only lower bound") {
    Clip clip("clip_lower", 0.0, std::nullopt);
    clip.receive_data(create_message<NumberData>(1, NumberData{-3.0}), 0);
    clip.execute();
    const auto& output = clip.get_output_queue(0);
    REQUIRE(output.size() == 1);
    const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
    REQUIRE(msg->data.value == Approx(0.0));
  }
}
