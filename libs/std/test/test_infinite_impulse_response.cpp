#include <catch2/catch.hpp>

#include "rtbot/Collector.h"
#include "rtbot/std/InfiniteImpulseResponse.h"

using namespace rtbot;

SCENARIO("InfiniteImpulseResponse operator handles basic filtering", "[iir]") {
  GIVEN("A simple first-order IIR filter") {
    std::vector<double> b = {1.0};
    std::vector<double> a = {0.5};
    auto iir = std::make_shared<InfiniteImpulseResponse>("iir1", b, a);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    iir->connect(col, 0, 0);

    WHEN("Processing a sequence of inputs") {
      iir->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      iir->execute();

      THEN("First output is purely feed-forward") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->data.value == Approx(1.0));
      }

      AND_WHEN("Processing second input") {
        iir->receive_data(create_message<NumberData>(2, NumberData{1.0}), 0);
        iir->execute();

        THEN("Output includes feedback term") {
          const auto& output = col->get_data_queue(0);
          REQUIRE(output.size() == 2);
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[1].get());
          REQUIRE(msg->data.value == Approx(0.5));  // 1.0 - 0.5*1.0
        }
      }
    }
  }

  GIVEN("A second-order IIR filter") {
    std::vector<double> b = {1.0, -0.5};
    std::vector<double> a = {0.2, 0.1};
    auto iir = std::make_shared<InfiniteImpulseResponse>("iir2", b, a);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    iir->connect(col, 0, 0);

    WHEN("Processing a sequence of inputs") {
      // First input just gets buffered
      iir->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      iir->execute();
      REQUIRE(col->get_data_queue(0).empty());

      // Second input produces first output using only feed-forward terms
      iir->receive_data(create_message<NumberData>(2, NumberData{1.0}), 0);
      iir->execute();
      {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 2);
        REQUIRE(msg->data.value == Approx(0.5));  // 1.0 - 0.5*1.0
      }

      // Third input includes first feedback term
      col->reset();
      iir->receive_data(create_message<NumberData>(3, NumberData{1.0}), 0);
      iir->execute();
      {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(0.4));  // 1.0 - 0.5*1.0 - 0.2*0.5
      }

      // Fourth input includes both feedback terms
      col->reset();
      iir->receive_data(create_message<NumberData>(4, NumberData{1.0}), 0);
      iir->execute();
      {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 4);
        REQUIRE(msg->data.value == Approx(0.37));  // 1.0 - 0.5*1.0 - 0.2*0.4 - 0.1*0.5
      }
    }
  }
}

SCENARIO("InfiniteImpulseResponse operator validates input", "[iir]") {
  SECTION("Empty coefficient vectors") {
    REQUIRE_THROWS_AS(InfiniteImpulseResponse("iir", std::vector<double>{}, std::vector<double>{0.5}),
                      std::runtime_error);

    REQUIRE_THROWS_AS(InfiniteImpulseResponse("iir", std::vector<double>{1.0}, std::vector<double>{}),
                      std::runtime_error);
  }

  SECTION("Invalid message type") {
    auto iir = std::make_shared<InfiniteImpulseResponse>("iir", std::vector<double>{1.0}, std::vector<double>{0.5});
    REQUIRE_THROWS_AS(iir->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
  }
}

SCENARIO("InfiniteImpulseResponse operator maintains buffer size", "[iir]") {
  GIVEN("A first-order IIR filter") {
    std::vector<double> b = {1.0};
    std::vector<double> a = {0.5};
    auto iir = std::make_shared<InfiniteImpulseResponse>("iir1", b, a);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    iir->connect(col, 0, 0);

    WHEN("Processing many inputs") {
      for (int i = 0; i < 10; i++) {
        iir->receive_data(create_message<NumberData>(i, NumberData{1.0}), 0);
      }
      iir->execute();

      THEN("Output remains stable") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 10);
        // Check last few outputs converge to steady state
        auto* last = dynamic_cast<const Message<NumberData>*>(output.back().get());
        REQUIRE(last->data.value == Approx(0.666667).epsilon(0.001));
      }
    }
  }
}

