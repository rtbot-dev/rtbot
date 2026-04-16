#include <catch2/catch.hpp>
#include <cmath>
#include <memory>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/std/ArithmeticScalar.h"

using namespace rtbot;

SCENARIO("ArithmeticScalar derived classes handle basic operations", "[math_scalar_op]") {
  SECTION("Add operator") {
    auto add = std::make_shared<Add>("add1", 2.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    add->connect(col, 0, 0);

    REQUIRE(add->type_name() == "Add");
    REQUIRE(add->get_value() == 2.0);

    add->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    add->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(msg->time == 1);
    REQUIRE(msg->data.value == 3.0);

    std::vector<std::pair<timestamp_t, double>> inputs = {{2, -1.0}, {4, 0.0}, {5, 10.0}};
    std::vector<std::pair<timestamp_t, double>> expected = {{2, 1.0}, {4, 2.0}, {5, 12.0}};

    col->reset();

    for (const auto& input : inputs) {
      add->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
      add->execute();
    }

    auto& received = col->get_data_queue(0);
    REQUIRE(received.size() == inputs.size());

    for (size_t i = 0; i < received.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(received[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }

    WHEN("State is serialized and restored") {
      auto state = add->collect();
      auto restored = std::make_shared<Add>("add1", 2.0);
      restored->restore_data_from_json(state);

      THEN("The operators match") {
        REQUIRE(*restored == *add);
      }
    }
  }

  SECTION("Scale operator") {
    auto scale = std::make_shared<Scale>("scale1", 2.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    scale->connect(col, 0, 0);

    REQUIRE(scale->type_name() == "Scale");
    REQUIRE(scale->get_value() == 2.0);

    std::vector<std::pair<timestamp_t, double>> inputs = {{1, 1.0}, {2, -1.0}, {4, 0.5}};
    std::vector<std::pair<timestamp_t, double>> expected = {{1, 2.0}, {2, -2.0}, {4, 1.0}};

    for (const auto& input : inputs) {
      scale->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    scale->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == inputs.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }

    WHEN("State is serialized and restored") {
      auto state = scale->collect();
      auto restored = std::make_shared<Scale>("scale1", 2.0);
      restored->restore_data_from_json(state);

      THEN("The operators match") {
        REQUIRE(*restored == *scale);
      }
    }
  }

  SECTION("Power operator") {
    auto power = std::make_shared<Power>("pow1", 2.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    power->connect(col, 0, 0);

    REQUIRE(power->type_name() == "Power");
    REQUIRE(power->get_value() == 2.0);

    std::vector<std::pair<timestamp_t, double>> inputs = {{1, 2.0}, {2, -2.0}, {4, 0.5}};
    std::vector<std::pair<timestamp_t, double>> expected = {{1, 4.0}, {2, 4.0}, {4, 0.25}};

    for (const auto& input : inputs) {
      power->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    power->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == inputs.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == Approx(expected[i].second));
    }

    WHEN("State is serialized and restored") {
      auto state = power->collect();
      auto restored = std::make_shared<Power>("pow1", 2.0);
      restored->restore_data_from_json(state);

      THEN("The operators match") {
        REQUIRE(*restored == *power);
      }
    }
  }
}

SCENARIO("ArithmeticScalar handles trigonometric functions", "[math_scalar_op]") {
  SECTION("Sin operator") {
    auto sin = std::make_shared<Sin>("sin1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    sin->connect(col, 0, 0);

    REQUIRE(sin->type_name() == "Sin");

    std::vector<std::pair<timestamp_t, double>> inputs = {{1, 0.0}, {2, M_PI / 2}, {4, M_PI}};
    std::vector<std::pair<timestamp_t, double>> expected = {{1, 0.0}, {2, 1.0}, {4, 0.0}};

    for (const auto& input : inputs) {
      sin->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    sin->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == inputs.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
    }

    WHEN("State is serialized and restored") {
      auto state = sin->collect();
      auto restored = std::make_shared<Sin>("sin1");
      restored->restore_data_from_json(state);

      THEN("The operators match") {
        REQUIRE(*restored == *sin);
      }
    }
  }
}

SCENARIO("ArithmeticScalar handles exponential and logarithmic functions", "[math_scalar_op]") {
  SECTION("Exp operator") {
    auto exp = std::make_shared<Exp>("exp1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    exp->connect(col, 0, 0);

    REQUIRE(exp->type_name() == "Exp");

    std::vector<std::pair<timestamp_t, double>> inputs = {{1, 0.0}, {2, 1.0}, {4, -1.0}};
    std::vector<std::pair<timestamp_t, double>> expected = {{1, 1.0}, {2, M_E}, {4, 1.0 / M_E}};

    for (const auto& input : inputs) {
      exp->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    exp->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == inputs.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == Approx(expected[i].second));
    }

    WHEN("State is serialized and restored") {
      auto state = exp->collect();
      auto restored = std::make_shared<Exp>("exp1");
      restored->restore_data_from_json(state);

      THEN("The operators match") {
        REQUIRE(*restored == *exp);
      }
    }
  }
}

SCENARIO("ArithmeticScalar handles rounding functions", "[math_scalar_op]") {
  SECTION("Round operator") {
    auto round = std::make_shared<Round>("round1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    round->connect(col, 0, 0);

    REQUIRE(round->type_name() == "Round");

    std::vector<std::pair<timestamp_t, double>> inputs = {{1, 1.4}, {2, 1.6}, {4, -1.5}, {5, -1.6}};
    std::vector<std::pair<timestamp_t, double>> expected = {{1, 1.0}, {2, 2.0}, {4, -2.0}, {5, -2.0}};

    for (const auto& input : inputs) {
      round->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }

    round->execute();
    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == inputs.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }

    WHEN("State is serialized and restored") {
      auto state = round->collect();
      auto restored = std::make_shared<Round>("round1");
      restored->restore_data_from_json(state);

      THEN("The operators match") {
        REQUIRE(*restored == *round);
      }
    }
  }
}

SCENARIO("ArithmeticScalar handles serialization", "[math_scalar_op][State]") {
  SECTION("Add operator serialization") {
    auto add = std::make_shared<Add>("add1", 2.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    add->connect(col, 0, 0);

    add->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    add->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    add->execute();

    auto state = add->collect();
    auto restored = std::make_shared<Add>("add1", 2.0);
    auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    restored->connect(rcol, 0, 0);
    restored->restore_data_from_json(state);

    REQUIRE(restored->type_name() == add->type_name());
    REQUIRE(restored->get_value() == add->get_value());
    REQUIRE(*restored == *add);

    restored->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
    restored->execute();

    auto& output = rcol->get_data_queue(0);
    REQUIRE(!output.empty());
    auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(msg->time == 3);
    REQUIRE(msg->data.value == 5.0);
  }
}

SCENARIO("ArithmeticScalar handles error cases", "[math_scalar_op]") {
  SECTION("Invalid message type") {
    auto add = std::make_shared<Add>("add1", 2.0);

    REQUIRE_THROWS_AS(add->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
  }

  SECTION("Invalid port index") {
    auto add = std::make_shared<Add>("add1", 2.0);

    REQUIRE_THROWS_AS(add->receive_data(create_message<NumberData>(1, NumberData{1.0}), 1), std::runtime_error);
  }

  SECTION("Log of negative number") {
    auto log = std::make_shared<Log>("log1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    log->connect(col, 0, 0);

    log->receive_data(create_message<NumberData>(1, NumberData{-1.0}), 0);
    log->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(!output.empty());
    auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(std::isnan(msg->data.value));
  }
}
