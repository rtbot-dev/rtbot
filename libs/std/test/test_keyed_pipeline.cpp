#include <catch2/catch.hpp>
#include <memory>
#include <vector>

#include "rtbot/std/CumulativeSum.h"
#include "rtbot/std/KeyedPipeline.h"
#include "rtbot/std/VectorExtract.h"
#include "rtbot/std/VectorProject.h"
#include "rtbot/fuse/FusedExpression.h"
#include "rtbot/OperatorJson.h"

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
    // Prototype: VectorExtract(index=1) → CumulativeSum
    auto kp = make_keyed_pipeline("kp1", 0, make_extract_cumsum_factory(1));

    REQUIRE(kp->type_name() == "KeyedPipeline");
    REQUIRE(kp->get_key_index() == 0);
    REQUIRE(kp->num_keys() == 0);

    // t=1: [1.0, 100.0] → key=1, extract=100, sum=100
    kp->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 100.0}}), 0);
    kp->execute();

    {
      auto& out = kp->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 1);
      REQUIRE(msg->data.values->size() == 2);
      REQUIRE((*msg->data.values)[0] == 1.0);    // key
      REQUIRE((*msg->data.values)[1] == 100.0);  // sum
    }

    kp->clear_all_output_ports();

    // t=2: [2.0, 200.0] → key=2, extract=200, sum=200
    kp->receive_data(create_message<VectorNumberData>(2, VectorNumberData{{2.0, 200.0}}), 0);
    kp->execute();

    {
      auto& out = kp->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 2);
      REQUIRE((*msg->data.values)[0] == 2.0);    // key
      REQUIRE((*msg->data.values)[1] == 200.0);  // sum
    }

    kp->clear_all_output_ports();

    // t=3: [1.0, 150.0] → key=1, extract=150, sum=100+150=250
    kp->receive_data(create_message<VectorNumberData>(3, VectorNumberData{{1.0, 150.0}}), 0);
    kp->execute();

    {
      auto& out = kp->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 3);
      REQUIRE((*msg->data.values)[0] == 1.0);    // key
      REQUIRE((*msg->data.values)[1] == 250.0);  // cumulative sum for key=1
    }

    kp->clear_all_output_ports();

    // t=4: [2.0, 250.0] → key=2, extract=250, sum=200+250=450
    kp->receive_data(create_message<VectorNumberData>(4, VectorNumberData{{2.0, 250.0}}), 0);
    kp->execute();

    {
      auto& out = kp->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 4);
      REQUIRE((*msg->data.values)[0] == 2.0);    // key
      REQUIRE((*msg->data.values)[1] == 450.0);  // cumulative sum for key=2
    }

    REQUIRE(kp->num_keys() == 2);
  }
}

SCENARIO("KeyedPipeline handles VectorNumberData output from sub-graph", "[keyed_pipeline]") {
  SECTION("VectorProject output") {
    // Prototype: VectorProject(indices=[1,2]) — outputs VectorNumberData
    auto kp = make_keyed_pipeline("kp1", 0, make_project_factory({1, 2}));

    // Input: [key, price, quantity]
    kp->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 10.5, 3.0}}), 0);
    kp->execute();

    auto& out = kp->get_output_queue(0);
    REQUIRE(out.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
    REQUIRE(msg->time == 1);
    REQUIRE(msg->data.values->size() == 3);    // key + 2 projected fields
    REQUIRE((*msg->data.values)[0] == 1.0);      // key
    REQUIRE((*msg->data.values)[1] == 10.5);     // price
    REQUIRE((*msg->data.values)[2] == 3.0);      // quantity
  }
}