SCENARIO("InfiniteImpulseResponse operator handles serialization", "[iir]") {
  GIVEN("A second-order IIR filter with processed data") {
    std::vector<double> b = {1.0, -0.5};
    std::vector<double> a = {0.2, 0.1};
    auto iir = std::make_shared<InfiniteImpulseResponse>("iir2", b, a);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    iir->connect(col, 0, 0);

    // Process enough data to fill buffers
    iir->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    iir->receive_data(create_message<NumberData>(2, NumberData{0.5}), 0);
    iir->receive_data(create_message<NumberData>(3, NumberData{0.8}), 0);
    iir->execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      auto state = iir->collect();

      // Create new operator with same configuration
      auto restored = std::make_shared<InfiniteImpulseResponse>("iir2", b, a);
      auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);

      // Restore state
      restored->restore_data_from_json(state);

      // Process new data on both operators
      col->reset();
      iir->receive_data(create_message<NumberData>(4, NumberData{0.7}), 0);
      restored->receive_data(create_message<NumberData>(4, NumberData{0.7}), 0);
      iir->execute();
      restored->execute();

      THEN("Both operators produce identical outputs") {
        const auto& orig_output = col->get_data_queue(0);
        const auto& rest_output = rcol->get_data_queue(0);

        REQUIRE(orig_output.size() == rest_output.size());
        REQUIRE(*iir == *restored);

        auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output[0].get());
        auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output[0].get());

        REQUIRE(orig_msg->time == rest_msg->time);
        REQUIRE(orig_msg->data.value == Approx(rest_msg->data.value));
      }
    }
  }
}

SCENARIO("InfiniteImpulseResponse init_from_first_sample seeds EMA without startup transient", "[iir][ema]") {
  GIVEN("An EMA(alpha=0.1) configured via InfiniteImpulseResponse with init_from_first_sample=true") {
    std::vector<double> b = {0.1};
    std::vector<double> a = {-0.9};
    auto ema_unseeded = std::make_shared<InfiniteImpulseResponse>("ema0", b, a, false);
    auto ema_seeded = std::make_shared<InfiniteImpulseResponse>("ema1", b, a, true);
    auto col_unseeded = std::make_shared<Collector>("cu", std::vector<std::string>{"number"});
    auto col_seeded = std::make_shared<Collector>("cs", std::vector<std::string>{"number"});
    ema_unseeded->connect(col_unseeded, 0, 0);
    ema_seeded->connect(col_seeded, 0, 0);

    WHEN("Feeding a constant stream [100.0, 100.0, 100.0]") {
      for (timestamp_t t = 1; t <= 3; ++t) {
        ema_unseeded->receive_data(create_message<NumberData>(t, NumberData{100.0}), 0);
        ema_seeded->receive_data(create_message<NumberData>(t, NumberData{100.0}), 0);
      }
      ema_unseeded->execute();
      ema_seeded->execute();

      THEN("Unseeded IIR has startup transient while seeded IIR outputs exact steady-state from t=1") {
        const auto& out_u = col_unseeded->get_data_queue(0);
        const auto& out_s = col_seeded->get_data_queue(0);
        REQUIRE(out_u.size() == 3);
        REQUIRE(out_s.size() == 3);

        // Unseeded: y_1 = 0.1 * 100 = 10.0, y_2 = 10 + 9 = 19.0
        REQUIRE(dynamic_cast<const Message<NumberData>*>(out_u[0].get())->data.value == Approx(10.0));
        REQUIRE(dynamic_cast<const Message<NumberData>*>(out_u[1].get())->data.value == Approx(19.0));

        // Seeded: y_1 = 100.0, y_2 = 100.0, y_3 = 100.0
        REQUIRE(dynamic_cast<const Message<NumberData>*>(out_s[0].get())->data.value == Approx(100.0));
        REQUIRE(dynamic_cast<const Message<NumberData>*>(out_s[1].get())->data.value == Approx(100.0));
        REQUIRE(dynamic_cast<const Message<NumberData>*>(out_s[2].get())->data.value == Approx(100.0));
      }

      AND_WHEN("Serialized and restored") {
        auto state = ema_seeded->collect();
        auto restored = std::make_shared<InfiniteImpulseResponse>("ema1", b, a, true);
        auto col_restored = std::make_shared<Collector>("cr", std::vector<std::string>{"number"});
        restored->connect(col_restored, 0, 0);
        restored->restore_data_from_json(state);
        REQUIRE(*ema_seeded == *restored);

        col_seeded->reset();
        ema_seeded->receive_data(create_message<NumberData>(4, NumberData{200.0}), 0);
        restored->receive_data(create_message<NumberData>(4, NumberData{200.0}), 0);
        ema_seeded->execute();
        restored->execute();

        const auto* m_orig = dynamic_cast<const Message<NumberData>*>(col_seeded->get_data_queue(0)[0].get());
        const auto* m_rest = dynamic_cast<const Message<NumberData>*>(col_restored->get_data_queue(0)[0].get());
        REQUIRE(m_orig->data.value == Approx(110.0));
        REQUIRE(m_rest->data.value == Approx(110.0));
      }
    }
  }
}
