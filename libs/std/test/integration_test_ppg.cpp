#include <catch2/catch.hpp>
#include <memory>
#include <vector>

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

    // Build pipeline
    input->connect(ma_short)->connect(minus, 0, 0)->connect(peak)->connect(join, 0, 0)->connect(output);
    input->connect(ma_long)->connect(minus, 0, 1);
    input->connect(join, 0, 1);
  }

  void clear_all_queues() {
    input->clear_all_output_ports();
    ma_short->clear_all_output_ports();
    ma_long->clear_all_output_ports();
    minus->clear_all_output_ports();
    peak->clear_all_output_ports();
    join->clear_all_output_ports();
    output->clear_all_output_ports();
  }

  std::shared_ptr<Input> input;
  std::shared_ptr<MovingAverage> ma_short;
  std::shared_ptr<MovingAverage> ma_long;
  std::shared_ptr<Subtraction> minus;
  std::shared_ptr<PeakDetector> peak;
  std::shared_ptr<Join> join;
  std::shared_ptr<Output> output;
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

    WHEN("Output queues are cleared") {
      // Fill pipeline partially
      for (int i = 0; i < short_window; i++) {
        pipeline.input->receive_data(create_message<NumberData>(s.ti[i], NumberData{s.ppg[i]}), 0);
        pipeline.input->execute();
      }

      pipeline.clear_all_queues();

      THEN("Output queues are empty but buffers retain data") {
        REQUIRE(pipeline.ma_short->get_output_queue(0).empty());
        REQUIRE(pipeline.ma_long->get_output_queue(0).empty());
        REQUIRE(pipeline.ma_short->buffer_size() == short_window);
        REQUIRE(pipeline.ma_long->buffer_size() == short_window);
      }
    }

    WHEN("Processing enough data for peak detection") {
      std::vector<timestamp_t> peak_times;
      const int test_window = 300;

      for (int i = 0; i < test_window; i++) {
        pipeline.input->receive_data(create_message<NumberData>(s.ti[i], NumberData{s.ppg[i]}), 0);
        pipeline.input->execute(true);

        const auto& peak_output = pipeline.peak->get_debug_output_queue(0);
        for (const auto& msg : peak_output) {
          peak_times.push_back(msg->time);
        }

        pipeline.clear_all_queues();
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
      Bytes ma_short_state = pipeline.ma_short->collect();
      Bytes ma_long_state = pipeline.ma_long->collect();
      Bytes peak_state = pipeline.peak->collect();

      // Create new pipeline
      PPGPipeline restored(dt, short_window, long_window);

      // Restore state
      auto it_short = ma_short_state.cbegin();
      auto it_long = ma_long_state.cbegin();
      auto it_peak = peak_state.cbegin();

      restored.ma_short->restore(it_short);
      restored.ma_long->restore(it_long);
      restored.peak->restore(it_peak);

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
          const auto& orig_output = pipeline.output->get_output_queue(0);
          const auto& rest_output = restored.output->get_output_queue(0);

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