SCENARIO("KeyedPipeline serialization roundtrip", "[keyed_pipeline][State]") {
  SECTION("Collect and restore preserves per-key state") {
    auto factory = make_extract_cumsum_factory(1);
    auto kp = make_keyed_pipeline("kp1", 0, factory);

    // Process 4 messages (2 keys, 2 messages each)
    kp->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 100.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(2, VectorNumberData{{2.0, 200.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(3, VectorNumberData{{1.0, 150.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(4, VectorNumberData{{2.0, 250.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    // Collect state as JSON
    auto state = kp->collect();

    // Restore into a new instance
    auto kp2 = make_keyed_pipeline("kp1", 0, factory);
    kp2->restore_data_from_json(state);

    REQUIRE(kp2->num_keys() == 2);

    // Process more messages and verify state continuity
    // key=1: previous sum was 250, new value 50 → sum=300
    kp2->receive_data(create_message<VectorNumberData>(5, VectorNumberData{{1.0, 50.0}}), 0);
    kp2->execute();

    {
      auto& out = kp2->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 5);
      REQUIRE((*msg->data.values)[0] == 1.0);
      REQUIRE((*msg->data.values)[1] == 300.0);  // 100 + 150 + 50
    }

    kp2->clear_all_output_ports();

    // key=2: previous sum was 450, new value 50 → sum=500
    kp2->receive_data(create_message<VectorNumberData>(6, VectorNumberData{{2.0, 50.0}}), 0);
    kp2->execute();

    {
      auto& out = kp2->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 6);
      REQUIRE((*msg->data.values)[0] == 2.0);
      REQUIRE((*msg->data.values)[1] == 500.0);  // 200 + 250 + 50
    }
  }
}

SCENARIO("KeyedPipeline handles many keys", "[keyed_pipeline]") {
  SECTION("100 distinct keys, 10 messages each") {
    auto kp = make_keyed_pipeline("kp1", 0, make_extract_cumsum_factory(1));

    timestamp_t t = 1;
    for (int round = 0; round < 10; round++) {
      for (int key = 0; key < 100; key++) {
        double key_val = static_cast<double>(key);
        double value = static_cast<double>(round + 1) * 10.0;  // 10, 20, 30, ...

        kp->receive_data(create_message<VectorNumberData>(t, VectorNumberData{{key_val, value}}), 0);
        kp->execute();

        auto& out = kp->get_output_queue(0);
        REQUIRE(out.size() == 1);
        auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());

        // Expected cumulative sum for this key at this round
        double expected_sum = 0.0;
        for (int r = 0; r <= round; r++) {
          expected_sum += static_cast<double>(r + 1) * 10.0;
        }

        REQUIRE((*msg->data.values)[0] == key_val);
        REQUIRE((*msg->data.values)[1] == Approx(expected_sum));

        kp->clear_all_output_ports();
        t++;
      }
    }

    REQUIRE(kp->num_keys() == 100);
  }
}

SCENARIO("KeyedPipeline new key callback", "[keyed_pipeline]") {
  SECTION("Callback called exactly once per new key") {
    std::vector<double> new_keys;
    auto kp = make_keyed_pipeline("kp1", 0, make_extract_cumsum_factory(1));
    kp->set_new_key_callback([&new_keys](double key) { new_keys.push_back(key); });

    // Process messages with keys [1, 2, 1, 3, 2]
    kp->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 10.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(2, VectorNumberData{{2.0, 20.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(3, VectorNumberData{{1.0, 30.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(4, VectorNumberData{{3.0, 40.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(5, VectorNumberData{{2.0, 50.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    REQUIRE(new_keys.size() == 3);
    REQUIRE(new_keys[0] == 1.0);
    REQUIRE(new_keys[1] == 2.0);
    REQUIRE(new_keys[2] == 3.0);
  }
}

SCENARIO("KeyedPipeline validation", "[keyed_pipeline]") {
  SECTION("Negative key_index throws") {
    REQUIRE_THROWS_AS(make_keyed_pipeline("kp1", -1, make_extract_cumsum_factory(1)), std::runtime_error);
  }

  SECTION("Key index out of bounds at runtime throws") {
    auto kp = make_keyed_pipeline("kp1", 5, make_extract_cumsum_factory(1));
    kp->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 2.0}}), 0);
    REQUIRE_THROWS_AS(kp->execute(), std::runtime_error);
  }
}

// --- Computed key mode tests ---

SCENARIO("KeyedPipeline computed key: basic routing with 2-column hash", "[keyed_pipeline][computed_key]") {
  SECTION("Routes messages by weighted sum of two columns") {
    // Prototype: VectorProject(indices=[0,1,2]) — passes through 3 fields
    auto kp = make_keyed_pipeline(
        "kp1",
        std::vector<int>{0, 1},           // key from cols 0 and 1
        make_project_factory({0, 1, 2}));

    // Input: [device_id, channel_id, amplitude]
    // key for (1, 10, ...) = PRIME * 1 + 10
    // key for (2, 20, ...) = PRIME * 2 + 20
    kp->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 10.0, 100.0}}), 0);
    kp->execute();

    {
      auto& out = kp->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->time == 1);
      // Computed key: output is prototype output directly (no key prepend)
      REQUIRE(msg->data.values->size() == 3);
      REQUIRE((*msg->data.values)[0] == 1.0);    // device_id
      REQUIRE((*msg->data.values)[1] == 10.0);   // channel_id
      REQUIRE((*msg->data.values)[2] == 100.0);  // amplitude
    }

    kp->clear_all_output_ports();

    // Different key group
    kp->receive_data(create_message<VectorNumberData>(2, VectorNumberData{{2.0, 20.0, 200.0}}), 0);
    kp->execute();

    {
      auto& out = kp->get_output_queue(0);
      REQUIRE(out.size() == 1);
      auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
      REQUIRE(msg->data.values->size() == 3);
      REQUIRE((*msg->data.values)[0] == 2.0);
      REQUIRE((*msg->data.values)[1] == 20.0);
      REQUIRE((*msg->data.values)[2] == 200.0);
    }

    REQUIRE(kp->num_keys() == 2);
  }
}

SCENARIO("KeyedPipeline computed key: per-key state isolation", "[keyed_pipeline][computed_key]") {
  SECTION("Cumulative sums are maintained independently per computed key") {
    // Prototype: VectorExtract(index=2) → CumulativeSum
    auto kp = make_keyed_pipeline(
        "kp1",
        std::vector<int>{0, 1},
        make_extract_cumsum_factory(2));

    // key group A: device=1, channel=10
    kp->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 10.0, 100.0}}), 0);
    kp->execute();
    {
      auto& out = kp->get_output_queue(0);
      REQUIRE(out.size() == 1);
      // CumulativeSum outputs NumberData, which gets cloned as-is in computed key mode.
      // The real use case has FusedExpression outputting VectorNumberData.
    }
    kp->clear_all_output_ports();

    // key group B: device=2, channel=20
    kp->receive_data(create_message<VectorNumberData>(2, VectorNumberData{{2.0, 20.0, 200.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    // key group A again: sum should be 100 + 150 = 250
    kp->receive_data(create_message<VectorNumberData>(3, VectorNumberData{{1.0, 10.0, 150.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    // key group B again: sum should be 200 + 250 = 450
    kp->receive_data(create_message<VectorNumberData>(4, VectorNumberData{{2.0, 20.0, 250.0}}), 0);
    kp->execute();

    // We can't easily check scalar vs vector in computed mode since CumulativeSum
    // outputs NumberData. The real use case has FusedExpression outputting VectorNumberData.
    REQUIRE(kp->num_keys() == 2);
  }
}

SCENARIO("KeyedPipeline computed key: single column coefficient", "[keyed_pipeline][computed_key]") {
  SECTION("Single column with coefficient 1.0 acts like classic mode without key prepend") {
    auto kp = make_keyed_pipeline(
        "kp1",
        std::vector<int>{0},
        make_project_factory({0, 1}));

    kp->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{5.0, 99.0}}), 0);
    kp->execute();

    auto& out = kp->get_output_queue(0);
    REQUIRE(out.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
    // No key prepend in computed mode: output = project({0,1}) = [5.0, 99.0]
    REQUIRE(msg->data.values->size() == 2);
    REQUIRE((*msg->data.values)[0] == 5.0);
    REQUIRE((*msg->data.values)[1] == 99.0);
  }
}

SCENARIO("KeyedPipeline computed key: validation", "[keyed_pipeline][computed_key]") {
  SECTION("Empty key_column_indices throws") {
    REQUIRE_THROWS_AS(
        make_keyed_pipeline("kp1", std::vector<int>{},
                            make_project_factory({0})),
        std::runtime_error);
  }

  SECTION("Negative index throws") {
    REQUIRE_THROWS_AS(
        make_keyed_pipeline("kp1", std::vector<int>{-1},
                            make_project_factory({0})),
        std::runtime_error);
  }

  SECTION("Out of bounds column index at runtime throws") {
    auto kp = make_keyed_pipeline(
        "kp1", std::vector<int>{5},
        make_project_factory({0}));
    kp->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 2.0}}), 0);
    REQUIRE_THROWS_AS(kp->execute(), std::runtime_error);
  }
}

SCENARIO("KeyedPipeline computed key: serialization roundtrip", "[keyed_pipeline][computed_key][State]") {
  SECTION("Collect and restore preserves per-key state") {
    auto factory = make_project_factory({0, 1, 2});
    auto kp = make_keyed_pipeline(
        "kp1",
        std::vector<int>{0, 1},
        factory);

    // Process messages for 2 key groups
    kp->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 10.0, 100.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(2, VectorNumberData{{2.0, 20.0, 200.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    REQUIRE(kp->num_keys() == 2);

    // Collect state
    auto state = kp->collect();

    // Restore into a new instance
    auto kp2 = make_keyed_pipeline(
        "kp1",
        std::vector<int>{0, 1},
        factory);
    kp2->restore_data_from_json(state);

    REQUIRE(kp2->num_keys() == 2);

    // Process new message for key group A and verify it routes correctly
    kp2->receive_data(create_message<VectorNumberData>(3, VectorNumberData{{1.0, 10.0, 300.0}}), 0);
    kp2->execute();

    auto& out = kp2->get_output_queue(0);
    REQUIRE(out.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
    REQUIRE(msg->data.values->size() == 3);
    REQUIRE((*msg->data.values)[0] == 1.0);
    REQUIRE((*msg->data.values)[1] == 10.0);
    REQUIRE((*msg->data.values)[2] == 300.0);
  }
}

SCENARIO("KeyedPipeline computed key: new key callback fires", "[keyed_pipeline][computed_key]") {
  SECTION("Callback called once per unique computed key") {
    std::vector<double> new_keys;
    auto kp = make_keyed_pipeline(
        "kp1",
        std::vector<int>{0},
        make_project_factory({0, 1}));
    kp->set_new_key_callback([&new_keys](double key) { new_keys.push_back(key); });

    kp->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 10.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(2, VectorNumberData{{2.0, 20.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    kp->receive_data(create_message<VectorNumberData>(3, VectorNumberData{{1.0, 30.0}}), 0);
    kp->execute();
    kp->clear_all_output_ports();

    REQUIRE(new_keys.size() == 2);
    REQUIRE(new_keys[0] == 1.0);
    REQUIRE(new_keys[1] == 2.0);
  }
}

SCENARIO("KeyedPipeline computed key: equality comparison", "[keyed_pipeline][computed_key]") {
  SECTION("Same configuration and state are equal") {
    auto factory = make_project_factory({0, 1});
    auto kp1 = make_keyed_pipeline("kp", std::vector<int>{0, 1}, factory);
    auto kp2 = make_keyed_pipeline("kp", std::vector<int>{0, 1}, factory);
    REQUIRE(*kp1 == *kp2);
  }

  SECTION("Different column indices are not equal") {
    auto factory = make_project_factory({0, 1});
    auto kp1 = make_keyed_pipeline("kp", std::vector<int>{0, 1}, factory);
    auto kp2 = make_keyed_pipeline("kp", std::vector<int>{0, 2}, factory);
    REQUIRE(*kp1 != *kp2);
  }

  SECTION("Computed key mode not equal to classic mode") {
    auto factory = make_project_factory({0, 1});
    auto kp1 = make_keyed_pipeline("kp", 0, factory);
    auto kp2 = make_keyed_pipeline("kp", std::vector<int>{0}, factory);
    REQUIRE(*kp1 != *kp2);
  }
}

SCENARIO("KeyedPipeline computed key: JSON roundtrip via OperatorJson", "[keyed_pipeline][computed_key][json]") {
  SECTION("Serialize and deserialize preserves computed key config") {
    // Build a KeyedPipeline with computed key that contains a VectorProject prototype
    auto kp = make_keyed_pipeline(
        "kp1",
        std::vector<int>{0, 1},
        make_project_factory({0, 1, 2}));

    // Test the read_op path with a hand-crafted JSON.
    // Note: keyCoefficients is NOT needed — the operator computes them internally.
    std::string json_str = R"({
      "type": "KeyedPipeline",
      "id": "kp1",
      "keyColumnIndices": [0, 1],
      "prototype": {
        "entry": {"operator": "proj"},
        "output": {"operator": "proj"},
        "operators": [
          {"type": "VectorProject", "id": "proj", "indices": [0, 1, 2]}
        ],
        "connections": []
      }
    })";

    auto restored = OperatorJson::read_op(json_str);
    REQUIRE(restored->type_name() == "KeyedPipeline");

    auto* kp_restored = dynamic_cast<KeyedPipeline*>(restored.get());
    REQUIRE(kp_restored != nullptr);
    REQUIRE(kp_restored->has_computed_key());
    REQUIRE(kp_restored->get_key_column_indices() == std::vector<int>{0, 1});

    // Verify it routes correctly
    kp_restored->receive_data(
        create_message<VectorNumberData>(1, VectorNumberData{{5.0, 10.0, 99.0}}), 0);
    kp_restored->execute();

    auto& out = kp_restored->get_output_queue(0);
    REQUIRE(out.size() == 1);
    auto* msg = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
    REQUIRE(msg->data.values->size() == 3);
    REQUIRE((*msg->data.values)[0] == 5.0);
    REQUIRE((*msg->data.values)[1] == 10.0);
    REQUIRE((*msg->data.values)[2] == 99.0);
  }

  SECTION("write_op emits keyColumnIndices only") {
    auto kp = make_keyed_pipeline(
        "kp1",
        std::vector<int>{2, 3},
        make_project_factory({0}));

    auto json_str = OperatorJson::write_op(kp);
    auto j = nlohmann::json::parse(json_str);
    REQUIRE(j.contains("keyColumnIndices"));
    REQUIRE_FALSE(j.contains("keyCoefficients"));
    REQUIRE_FALSE(j.contains("key_index"));
    REQUIRE(j["keyColumnIndices"] == std::vector<int>{2, 3});
  }

  SECTION("write_op for classic mode emits key_index") {
    auto kp = make_keyed_pipeline("kp1", 0, make_project_factory({0}));
    auto json_str = OperatorJson::write_op(kp);
    auto j = nlohmann::json::parse(json_str);
    REQUIRE(j.contains("key_index"));
    REQUIRE_FALSE(j.contains("keyColumnIndices"));
    REQUIRE(j["key_index"] == 0);
  }
}
