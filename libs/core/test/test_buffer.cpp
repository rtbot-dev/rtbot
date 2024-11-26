#include <catch2/catch.hpp>
#include <cmath>

#include "rtbot/Buffer.h"

using namespace rtbot;

// Custom Feature sets for testing different configurations
struct NoFeatures {
  static constexpr bool TRACK_SUM = false;
  static constexpr bool TRACK_MEAN = false;
  static constexpr bool TRACK_VARIANCE = false;
};

struct SumOnly {
  static constexpr bool TRACK_SUM = true;
  static constexpr bool TRACK_MEAN = false;
  static constexpr bool TRACK_VARIANCE = false;
};

struct MeanOnly {
  static constexpr bool TRACK_SUM = false;
  static constexpr bool TRACK_MEAN = true;
  static constexpr bool TRACK_VARIANCE = false;
};

struct FullStats {
  static constexpr bool TRACK_SUM = true;
  static constexpr bool TRACK_MEAN = true;
  static constexpr bool TRACK_VARIANCE = true;
};

// Test implementation of Buffer
template <typename Features = BufferFeatures>
class TestBuffer : public Buffer<NumberData, Features> {
 public:
  TestBuffer(std::string id, size_t window_size) : Buffer<NumberData, Features>(id, window_size) {}

  std::string type_name() const override { return "TestBuffer"; }

 protected:
  bool process_message(const Message<NumberData>* msg) override {
    return true;  // Always process messages for testing
  }
};

SCENARIO("Buffer operator handles basic operations", "[Buffer]") {
  GIVEN("A Buffer with window size 3") {
    auto buffer = TestBuffer<NoFeatures>("test", 3);

    WHEN("Messages are added within window size") {
      buffer.receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      buffer.execute();
      buffer.receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      buffer.execute();

      THEN("Buffer size matches number of messages") {
        REQUIRE(buffer.buffer_size() == 2);
        REQUIRE_FALSE(buffer.buffer_full());
      }

      AND_WHEN("Buffer becomes full") {
        buffer.receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
        buffer.execute();

        THEN("Buffer is reported as full") {
          REQUIRE(buffer.buffer_size() == 3);
          REQUIRE(buffer.buffer_full());
        }
      }
    }

    WHEN("More messages than window size are added") {
      buffer.receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      buffer.execute();
      buffer.receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      buffer.execute();
      buffer.receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      buffer.execute();
      buffer.receive_data(create_message<NumberData>(4, NumberData{4.0}), 0);
      buffer.execute();

      THEN("Oldest message is removed") {
        REQUIRE(buffer.buffer_size() == 3);
        REQUIRE(buffer.buffer().front().value == 2.0);
        REQUIRE(buffer.buffer().back().value == 4.0);
      }
    }
  }
}

SCENARIO("Buffer operator calculates running sum correctly", "[Buffer][Statistics]") {
  GIVEN("A Buffer with sum tracking enabled") {
    auto buffer = TestBuffer<SumOnly>("test", 3);

    WHEN("Messages are added sequentially") {
      buffer.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      buffer.execute();
      buffer.receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
      buffer.execute();

      THEN("Sum is calculated correctly") { REQUIRE(buffer.sum() == 5.0); }

      AND_WHEN("Buffer overflows") {
        buffer.receive_data(create_message<NumberData>(3, NumberData{4.0}), 0);
        buffer.execute();
        buffer.receive_data(create_message<NumberData>(4, NumberData{5.0}), 0);
        buffer.execute();

        THEN("Sum is updated correctly") {
          REQUIRE(buffer.sum() == 12.0);  // 3.0 + 4.0 + 5.0
        }
      }
    }
  }
}

