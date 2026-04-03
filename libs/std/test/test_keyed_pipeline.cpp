#include <catch2/catch.hpp>
#include <memory>
#include <vector>

#include "rtbot/std/CumulativeSum.h"
#include "rtbot/std/KeyedPipeline.h"
#include "rtbot/std/VectorExtract.h"
#include "rtbot/std/VectorProject.h"

using namespace rtbot;

// Helper: creates a sub-graph with VectorExtract(index) → CumulativeSum
// Input: VectorNumberData, Output: NumberData (cumulative sum of extracted field)
static KeyedPipeline::SubGraphFactory make_extract_cumsum_factory(int extract_index) {
  return [extract_index]() -> SubGraph {
    SubGraph sg;
    auto ext = std::make_shared<VectorExtract>("ext", extract_index);
    auto sum = std::make_shared<CumulativeSum>("sum");
    ext->connect(sum, 0, 0);
    sg.operators["ext"] = ext;
    sg.operators["sum"] = sum;
    sg.entry = ext;
    sg.output = sum;
    return sg;
  };
}

// Helper: creates a sub-graph with VectorProject(indices)
// Input: VectorNumberData, Output: VectorNumberData (projected fields)
static KeyedPipeline::SubGraphFactory make_project_factory(std::vector<int> indices) {
  return [indices]() -> SubGraph {
    SubGraph sg;
    auto proj = std::make_shared<VectorProject>("proj", indices);
    sg.operators["proj"] = proj;
    sg.entry = proj;
    sg.output = proj;
    return sg;
  };
}

SCENARIO("KeyedPipeline routes messages by key with persistent state", "[keyed_pipeline]") {
  SECTION("Basic routing with 2 keys") {
    // Prototype: VectorExtract(index=0) → CumulativeSum
    // Key now comes from msg->id, data is [value]
    auto kp = make_keyed_pipeline("kp1", make_extract_cumsum_factory(0));

    REQUIRE(kp->type_name() == "KeyedPipeline");
    REQUIRE(kp->num_keys() == 0);

    // t=1, id=1: [100.0] → key=1, extract=100, sum=100
    kp->receive_data(create_message<VectorNumberData>(1, 1, VectorNumberData{{100.0}}), 0);
    kp->execute();

    {
      auto& out = kp->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 1);
      REQUIRE(msg->id == 1);
      REQUIRE(msg->data.values.size() == 1);
      REQUIRE(msg->data.values[0] == 100.0);  // sum
    }

    kp->clear_all_output_ports();

    // t=2, id=2: [200.0] → key=2, extract=200, sum=200
    kp->receive_data(create_message<VectorNumberData>(2, 2, VectorNumberData{{200.0}}), 0);
    kp->execute();

    {
      auto& out = kp->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 2);
      REQUIRE(msg->id == 2);
      REQUIRE(msg->data.values[0] == 200.0);  // sum
    }

    kp->clear_all_output_ports();

    // t=3, id=1: [150.0] → key=1, extract=150, sum=100+150=250
    kp->receive_data(create_message<VectorNumberData>(3, 1, VectorNumberData{{150.0}}), 0);
    kp->execute();

    {
      auto& out = kp->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 3);
      REQUIRE(msg->id == 1);
      REQUIRE(msg->data.values[0] == 250.0);  // cumulative sum for key=1
    }

    kp->clear_all_output_ports();

    // t=4, id=2: [250.0] → key=2, extract=250, sum=200+250=450
    kp->receive_data(create_message<VectorNumberData>(4, 2, VectorNumberData{{250.0}}), 0);
    kp->execute();

    {
      auto& out = kp->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 4);
      REQUIRE(msg->id == 2);
      REQUIRE(msg->data.values[0] == 450.0);  // cumulative sum for key=2
    }

    REQUIRE(kp->num_keys() == 2);
  }
}

