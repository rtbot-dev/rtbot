#include <catch2/catch.hpp>

#include "rtbot/std/FilterSync.h"

using namespace rtbot;

// ---------------------------------------------------------------------------
// SyncGreaterThan
// ---------------------------------------------------------------------------

SCENARIO("SyncGreaterThan passes messages when port0 > port1", "[filter_sync]") {
  GIVEN("A SyncGreaterThan operator") {
    auto op = make_sync_greater_than("sgt1");

    WHEN("port0 value is strictly greater than port1 value") {
      op->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      op->execute();

      THEN("The port0 value is forwarded") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == Approx(10.0));
      }
    }

    WHEN("port0 value is equal to port1 value") {
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }

    WHEN("port0 value is less than port1 value") {
      op->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{7.0}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }

    WHEN("Messages have different timestamps") {
      op->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      op->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }
  }
}

SCENARIO("SyncGreaterThan handles multiple synchronized pairs", "[filter_sync]") {
  GIVEN("A SyncGreaterThan operator") {
    auto op = make_sync_greater_than("sgt1");

    WHEN("A mix of passing and blocking pairs arrive") {
      // t=1: 10 > 5 → pass
      op->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      // t=2: 3 > 7 → block
      op->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
      op->receive_data(create_message<NumberData>(2, NumberData{7.0}), 1);
      // t=3: 9 > 1 → pass
      op->receive_data(create_message<NumberData>(3, NumberData{9.0}), 0);
      op->receive_data(create_message<NumberData>(3, NumberData{1.0}), 1);
      op->execute();

      THEN("Only the passing messages appear in the output") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 2);

        const auto* msg0 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg0->time == 1);
        REQUIRE(msg0->data.value == Approx(10.0));

        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg1->time == 3);
        REQUIRE(msg1->data.value == Approx(9.0));
      }
    }
  }
}

SCENARIO("SyncGreaterThan state serialization", "[filter_sync][state]") {
  GIVEN("A SyncGreaterThan operator with buffered data") {
    auto op = make_sync_greater_than("sgt1");

    op->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    op->execute();

    WHEN("State is serialized and restored") {
      Bytes state = op->collect();
      auto restored = make_sync_greater_than("sgt1");
      auto it = state.cbegin();
      restored->restore(it);

      THEN("The restored operator equals the original") { REQUIRE(*restored == *op); }
    }
  }
}

// ---------------------------------------------------------------------------
// SyncLessThan
// ---------------------------------------------------------------------------

SCENARIO("SyncLessThan passes messages when port0 < port1", "[filter_sync]") {
  GIVEN("A SyncLessThan operator") {
    auto op = make_sync_less_than("slt1");

    WHEN("port0 value is strictly less than port1 value") {
      op->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{7.0}), 1);
      op->execute();

      THEN("The port0 value is forwarded") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == Approx(3.0));
      }
    }

    WHEN("port0 value is equal to port1 value") {
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }

    WHEN("port0 value is greater than port1 value") {
      op->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }

    WHEN("Messages have different timestamps") {
      op->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
      op->receive_data(create_message<NumberData>(2, NumberData{7.0}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }
  }
}

SCENARIO("SyncLessThan state serialization", "[filter_sync][state]") {
  GIVEN("A SyncLessThan operator with buffered data") {
    auto op = make_sync_less_than("slt1");

    op->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
    op->receive_data(create_message<NumberData>(1, NumberData{7.0}), 1);
    op->execute();

    WHEN("State is serialized and restored") {
      Bytes state = op->collect();
      auto restored = make_sync_less_than("slt1");
      auto it = state.cbegin();
      restored->restore(it);

      THEN("The restored operator equals the original") { REQUIRE(*restored == *op); }
    }
  }
}

// ---------------------------------------------------------------------------
// SyncEqual
// ---------------------------------------------------------------------------

SCENARIO("SyncEqual passes messages when |port0 - port1| < epsilon", "[filter_sync]") {
  GIVEN("A SyncEqual operator with default epsilon") {
    auto op = make_sync_equal("seq1");

    WHEN("Values are identical") {
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      op->execute();

      THEN("The port0 value is forwarded") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg->data.value == Approx(5.0));
      }
    }

    WHEN("Values differ by less than epsilon") {
      double eps = 1e-10;
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0 + eps * 0.5}), 1);
      op->execute();

      THEN("The port0 value is forwarded") {
        REQUIRE(op->get_output_queue(0).size() == 1);
      }
    }

    WHEN("Values differ by more than epsilon") {
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{6.0}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }
  }

  GIVEN("A SyncEqual operator with custom epsilon of 0.5") {
    auto op = make_sync_equal("seq1", 2, 0.5);

    WHEN("Values differ by 0.4 (within epsilon)") {
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.4}), 1);
      op->execute();

      THEN("The port0 value is forwarded") {
        REQUIRE(op->get_output_queue(0).size() == 1);
      }
    }

    WHEN("Values differ by exactly epsilon (boundary, not strictly less)") {
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.5}), 1);
      op->execute();

      THEN("No output is produced (difference not strictly less than epsilon)") {
        REQUIRE(op->get_output_queue(0).empty());
      }
    }

    WHEN("Values differ by more than epsilon") {
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{6.0}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }
  }
}