SCENARIO("Buffer operator calculates running mean correctly", "[Buffer][Statistics]") {
  GIVEN("A Buffer with mean tracking enabled") {
    auto buffer = TestBuffer<MeanOnly>("test", 3);

    WHEN("Messages are added sequentially") {
      buffer.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      buffer.execute();
      buffer.receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
      buffer.execute();

      THEN("Mean is calculated correctly") { REQUIRE(buffer.mean() == 3.0); }

      AND_WHEN("Buffer overflows") {
        buffer.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);
        buffer.execute();
        buffer.receive_data(create_message<NumberData>(4, NumberData{8.0}), 0);
        buffer.execute();

        THEN("Mean is updated correctly") {
          REQUIRE(buffer.mean() == 6.0);  // (4.0 + 6.0 + 8.0) / 3
        }
      }
    }
  }
}

SCENARIO("Buffer operator calculates variance and standard deviation correctly", "[Buffer][Statistics]") {
  GIVEN("A Buffer with full statistical tracking enabled") {
    auto buffer = TestBuffer<FullStats>("test", 3);

    WHEN("Messages with known variance are added") {
      buffer.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      buffer.execute();
      buffer.receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
      buffer.execute();
      buffer.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);
      buffer.execute();

      THEN("Variance and standard deviation are calculated correctly") {
        REQUIRE(buffer.variance() == 4.0);
        REQUIRE(buffer.standard_deviation() == 2.0);
      }
    }
  }
}

SCENARIO("Buffer operator handles state serialization and restoration", "[Buffer][State]") {
  GIVEN("A Buffer with data and full statistics") {
    auto buffer = TestBuffer<FullStats>("test", 3);
    buffer.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
    buffer.execute();
    buffer.receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
    buffer.execute();
    buffer.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);
    buffer.execute();

    WHEN("State is serialized and restored to a new buffer") {
      Bytes state = buffer.collect();
      auto restored_buffer = TestBuffer<FullStats>("test", 3);
      Bytes::const_iterator it = state.begin();
      restored_buffer.restore(it);

      THEN("All statistics match the original buffer") {
        REQUIRE(restored_buffer.buffer_size() == buffer.buffer_size());
        REQUIRE(restored_buffer.sum() == buffer.sum());
        REQUIRE(restored_buffer.mean() == buffer.mean());
        REQUIRE(restored_buffer.variance() == buffer.variance());

        AND_THEN("Buffer contents match") {
          REQUIRE(restored_buffer.buffer().front().value == buffer.buffer().front().value);
          REQUIRE(restored_buffer.buffer().back().value == buffer.buffer().back().value);
        }
      }
    }
  }
}

SCENARIO("Buffer operator handles edge cases", "[Buffer][EdgeCases]") {
  GIVEN("A Buffer configuration") {
    WHEN("Created with invalid window size") {
      THEN("Constructor throws exception") { REQUIRE_THROWS_AS(TestBuffer<NoFeatures>("test", 0), std::runtime_error); }
    }

    WHEN("Created with window size 1") {
      auto buffer = TestBuffer<FullStats>("test", 1);

      THEN("Statistics are calculated correctly for single value") {
        buffer.receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
        buffer.execute();

        REQUIRE(buffer.sum() == 5.0);
        REQUIRE(buffer.mean() == 5.0);
        REQUIRE(buffer.variance() == 0.0);
      }
    }
  }

  GIVEN("A Buffer with large values") {
    auto buffer = TestBuffer<FullStats>("test", 3);

    WHEN("Very large numbers are added") {
      double large_value = 1e15;
      buffer.receive_data(create_message<NumberData>(1, NumberData{large_value}), 0);
      buffer.execute();
      buffer.receive_data(create_message<NumberData>(2, NumberData{large_value + 1}), 0);
      buffer.execute();
      buffer.receive_data(create_message<NumberData>(3, NumberData{large_value + 2}), 0);
      buffer.execute();

      THEN("Statistics remain numerically stable") {
        REQUIRE(std::abs(buffer.mean() - (large_value + 1)) < 1e-10);
        REQUIRE(buffer.variance() == 1.0);
      }
    }
  }
}