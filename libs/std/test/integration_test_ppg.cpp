#include <catch2/catch.hpp>
#include <memory>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/std/ArithmeticSync.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "tools.h"

using namespace rtbot;

class PPGPipeline {
 public:
  PPGPipeline(double dt, int short_window, int long_window) {
    input = std::make_shared<Input>("i1", std::vector<std::string>{PortType::NUMBER});
    ma_short = std::make_shared<MovingAverage>("ma1", short_window);
    ma_long = std::make_shared<MovingAverage>("ma2", long_window);
    minus = std::make_shared<Subtraction>("diff");
    peak = std::make_shared<PeakDetector>("peak", 2 * short_window + 1);
    join = std::make_shared<Join>("join", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER});
    output = std::make_shared<Output>("o1", std::vector<std::string>{PortType::NUMBER});
    col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});

    // Build pipeline
    input->connect(ma_short)->connect(minus, 0, 0)->connect(peak)->connect(join, 0, 0)->connect(output);
    input->connect(ma_long)->connect(minus, 0, 1);
    input->connect(join, 0, 1);
    output->connect(col, 0, 0);
  }

  void reset_sink() { col->reset(); }

  std::shared_ptr<Input> input;
  std::shared_ptr<MovingAverage> ma_short;
  std::shared_ptr<MovingAverage> ma_long;
  std::shared_ptr<Subtraction> minus;
  std::shared_ptr<PeakDetector> peak;
  std::shared_ptr<Join> join;
  std::shared_ptr<Output> output;
  std::shared_ptr<Collector> col;
};

SCENARIO("PPG Pipeline propagates messages correctly", "[PPG][Integration]") {
  auto s = SamplePPG("examples/data/ppg.csv");
  const double dt = s.dt();
  const int short_window = static_cast<int>(std::round(50 / dt));
  const int long_window = static_cast<int>(std::round(2000 / dt));

  GIVEN("A configured PPG processing pipeline") {
    PPGPipeline pipeline(dt, short_window, long_window);

    WHEN("Processing a single data point") {
      pipeline.input->receive_data(create_message<NumberData>(s.ti[0], NumberData{s.ppg[0]}), 0);
      pipeline.input->execute(true);

      THEN("Messages propagate through moving averages") {
        REQUIRE(pipeline.ma_short->get_debug_output_queue(0).size() <= 1);
        REQUIRE(pipeline.ma_long->get_debug_output_queue(0).size() <= 1);
      }

      AND_THEN("Moving average buffers start filling") {
        REQUIRE(pipeline.ma_short->buffer_size() == 1);
        REQUIRE(pipeline.ma_long->buffer_size() == 1);
      }
    }

    WHEN("Processing enough points to fill short window") {
      for (int i = 1; i < short_window + 1; i++) {
        pipeline.input->receive_data(create_message<NumberData>(s.ti[i], NumberData{s.ppg[i]}), 0);
        pipeline.input->execute(true);
      }

      THEN("Short moving average starts producing output") {
        REQUIRE(pipeline.ma_short->buffer_full());
        REQUIRE(pipeline.ma_short->get_debug_output_queue(0).size() == 1);
      }

      AND_THEN("Long moving average still filling") {
        REQUIRE_FALSE(pipeline.ma_long->buffer_full());
        REQUIRE(pipeline.ma_long->buffer_size() == short_window);
      }
    }

    WHEN("The terminal sink is reset") {
      // Fill pipeline partially
      for (int i = 0; i < short_window; i++) {
        pipeline.input->receive_data(create_message<NumberData>(s.ti[i], NumberData{s.ppg[i]}), 0);
        pipeline.input->execute();
      }

      pipeline.reset_sink();

      THEN("Buffers retain data after clearing terminal output") {
        REQUIRE(pipeline.col->get_data_queue(0).empty());
        REQUIRE(pipeline.ma_short->buffer_size() == short_window);
        REQUIRE(pipeline.ma_long->buffer_size() == short_window);
      }
    }

    WHEN("Processing enough data for peak detection") {
      std::vector<timestamp_t> peak_times;
      const int test_window = 300;

      size_t last_seen = 0;
      for (int i = 0; i < test_window; i++) {
        pipeline.input->receive_data(create_message<NumberData>(s.ti[i], NumberData{s.ppg[i]}), 0);
        pipeline.input->execute(true);

        const auto& peak_output = pipeline.peak->get_debug_output_queue(0);
        for (size_t k = last_seen; k < peak_output.size(); k++) {
          peak_times.push_back(peak_output[k]->time);
        }
        last_seen = peak_output.size();

        pipeline.reset_sink();
      }

      THEN("Peaks are detected in expected time ranges") {
        REQUIRE(!peak_times.empty());
        // Verify minimum time between peaks
        if (peak_times.size() >= 2) {
          for (size_t i = 1; i < peak_times.size(); i++) {
            REQUIRE(peak_times[i] - peak_times[i - 1] >= short_window);
          }
        }
      }
    }
  }
}

SCENARIO("PPG Pipeline handles state serialization", "[PPG][Integration]") {
  auto s = SamplePPG("examples/data/ppg.csv");
  const double dt = s.dt();
  const int short_window = static_cast<int>(std::round(50 / dt));
  const int long_window = static_cast<int>(std::round(2000 / dt));

  GIVEN("A PPG pipeline with processed data") {
    PPGPipeline pipeline(dt, short_window, long_window);

    // Process initial batch of data
    for (int i = 0; i < short_window * 2; i++) {
      pipeline.input->receive_data(create_message<NumberData>(s.ti[i], NumberData{s.ppg[i]}), 0);
      pipeline.input->execute();
    }

    WHEN("Pipeline state is serialized and restored") {
      // Serialize each component
      auto ma_short_state = pipeline.ma_short->collect();
      auto ma_long_state = pipeline.ma_long->collect();
      auto peak_state = pipeline.peak->collect();

      // Create new pipeline
      PPGPipeline restored(dt, short_window, long_window);

      // Restore state
      restored.ma_short->restore_data_from_json(ma_short_state);
      restored.ma_long->restore_data_from_json(ma_long_state);
      restored.peak->restore_data_from_json(peak_state);

      THEN("Moving average buffers are preserved") {
        REQUIRE(restored.ma_short->buffer_size() == pipeline.ma_short->buffer_size());
        REQUIRE(restored.ma_long->buffer_size() == pipeline.ma_long->buffer_size());
      }

      AND_WHEN("Processing new data point") {
        auto test_time = s.ti[short_window * 2];
        auto test_value = s.ppg[short_window * 2];

        pipeline.input->receive_data(create_message<NumberData>(test_time, NumberData{test_value}), 0);
        restored.input->receive_data(create_message<NumberData>(test_time, NumberData{test_value}), 0);

        pipeline.input->execute();
        restored.input->execute();

        THEN("Both pipelines produce identical output") {
          const auto& orig_output = pipeline.col->get_data_queue(0);
          const auto& rest_output = restored.col->get_data_queue(0);

          REQUIRE(orig_output.size() == rest_output.size());
          if (!orig_output.empty()) {
            const auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output[0].get());
            const auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output[0].get());
            REQUIRE(orig_msg->time == rest_msg->time);
            REQUIRE(orig_msg->data.value == rest_msg->data.value);
          }
        }
      }
    }
  }
}