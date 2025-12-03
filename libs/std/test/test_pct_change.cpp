#include <catch2/catch.hpp>

#include "rtbot/std/PctChange.h"

using namespace rtbot;

SCENARIO("PctChange computes percentage variation between consecutive samples", "[PctChange]") {
  GIVEN("A PctChange operator receiving a value stream") {
    auto pct = std::make_unique<PctChange>("pct1");

    WHEN("Only the initial message is processed") {
      pct->receive_data(create_message<NumberData>(1, NumberData{100.0}), 0);
      pct->execute();

      THEN("No output is produced for lack of a baseline") {
        const auto& output = pct->get_output_queue(0);
        REQUIRE(output.empty());
      }
    }

    WHEN("A sequence of values is processed") {
      pct->receive_data(create_message<NumberData>(1, NumberData{100.0}), 0);
      pct->execute();
      pct->receive_data(create_message<NumberData>(2, NumberData{120.0}), 0);
      pct->execute();
      pct->receive_data(create_message<NumberData>(3, NumberData{150.0}), 0);
      pct->execute();

      THEN("Percentage change is emitted with the current timestamp") {
        const auto& output = pct->get_output_queue(0);
        REQUIRE(output.size() == 2);

        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());

        REQUIRE(msg1->time == 2);
        REQUIRE(msg2->time == 3);
        REQUIRE(msg1->data.value == Approx(0.20));
        REQUIRE(msg2->data.value == Approx(0.25));
      }
    }

    WHEN("The baseline is zero") {
      pct->receive_data(create_message<NumberData>(1, NumberData{0.0}), 0);
      pct->execute();
      pct->receive_data(create_message<NumberData>(2, NumberData{10.0}), 0);
      pct->execute();
      pct->receive_data(create_message<NumberData>(3, NumberData{5.0}), 0);
      pct->execute();

      THEN("Outputs start once the previous value becomes non-zero") {
        const auto& output = pct->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(-0.5));
      }
    }
  }
}

SCENARIO("PctChange handles reset and persisted state", "[PctChange][State]") {
  GIVEN("A PctChange operator that already processed samples") {
    auto pct = std::make_unique<PctChange>("pct_state");

    pct->receive_data(create_message<NumberData>(1, NumberData{100.0}), 0);
    pct->execute();
    pct->receive_data(create_message<NumberData>(2, NumberData{110.0}), 0);
    pct->execute();

    WHEN("The operator is reset") {
      pct->reset();
      pct->clear_all_output_ports();
      pct->receive_data(create_message<NumberData>(3, NumberData{130.0}), 0);
      pct->execute();

      THEN("No percentage change is produced because baseline was cleared") {
        const auto& output = pct->get_output_queue(0);
        REQUIRE(output.empty());
      }
    }

    WHEN("State is serialized and restored") {
      Bytes state = pct->collect();
      auto restored = std::make_unique<PctChange>("pct_state");
      auto it = state.cbegin();
      restored->restore(it);
      restored->clear_all_output_ports();

      restored->receive_data(create_message<NumberData>(3, NumberData{132.0}), 0);
      restored->execute();

      THEN("Restored operator continues using the stored previous value") {
        const auto& output = restored->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(0.2));
      }
    }
  }
}


