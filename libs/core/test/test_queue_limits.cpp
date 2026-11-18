#include <catch2/catch.hpp>
#include "rtbot/Collector.h"
#include "rtbot/Join.h"
#include "rtbot/Input.h"
#include "rtbot/QueueLimits.h"

using namespace rtbot;

namespace {
class QueueProbe : public Operator {
 public:
  QueueProbe(std::string id, QueueLimits limits) : Operator(std::move(id), limits) {
    add_data_port<NumberData>();
    add_control_port<NumberData>();
    add_output_port<NumberData>();
  }
  std::string type_name() const override { return "QueueProbe"; }
  void send(timestamp_t time, bool debug) { execute(debug); emit_output(0, create_message<NumberData>(time, NumberData{1}), debug); }
  void send_batch(size_t count, bool debug) {
    execute(debug);
    std::vector<std::unique_ptr<BaseMessage>> batch;
    for (size_t i = 1; i <= count; ++i) batch.push_back(create_message<NumberData>(i, NumberData{1}));
    emit_output(0, std::move(batch), debug);
  }
 protected:
  void process_data(bool) override {}
};
}

TEST_CASE("Queue configuration is explicit and strictly parsed", "[queue_limits]") {
  REQUIRE(QueueLimits::parse(nullptr, nullptr).max_messages == 17280);
  REQUIRE(QueueLimits::parse(nullptr, nullptr).overflow == QueueOverflowPolicy::Error);
  REQUIRE(QueueLimits::parse("0", "drop_oldest").max_messages == 0);
  REQUIRE(QueueLimits::parse("42", "drop_oldest").overflow == QueueOverflowPolicy::DropOldest);
  for (auto bad : {"", "-1", "+1", " 1", "1.0", "10oops", "9999999999999999999999999"})
    REQUIRE_THROWS_AS(QueueLimits::parse(bad, nullptr), std::invalid_argument);
  REQUIRE_THROWS_AS(QueueLimits::parse(nullptr, "silent"), std::invalid_argument);
}

TEST_CASE("Direct data and control ingress reject before growing or advancing timestamps", "[queue_limits]") {
  for (auto kind : {PortKind::DATA, PortKind::CONTROL}) {
    QueueProbe probe("bounded", {2, QueueOverflowPolicy::Error});
    auto receive = [&](int time) {
      auto message = create_message<NumberData>(time, NumberData{1});
      if (kind == PortKind::DATA) probe.receive_data(std::move(message), 0, true);
      else probe.receive_control(std::move(message), 0, true);
    };
    receive(1); receive(2);
    REQUIRE_THROWS_WITH(receive(3), Catch::Contains("Queue limit exceeded"));
    auto& queue = kind == PortKind::DATA ? probe.get_data_queue(0) : probe.get_control_queue(0);
    REQUIRE(queue.size() == 2);
    REQUIRE(queue.front()->time == 1);
    REQUIRE(probe.queue_statistics(kind, 0).rejected_messages == 1);
    queue.pop_front();
    REQUIRE_NOTHROW(receive(3));
  }
}

TEST_CASE("Fast dispatch bounds stalled data and control queues in all modes", "[queue_limits]") {
  for (bool batch : {false, true}) for (bool debug : {false, true})
  for (auto kind : {PortKind::DATA, PortKind::CONTROL}) {
    QueueProbe source("source", {0, QueueOverflowPolicy::Error});
    auto sink = std::make_shared<QueueProbe>("stalled", QueueLimits{3, QueueOverflowPolicy::DropOldest});
    source.connect(sink, 0, 0, kind);
    if (batch) source.send_batch(100, debug);
    else for (int i = 1; i <= 100; ++i) source.send(i, debug);
    auto& queue = kind == PortKind::DATA ? sink->get_data_queue(0) : sink->get_control_queue(0);
    REQUIRE(queue.size() == 3);
    REQUIRE(queue.front()->time == 98);
    REQUIRE(queue.back()->time == 100);
    REQUIRE(sink->queue_statistics(kind, 0).dropped_messages == 97);
    REQUIRE(sink->queue_statistics(kind, 0).peak_messages == 3);
  }
}

TEST_CASE("Fast dispatch reports overflow instead of silently dropping", "[queue_limits]") {
  for (bool batch : {false, true}) for (bool debug : {false, true}) {
    QueueProbe source("source", {0, QueueOverflowPolicy::Error});
    auto sink = std::make_shared<QueueProbe>("stalled", QueueLimits{2, QueueOverflowPolicy::Error});
    source.connect(sink);
    auto fill = [&] {
      if (batch) source.send_batch(3, debug);
      else for (int i = 1; i <= 3; ++i) source.send(i, debug);
    };
    REQUIRE_THROWS_WITH(fill(), Catch::Contains("stalled"));
    REQUIRE(sink->get_data_queue(0).size() == 2);
    REQUIRE(sink->queue_statistics(PortKind::DATA, 0).rejected_messages == 1);
    sink->get_data_queue(0).pop_front();
    REQUIRE_NOTHROW(source.send(3, debug));
  }
}

