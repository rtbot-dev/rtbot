#include <catch2/catch.hpp>
#include <cmath>
#include <memory>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/std/FilterScalar.h"

using namespace rtbot;

SCENARIO("FilterScalarOp derived classes handle basic filtering", "[filter_scalar_op]") {
  SECTION("LessThan operator") {
    auto lt = std::make_shared<LessThan>("lt1", 3.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    lt->connect(col, 0, 0);

    REQUIRE(lt->type_name() == "LessThan");
    REQUIRE(lt->get_threshold() == 3.0);

    std::vector<std::pair<timestamp_t, double>> inputs = {
        {1, 1.0}, {2, 4.0}, {4, 2.5}, {5, 3.0}};

    std::vector<std::pair<timestamp_t, double>> expected = {{1, 1.0}, {4, 2.5}};

    for (const auto& input : inputs) {
      lt->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    lt->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == expected.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }
  }

  SECTION("GreaterThan operator") {
    auto gt = std::make_shared<GreaterThan>("gt1", 3.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    gt->connect(col, 0, 0);

    REQUIRE(gt->type_name() == "GreaterThan");
    REQUIRE(gt->get_threshold() == 3.0);

    std::vector<std::pair<timestamp_t, double>> inputs = {
        {1, 1.0}, {2, 4.0}, {4, 2.5}, {5, 3.0}};

    std::vector<std::pair<timestamp_t, double>> expected = {{2, 4.0}};

    for (const auto& input : inputs) {
      gt->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    gt->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == expected.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }
  }

  SECTION("GreaterThan operator small value") {
    auto gt = std::make_shared<GreaterThan>("gt1", 0.5);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    gt->connect(col, 0, 0);

    REQUIRE(gt->type_name() == "GreaterThan");
    REQUIRE(gt->get_threshold() == 0.5);

    std::vector<std::pair<timestamp_t, double>> inputs = {
        {0, 0.3}, {1, 1.0}, {2, 4.0}, {4, 0.2}, {5, 0.5}};

    std::vector<std::pair<timestamp_t, double>> expected = {{1, 1.0}, {2, 4.0}};

    for (const auto& input : inputs) {
      gt->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    gt->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == expected.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }
  }

  SECTION("EqualTo operator") {
    auto eq = std::make_shared<EqualTo>("eq1", 3.0, 0.1);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    eq->connect(col, 0, 0);

    REQUIRE(eq->type_name() == "EqualTo");
    REQUIRE(eq->get_value() == 3.0);
    REQUIRE(eq->get_epsilon() == 0.1);

    std::vector<std::pair<timestamp_t, double>> inputs = {
        {1, 1.0}, {2, 2.95}, {4, 3.05}, {5, 3.2}};

    std::vector<std::pair<timestamp_t, double>> expected = {{2, 2.95}, {4, 3.05}};

    for (const auto& input : inputs) {
      eq->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    eq->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == expected.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }
  }
}

SCENARIO("FilterScalarOp handles edge cases", "[filter_scalar_op]") {
  SECTION("NaN values") {
    auto gt = std::make_shared<GreaterThan>("gt1", 0.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    gt->connect(col, 0, 0);

    gt->receive_data(create_message<NumberData>(1, NumberData{std::numeric_limits<double>::quiet_NaN()}), 0);
    gt->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.empty());
  }

  SECTION("Infinity values") {
    auto lt = std::make_shared<LessThan>("lt1", std::numeric_limits<double>::infinity());
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    lt->connect(col, 0, 0);

    lt->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    lt->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(!output.empty());
  }

  SECTION("Exact equality with zero epsilon") {
    auto eq = std::make_shared<EqualTo>("eq1", 3.0, 0.0);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    eq->connect(col, 0, 0);

    eq->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
    eq->receive_data(create_message<NumberData>(2, NumberData{3.0 + 1e-15}), 0);
    eq->execute();

    auto& output = col->get_data_queue(0);
    REQUIRE(output.size() == 1);
  }
}

SCENARIO("FilterScalarOp handles error cases", "[filter_scalar_op]") {
  SECTION("Invalid message type") {
    auto lt = std::make_shared<LessThan>("lt1", 3.0);

    REQUIRE_THROWS_AS(lt->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
  }

  SECTION("Invalid port index") {
    auto lt = std::make_shared<LessThan>("lt1", 3.0);

    REQUIRE_THROWS_AS(lt->receive_data(create_message<NumberData>(1, NumberData{1.0}), 1), std::runtime_error);
  }
}

SCENARIO("FilterScalarOp handles serialization", "[filter_scalar_op][State]") {
  SECTION("operators serialization") {
    auto lt = std::make_shared<LessThan>("lt1", 3.0);
    auto gt = std::make_shared<GreaterThan>("gt1", 3.0);
    auto et = std::make_shared<EqualTo>("et1", 3.0);
    auto net = std::make_shared<NotEqualTo>("net1", 3.0);

    // Fill with some data
    lt->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    lt->receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
    lt->execute();

    gt->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    gt->receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
    gt->execute();

    et->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    et->receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
    et->execute();

    net->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    net->receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
    net->execute();

    WHEN("LessThan is serialized and restored") {
      auto state = lt->collect();
      auto restored = std::make_shared<LessThan>("lt1", 3.0);
      auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);
      restored->restore_data_from_json(state);

      REQUIRE(*restored == *lt);
      REQUIRE(restored->type_name() == lt->type_name());
      REQUIRE(restored->get_threshold() == lt->get_threshold());

      restored->receive_data(create_message<NumberData>(3, NumberData{2.0}), 0);
      restored->execute();

      auto& output = rcol->get_data_queue(0);
      REQUIRE(!output.empty());
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
      REQUIRE(msg->time == 3);
      REQUIRE(msg->data.value == 2.0);
    }

    WHEN("GreaterThan is serialized and restored") {
      auto state = gt->collect();
      auto restored = std::make_shared<GreaterThan>("gt1", 3.0);
      auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);
      restored->restore_data_from_json(state);

      REQUIRE(*restored == *gt);

      restored->receive_data(create_message<NumberData>(3, NumberData{2.0}), 0);
      restored->execute();

      auto& output = rcol->get_data_queue(0);
      REQUIRE(output.empty());
    }

    WHEN("EqualTo is serialized and restored") {
      auto state = et->collect();
      auto restored = std::make_shared<EqualTo>("et1", 3.0);
      auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);
      restored->restore_data_from_json(state);

      REQUIRE(*restored == *et);

      restored->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      restored->execute();

      auto& output = rcol->get_data_queue(0);
      REQUIRE(!output.empty());
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
      REQUIRE(msg->time == 3);
      REQUIRE(msg->data.value == 3.0);
    }

    WHEN("NotEqualTo is serialized and restored") {
      auto state = net->collect();
      auto restored = std::make_shared<NotEqualTo>("net1", 3.0);
      auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);
      restored->restore_data_from_json(state);

      REQUIRE(*restored == *net);

      restored->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      restored->execute();

      auto& output = rcol->get_data_queue(0);
      REQUIRE(output.empty());
    }
  }
}