SCENARIO("SyncEqual rejects non-positive epsilon", "[filter_sync]") {
  THEN("Constructing with epsilon=0 throws") {
    REQUIRE_THROWS_AS(make_sync_equal("seq1", 2, 0.0), std::runtime_error);
  }
  THEN("Constructing with negative epsilon throws") {
    REQUIRE_THROWS_AS(make_sync_equal("seq1", 2, -1.0), std::runtime_error);
  }
}

SCENARIO("SyncEqual state serialization", "[filter_sync][state]") {
  GIVEN("A SyncEqual operator with buffered data") {
    auto op = make_sync_equal("seq1", 2, 0.5);

    op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    op->receive_data(create_message<NumberData>(1, NumberData{5.2}), 1);
    op->execute();

    WHEN("State is serialized and restored") {
      Bytes state = op->collect();
      auto restored = make_sync_equal("seq1", 2, 0.5);
      auto it = state.cbegin();
      restored->restore(it);

      THEN("The restored operator equals the original") { REQUIRE(*restored == *op); }
    }
  }
}

// ---------------------------------------------------------------------------
// SyncNotEqual
// ---------------------------------------------------------------------------

SCENARIO("SyncNotEqual passes messages when |port0 - port1| >= epsilon", "[filter_sync]") {
  GIVEN("A SyncNotEqual operator with default epsilon") {
    auto op = make_sync_not_equal("sne1");

    WHEN("Values differ significantly") {
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{10.0}), 1);
      op->execute();

      THEN("The port0 value is forwarded") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg->data.value == Approx(5.0));
      }
    }

    WHEN("Values are identical") {
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }

    WHEN("Values differ by less than epsilon") {
      double eps = 1e-10;
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0 + eps * 0.5}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }
  }

  GIVEN("A SyncNotEqual operator with custom epsilon of 0.5") {
    auto op = make_sync_not_equal("sne1", 2, 0.5);

    WHEN("Values differ by exactly epsilon (boundary — passes)") {
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.5}), 1);
      op->execute();

      THEN("The port0 value is forwarded") {
        REQUIRE(op->get_output_queue(0).size() == 1);
      }
    }

    WHEN("Values differ by less than epsilon") {
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.4}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }
  }
}

SCENARIO("SyncNotEqual rejects non-positive epsilon", "[filter_sync]") {
  THEN("Constructing with epsilon=0 throws") {
    REQUIRE_THROWS_AS(make_sync_not_equal("sne1", 2, 0.0), std::runtime_error);
  }
  THEN("Constructing with negative epsilon throws") {
    REQUIRE_THROWS_AS(make_sync_not_equal("sne1", 2, -1.0), std::runtime_error);
  }
}

SCENARIO("SyncNotEqual state serialization", "[filter_sync][state]") {
  GIVEN("A SyncNotEqual operator with buffered data") {
    auto op = make_sync_not_equal("sne1");

    op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    op->receive_data(create_message<NumberData>(1, NumberData{10.0}), 1);
    op->execute();

    WHEN("State is serialized and restored") {
      Bytes state = op->collect();
      auto restored = make_sync_not_equal("sne1");
      auto it = state.cbegin();
      restored->restore(it);

      THEN("The restored operator equals the original") { REQUIRE(*restored == *op); }
    }
  }
}

// ---------------------------------------------------------------------------
// Shared behaviour: 3-port FilterSync
// ---------------------------------------------------------------------------

SCENARIO("FilterSync operators work with more than 2 ports", "[filter_sync]") {
  GIVEN("A SyncGreaterThan operator with 3 ports") {
    auto op = make_sync_greater_than("sgt1", 3);

    WHEN("port0 > port1 and port0 > port2") {
      op->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      op->receive_data(create_message<NumberData>(1, NumberData{3.0}), 2);
      op->execute();

      THEN("The port0 value is forwarded") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg->data.value == Approx(10.0));
      }
    }

    WHEN("port0 > port1 but port0 <= port2") {
      op->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      op->receive_data(create_message<NumberData>(1, NumberData{15.0}), 2);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }

    WHEN("Only 2 of 3 ports have messages") {
      op->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      op->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      op->execute();

      THEN("No output is produced") { REQUIRE(op->get_output_queue(0).empty()); }
    }
  }
}