TEST_CASE("A stalled join stays bounded and resumes on matching input", "[queue_limits]") {
  auto join = make_number_join("join", 2);
  join->set_queue_limits({3, QueueOverflowPolicy::DropOldest});
  auto output = make_collector("output", {"number", "number"});
  join->connect(output, 0, 0); join->connect(output, 1, 1);
  QueueProbe source("source", {0, QueueOverflowPolicy::Error});
  source.connect(join);
  for (int i = 1; i <= 50000; ++i) { source.send(i, false); join->execute(); }
  REQUIRE(join->get_data_queue(0).size() == 3);
  REQUIRE(join->queue_statistics(PortKind::DATA, 0).dropped_messages == 49997);
  join->receive_data(create_message<NumberData>(50000, NumberData{2}), 1);
  join->execute();
  REQUIRE(output->get_data_queue(0).size() == 1);
  REQUIRE(output->get_data_queue(0).front()->time == 50000);
}

TEST_CASE("Snapshot recovery rejects oversized queues without truncating persisted state", "[queue_limits]") {
  QueueProbe source("saved", {0, QueueOverflowPolicy::Error});
  for (int i = 1; i <= 4; ++i) source.receive_data(create_message<NumberData>(i, NumberData{1}), 0);
  auto state = source.collect();
  QueueProbe restored("restored", {3, QueueOverflowPolicy::DropOldest});
  REQUIRE_THROWS_WITH(restored.restore_data_from_json(state), Catch::Contains("Snapshot queue exceeds"));
  REQUIRE(restored.get_data_queue(0).empty());
  restored.set_queue_limits({4, QueueOverflowPolicy::Error});
  REQUIRE_NOTHROW(restored.restore_data_from_json(state));
  REQUIRE(restored.get_data_queue(0).size() == 4);
  REQUIRE_THROWS_AS(restored.set_queue_limits({3, QueueOverflowPolicy::DropOldest}), std::invalid_argument);
}

TEST_CASE("Collectors retain complete output batches above the default input bound", "[queue_limits]") {
  QueueProbe source("source", {0, QueueOverflowPolicy::Error});
  auto sink = make_number_collector("collector");
  source.connect(sink);
  source.send_batch(20000, false);
  REQUIRE(sink->get_data_queue(0).size() == 20000);
}

TEST_CASE("Explicit recovery truncation keeps newest history and subsequent ports aligned", "[queue_limits]") {
  QueueProbe saved("saved", {0, QueueOverflowPolicy::Error});
  for (int i = 1; i <= 10; ++i) {
    saved.receive_data(create_message<NumberData>(i, NumberData{double(i)}), 0);
    saved.receive_control(create_message<NumberData>(i, NumberData{double(i)}), 0);
  }
  QueueProbe restored("restored", {3, QueueOverflowPolicy::DropOldest, QueueOverflowPolicy::DropOldest});
  restored.restore_data_from_json(saved.collect());
  for (auto kind : {PortKind::DATA, PortKind::CONTROL}) {
    auto& q = kind == PortKind::DATA ? restored.get_data_queue(0) : restored.get_control_queue(0);
    REQUIRE(q.size() == 3);
    REQUIRE(q.front()->time == 8);
    REQUIRE(q.back()->time == 10);
    REQUIRE(restored.queue_statistics(kind, 0).dropped_messages == 7);
  }
  REQUIRE(QueueLimits::parse("3", "drop_oldest", "drop_oldest").restore_overflow == QueueOverflowPolicy::DropOldest);
  REQUIRE_THROWS_AS(QueueLimits::parse("3", "error", "invalid"), std::invalid_argument);
}

TEST_CASE("Input boundary does not swallow capacity errors", "[queue_limits]") {
  auto input = make_number_input("input");
  input->set_queue_limits({1, QueueOverflowPolicy::Error});
  input->receive_data(create_message<NumberData>(1, {1}), 0);
  REQUIRE_THROWS_AS(input->receive_data(create_message<NumberData>(2, {2}), 0), QueueLimitExceeded);
  input->execute();
  REQUIRE_NOTHROW(input->receive_data(create_message<NumberData>(2, {2}), 0));
  REQUIRE(input->get_data_queue(0).size() == 1);
}