SCENARIO("KeyedPipeline handles VectorNumberData output from sub-graph", "[keyed_pipeline]") {
  SECTION("VectorProject output") {
    // Prototype: VectorProject(indices=[0,1]) — outputs VectorNumberData
    auto kp = make_keyed_pipeline("kp1", make_project_factory({0, 1}));

    // Input: id=1, [price, quantity]
    kp->receive_data(create_message<VectorNumberData>(1, 1, VectorNumberData{{10.5, 3.0}}), 0);
    kp->execute();

    auto& out = kp->get_output_queue(0);
    REQUIRE(out.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
    REQUIRE(msg->time == 1);
    REQUIRE(msg->id == 1);
    REQUIRE(msg->data.values.size() == 2);
    REQUIRE(msg->data.values[0] == 10.5);     // price
    REQUIRE(msg->data.values[1] == 3.0);      // quantity
  }
}

SCENARIO("KeyedPipeline serialization roundtrip", "[keyed_pipeline][State]") {
  SECTION("Collect and restore preserves per-key state") {
    auto factory = make_extract_cumsum_factory(0);
    auto kp = make_keyed_pipeline("kp1", factory);

    // Process 4 messages (2 keys, 2 messages each)
    kp->receive_data(create_message<VectorNumberData>(1, 1, VectorNumberData{{100.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(2, 2, VectorNumberData{{200.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(3, 1, VectorNumberData{{150.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(4, 2, VectorNumberData{{250.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    // Collect state as JSON
    auto state = kp->collect();

    // Restore into a new instance
    auto kp2 = make_keyed_pipeline("kp1", factory);
    kp2->restore_data_from_json(state);

    REQUIRE(kp2->num_keys() == 2);

    // Process more messages and verify state continuity
    // key=1: previous sum was 250, new value 50 → sum=300
    kp2->receive_data(create_message<VectorNumberData>(5, 1, VectorNumberData{{50.0}}), 0);
    kp2->execute();

    {
      auto& out = kp2->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 5);
      REQUIRE(msg->id == 1);
      REQUIRE(msg->data.values[0] == 300.0);  // 100 + 150 + 50
    }

    kp2->clear_all_output_ports();

    // key=2: previous sum was 450, new value 50 → sum=500
    kp2->receive_data(create_message<VectorNumberData>(6, 2, VectorNumberData{{50.0}}), 0);
    kp2->execute();

    {
      auto& out = kp2->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 6);
      REQUIRE(msg->id == 2);
      REQUIRE(msg->data.values[0] == 500.0);  // 200 + 250 + 50
    }
  }
}

SCENARIO("KeyedPipeline handles many keys", "[keyed_pipeline]") {
  SECTION("100 distinct keys, 10 messages each") {
    auto kp = make_keyed_pipeline("kp1", make_extract_cumsum_factory(0));

    timestamp_t t = 1;
    for (int round = 0; round < 10; round++) {
      for (int key = 0; key < 100; key++) {
        uint64_t key_id = static_cast<uint64_t>(key);
        double value = static_cast<double>(round + 1) * 10.0;  // 10, 20, 30, ...

        kp->receive_data(create_message<VectorNumberData>(t, key_id, VectorNumberData{{value}}), 0);
        kp->execute();

        auto& out = kp->get_output_queue(0);
        REQUIRE(out.size() == 1);
        auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());

        // Expected cumulative sum for this key at this round
        double expected_sum = 0.0;
        for (int r = 0; r <= round; r++) {
          expected_sum += static_cast<double>(r + 1) * 10.0;
        }

        REQUIRE(msg->id == key_id);
        REQUIRE(msg->data.values[0] == Approx(expected_sum));

        kp->clear_all_output_ports();
        t++;
      }
    }

    REQUIRE(kp->num_keys() == 100);
  }
}

SCENARIO("KeyedPipeline new key callback", "[keyed_pipeline]") {
  SECTION("Callback called exactly once per new key") {
    std::vector<uint64_t> new_keys;
    auto kp = make_keyed_pipeline("kp1", make_extract_cumsum_factory(0));
    kp->set_new_key_callback([&new_keys](uint64_t key) { new_keys.push_back(key); });

    // Process messages with keys [1, 2, 1, 3, 2]
    kp->receive_data(create_message<VectorNumberData>(1, 1, VectorNumberData{{10.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(2, 2, VectorNumberData{{20.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(3, 1, VectorNumberData{{30.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(4, 3, VectorNumberData{{40.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(5, 2, VectorNumberData{{50.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    REQUIRE(new_keys.size() == 3);
    REQUIRE(new_keys[0] == 1);
    REQUIRE(new_keys[1] == 2);
    REQUIRE(new_keys[2] == 3);
  }
}
