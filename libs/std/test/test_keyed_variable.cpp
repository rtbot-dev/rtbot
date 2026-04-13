#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/Demultiplexer.h"
#include "rtbot/std/KeyedVariable.h"
#include "rtbot/std/VectorExtract.h"

using namespace rtbot;

// ---------------------------------------------------------------------------
// Exists mode
// ---------------------------------------------------------------------------
SCENARIO("KeyedVariable exists mode: basic add/query/delete", "[keyed_variable]") {
  GIVEN("A KeyedVariable in exists mode") {
    auto kv = make_keyed_variable("kv1", "exists");

    WHEN("Keys are added and queried") {
      // Add key 42 at t=10
      kv->receive_data(create_message<VectorNumberData>(10, VectorNumberData{{42.0, 1.0}}), 0);
      kv->execute();

      // Add key 17 at t=20
      kv->receive_data(create_message<VectorNumberData>(20, VectorNumberData{{17.0, 1.0}}), 0);
      kv->execute();
      kv->clear_all_output_ports();

      // Query key 42 — use c2 heartbeat at t=30 to advance timeline
      kv->receive_control(create_message<NumberData>(30, NumberData{42.0}), 0);  // c1: query 42
      kv->receive_control(create_message<NumberData>(30, NumberData{0.0}), 1);   // c2: heartbeat
      kv->execute();

      THEN("Key 42 exists → true") {
        const auto& out = kv->get_output_queue(0);
        REQUIRE(out.size() == 1);
        const auto* msg = dynamic_cast<const Message<BooleanData>*>(out[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 30);
        REQUIRE(msg->data.value == true);
      }
    }

    WHEN("A non-existent key is queried") {
      kv->receive_data(create_message<VectorNumberData>(10, VectorNumberData{{42.0, 1.0}}), 0);
      kv->execute();
      kv->clear_all_output_ports();

      kv->receive_control(create_message<NumberData>(20, NumberData{99.0}), 0);  // c1: query 99
      kv->receive_control(create_message<NumberData>(20, NumberData{0.0}), 1);   // c2: heartbeat
      kv->execute();

      THEN("Key 99 does not exist → false") {
        const auto& out = kv->get_output_queue(0);
        REQUIRE(out.size() == 1);
        const auto* msg = dynamic_cast<const Message<BooleanData>*>(out[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->data.value == false);
      }
    }

    WHEN("A key is deleted with NaN and then queried") {
      // Add key 42
      kv->receive_data(create_message<VectorNumberData>(10, VectorNumberData{{42.0, 1.0}}), 0);
      kv->execute();

      // Delete key 42 with NaN
      kv->receive_data(create_message<VectorNumberData>(20, VectorNumberData{{42.0, std::numeric_limits<double>::quiet_NaN()}}), 0);
      kv->execute();
      kv->clear_all_output_ports();

      // Query key 42 after deletion
      kv->receive_control(create_message<NumberData>(30, NumberData{42.0}), 0);  // c1
      kv->receive_control(create_message<NumberData>(30, NumberData{0.0}), 1);   // c2
      kv->execute();

      THEN("Key 42 was deleted → false") {
        const auto& out = kv->get_output_queue(0);
        REQUIRE(out.size() == 1);
        const auto* msg = dynamic_cast<const Message<BooleanData>*>(out[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->data.value == false);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Lookup mode
// ---------------------------------------------------------------------------
SCENARIO("KeyedVariable lookup mode: add/query/update", "[keyed_variable]") {
  GIVEN("A KeyedVariable in lookup mode with default 0.0") {
    auto kv = make_keyed_variable("kv2", "lookup", 0.0);

    WHEN("Keys are added and queried") {
      kv->receive_data(create_message<VectorNumberData>(10, VectorNumberData{{42.0, 100.0}}), 0);
      kv->execute();
      kv->receive_data(create_message<VectorNumberData>(20, VectorNumberData{{17.0, 200.0}}), 0);
      kv->execute();
      kv->clear_all_output_ports();

      // Query key 42
      kv->receive_control(create_message<NumberData>(30, NumberData{42.0}), 0);
      kv->receive_control(create_message<NumberData>(30, NumberData{0.0}), 1);
      kv->execute();

      THEN("Returns stored value 100.0 for key 42") {
        const auto& out = kv->get_output_queue(0);
        REQUIRE(out.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(out[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 30);
        REQUIRE(msg->data.value == 100.0);
      }
    }

    WHEN("A missing key is queried") {
      kv->receive_data(create_message<VectorNumberData>(10, VectorNumberData{{42.0, 100.0}}), 0);
      kv->execute();
      kv->clear_all_output_ports();

      kv->receive_control(create_message<NumberData>(20, NumberData{99.0}), 0);
      kv->receive_control(create_message<NumberData>(20, NumberData{0.0}), 1);
      kv->execute();

      THEN("Returns default value 0.0 for missing key 99") {
        const auto& out = kv->get_output_queue(0);
        REQUIRE(out.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(out[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->data.value == 0.0);
      }
    }

    WHEN("A key value is updated") {
      kv->receive_data(create_message<VectorNumberData>(10, VectorNumberData{{42.0, 100.0}}), 0);
      kv->execute();
      kv->receive_data(create_message<VectorNumberData>(20, VectorNumberData{{42.0, 150.0}}), 0);
      kv->execute();
      kv->clear_all_output_ports();

      kv->receive_control(create_message<NumberData>(30, NumberData{42.0}), 0);
      kv->receive_control(create_message<NumberData>(30, NumberData{0.0}), 1);
      kv->execute();

      THEN("Returns updated value 150.0") {
        const auto& out = kv->get_output_queue(0);
        REQUIRE(out.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(out[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->data.value == 150.0);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Heartbeat: timeline advances without i1 updates
// ---------------------------------------------------------------------------
SCENARIO("KeyedVariable heartbeat advances timeline", "[keyed_variable][heartbeat]") {
  GIVEN("A KeyedVariable with watchlist data at t=0, trade queries at t=1,2,3") {
    auto kv = make_keyed_variable("kv3", "exists");

    // t=0: watchlist adds key 42
    kv->receive_data(create_message<VectorNumberData>(0, VectorNumberData{{42.0, 1.0}}), 0);
    kv->execute();
    kv->clear_all_output_ports();

    WHEN("Trades arrive with heartbeat at t=1,2,3 (no more i1 data)") {
      // t=1: heartbeat + query key=42 → true
      kv->receive_control(create_message<NumberData>(1, NumberData{42.0}), 0);  // c1
      kv->receive_control(create_message<NumberData>(1, NumberData{0.0}), 1);   // c2
      kv->execute();
      auto& out1 = kv->get_output_queue(0);
      REQUIRE(out1.size() == 1);
      const auto* msg1 = dynamic_cast<const Message<BooleanData>*>(out1[0].get());
      REQUIRE(msg1->time == 1);
      REQUIRE(msg1->data.value == true);
      kv->clear_all_output_ports();

      // t=2: heartbeat + query key=99 → false
      kv->receive_control(create_message<NumberData>(2, NumberData{99.0}), 0);
      kv->receive_control(create_message<NumberData>(2, NumberData{0.0}), 1);
      kv->execute();
      auto& out2 = kv->get_output_queue(0);
      REQUIRE(out2.size() == 1);
      const auto* msg2 = dynamic_cast<const Message<BooleanData>*>(out2[0].get());
      REQUIRE(msg2->time == 2);
      REQUIRE(msg2->data.value == false);
      kv->clear_all_output_ports();

      // t=3: heartbeat + query key=42 → still true (not changed)
      kv->receive_control(create_message<NumberData>(3, NumberData{42.0}), 0);
      kv->receive_control(create_message<NumberData>(3, NumberData{0.0}), 1);
      kv->execute();
      auto& out3 = kv->get_output_queue(0);
      REQUIRE(out3.size() == 1);
      const auto* msg3 = dynamic_cast<const Message<BooleanData>*>(out3[0].get());
      REQUIRE(msg3->time == 3);
      REQUIRE(msg3->data.value == true);

      THEN("All three queries resolved correctly without new i1 data") {
        // Verified inline above
        REQUIRE(true);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Processing order: query sees data update at the same timestamp
// ---------------------------------------------------------------------------
SCENARIO("KeyedVariable processing order: c1 sees i1 update at same timestamp",
         "[keyed_variable]") {
  GIVEN("An empty KeyedVariable in exists mode") {
    auto kv = make_keyed_variable("kv4", "exists");

    WHEN("i1 and c1 both arrive at t=5 (no prior state)") {
      // Queue i1 update [42→1.0] and c1 query 42 at the same timestamp
      kv->receive_data(create_message<VectorNumberData>(5, VectorNumberData{{42.0, 1.0}}), 0);
      kv->receive_control(create_message<NumberData>(5, NumberData{42.0}), 0);

      // c1 is buffered, then i1 updates hashmap, then c1 is resolved
      kv->execute();

      THEN("Query sees the update: key 42 exists → true") {
        const auto& out = kv->get_output_queue(0);
        REQUIRE(out.size() == 1);
        const auto* msg = dynamic_cast<const Message<BooleanData>*>(out[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 5);
        REQUIRE(msg->data.value == true);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Query stalls without timeline advancement
// ---------------------------------------------------------------------------
SCENARIO("KeyedVariable query stalls when timeline not advanced", "[keyed_variable]") {
  GIVEN("A KeyedVariable with i1 data only at t=10") {
    auto kv = make_keyed_variable("kv5", "exists");

    kv->receive_data(create_message<VectorNumberData>(10, VectorNumberData{{42.0, 1.0}}), 0);
    kv->execute();
    kv->clear_all_output_ports();

    WHEN("A c1 query arrives at t=50 without any heartbeat or further i1") {
      kv->receive_control(create_message<NumberData>(50, NumberData{42.0}), 0);
      kv->execute();

      THEN("Query stalls — no output produced") {
        const auto& out = kv->get_output_queue(0);
        REQUIRE(out.empty());
      }
    }

    WHEN("A c2 heartbeat arrives later at t=55, unlocking the pending query") {
      kv->receive_control(create_message<NumberData>(50, NumberData{42.0}), 0);
      kv->execute();
      kv->clear_all_output_ports();

      kv->receive_control(create_message<NumberData>(55, NumberData{0.0}), 1);  // c2
      kv->execute();

      THEN("Query at t=50 is resolved with current state") {
        const auto& out = kv->get_output_queue(0);
        REQUIRE(out.size() == 1);
        const auto* msg = dynamic_cast<const Message<BooleanData>*>(out[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 50);
        REQUIRE(msg->data.value == true);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Serialization roundtrip
// ---------------------------------------------------------------------------
SCENARIO("KeyedVariable serialization roundtrip", "[keyed_variable][State]") {
  GIVEN("A KeyedVariable with several keys and pending queries") {
    auto kv = make_keyed_variable("kv6", "lookup", 0.0);

    kv->receive_data(create_message<VectorNumberData>(10, VectorNumberData{{42.0, 100.0}}), 0);
    kv->execute();
    kv->receive_data(create_message<VectorNumberData>(20, VectorNumberData{{17.0, 200.0}}), 0);
    kv->execute();
    kv->receive_data(create_message<VectorNumberData>(30, VectorNumberData{{8.0, 300.0}}), 0);
    kv->execute();
    kv->clear_all_output_ports();

    WHEN("State is serialized and restored") {
      auto state = kv->collect();

      auto restored = make_keyed_variable("kv6", "lookup", 0.0);
      restored->restore_data_from_json(state);

      // Query all three keys on the restored operator
      restored->receive_control(create_message<NumberData>(40, NumberData{42.0}), 0);
      restored->receive_control(create_message<NumberData>(40, NumberData{0.0}), 1);
      restored->execute();
      auto& out1 = restored->get_output_queue(0);
      REQUIRE(out1.size() == 1);
      const auto* m1 = dynamic_cast<const Message<NumberData>*>(out1[0].get());
      REQUIRE(m1->data.value == 100.0);
      restored->clear_all_output_ports();

      restored->receive_control(create_message<NumberData>(50, NumberData{17.0}), 0);
      restored->receive_control(create_message<NumberData>(50, NumberData{0.0}), 1);
      restored->execute();
      auto& out2 = restored->get_output_queue(0);
      REQUIRE(out2.size() == 1);
      const auto* m2 = dynamic_cast<const Message<NumberData>*>(out2[0].get());
      REQUIRE(m2->data.value == 200.0);
      restored->clear_all_output_ports();

      restored->receive_control(create_message<NumberData>(60, NumberData{8.0}), 0);
      restored->receive_control(create_message<NumberData>(60, NumberData{0.0}), 1);
      restored->execute();
      auto& out3 = restored->get_output_queue(0);
      REQUIRE(out3.size() == 1);
      const auto* m3 = dynamic_cast<const Message<NumberData>*>(out3[0].get());

      THEN("All keys are restored with correct values") {
        REQUIRE(m3->data.value == 300.0);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Full JOIN pattern: stream-TABLE join via KeyedVariable + Demultiplexer
// ---------------------------------------------------------------------------
SCENARIO("KeyedVariable JOIN pattern: trades filtered by watchlist", "[keyed_variable]") {
  // Pipeline:
  //   watchlist_source → kv.i1
  //   trade_source     → ve.i1 → kv.c1 (query account_id)
  //                    → ve.i1 → kv.c2 (heartbeat, same output as c1)
  //   kv.o1            → dmux.c1 (exists gate)
  //   trade_source     → dmux.i1 (full trade tuple)
  //   dmux.o1          → output (passing trades)
  GIVEN("A connected KeyedVariable + VectorExtract + Demultiplexer pipeline") {
    auto kv = make_keyed_variable("kv", "exists");
    auto ve = std::make_shared<VectorExtract>("ve", 0);  // index 0 = account_id
    auto dmux = make_demultiplexer_vector_number("dmux", 1);

    // ve.o1 → kv.c1 (query)
    ve->connect(kv, 0, 0, PortKind::CONTROL);
    // ve.o1 → kv.c2 (heartbeat) — same NumberData, different port
    ve->connect(kv, 0, 1, PortKind::CONTROL);
    // kv.o1 → dmux.c1 (boolean gate)
    kv->connect(dmux, 0, 0, PortKind::CONTROL);

    // t=0: watchlist adds account 42
    kv->receive_data(create_message<VectorNumberData>(0, VectorNumberData{{42.0, 1.0}}), 0);
    kv->execute();

    // t=1: watchlist adds account 17
    kv->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{17.0, 1.0}}), 0);
    kv->execute();
    kv->clear_all_output_ports();

    WHEN("Trades for accounts 42, 99, 17, 8 are processed in sequence") {
      // t=2: trade [42, 500] — account 42 is on watchlist → should pass
      dmux->receive_data(create_message<VectorNumberData>(2, VectorNumberData{{42.0, 500.0}}), 0);
      ve->receive_data(create_message<VectorNumberData>(2, VectorNumberData{{42.0, 500.0}}), 0);
      ve->execute();  // propagates → kv → dmux.c1 → dmux processes

      auto& out2 = dmux->get_output_queue(0);
      REQUIRE(out2.size() == 1);
      const auto* t2 = dynamic_cast<const Message<VectorNumberData>*>(out2[0].get());
      REQUIRE(t2->time == 2);
      REQUIRE((*t2->data.values)[0] == 42.0);
      dmux->clear_all_output_ports();

      // t=3: trade [99, 200] — account 99 not on watchlist → should drop
      dmux->receive_data(create_message<VectorNumberData>(3, VectorNumberData{{99.0, 200.0}}), 0);
      ve->receive_data(create_message<VectorNumberData>(3, VectorNumberData{{99.0, 200.0}}), 0);
      ve->execute();

      REQUIRE(dmux->get_output_queue(0).empty());
      dmux->clear_all_output_ports();

      // t=4: trade [17, 300] — account 17 is on watchlist → should pass
      dmux->receive_data(create_message<VectorNumberData>(4, VectorNumberData{{17.0, 300.0}}), 0);
      ve->receive_data(create_message<VectorNumberData>(4, VectorNumberData{{17.0, 300.0}}), 0);
      ve->execute();

      auto& out4 = dmux->get_output_queue(0);
      REQUIRE(out4.size() == 1);
      const auto* t4 = dynamic_cast<const Message<VectorNumberData>*>(out4[0].get());
      REQUIRE(t4->time == 4);
      REQUIRE((*t4->data.values)[0] == 17.0);
      dmux->clear_all_output_ports();

      // t=5: trade [8, 400] — account 8 not on watchlist → should drop
      dmux->receive_data(create_message<VectorNumberData>(5, VectorNumberData{{8.0, 400.0}}), 0);
      ve->receive_data(create_message<VectorNumberData>(5, VectorNumberData{{8.0, 400.0}}), 0);
      ve->execute();

      THEN("Only trades for accounts 42 and 17 pass through") {
        REQUIRE(dmux->get_output_queue(0).empty());
      }
    }
  }
}
