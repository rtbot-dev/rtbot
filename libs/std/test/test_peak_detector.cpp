#include <catch2/catch.hpp>
#include <iostream>
#include <memory>

#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/std/ArithmeticSync.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "rtbot/telemetry/OpenTelemetryInit.h"
#include "tools.h"

using namespace rtbot;

SCENARIO("PeakDetector handles basic peak detection", "[PeakDetector]") {
  GIVEN("A PeakDetector with window size 3") {
    auto detector = std::make_unique<PeakDetector>("test", 3);

    WHEN("Processing a simple peak") {
      detector->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      detector->execute();
      detector->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      detector->execute();
      detector->receive_data(create_message<NumberData>(3, NumberData{1.0}), 0);
      detector->execute();

      THEN("Peak is detected") {
        const auto& output = detector->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 2);
        REQUIRE(msg->data.value == 2.0);
      }
    }

    WHEN("Processing a non-peak") {
      detector->receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      detector->execute();
      detector->receive_data(create_message<NumberData>(2, NumberData{1.0}), 0);
      detector->execute();
      detector->receive_data(create_message<NumberData>(3, NumberData{2.0}), 0);
      detector->execute();

      THEN("No peak is detected") {
        const auto& output = detector->get_output_queue(0);
        REQUIRE(output.empty());
      }
    }
  }
}

SCENARIO("PeakDetector handles edge cases", "[PeakDetector]") {
  SECTION("Invalid window sizes") {
    REQUIRE_THROWS_AS(PeakDetector("test", 2), std::runtime_error);  // Even size
    REQUIRE_THROWS_AS(PeakDetector("test", 1), std::runtime_error);  // Too small
  }

  GIVEN("A PeakDetector with window size 5") {
    auto detector = std::make_unique<PeakDetector>("test", 5);

    WHEN("Processing a plateau") {
      detector->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      detector->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      detector->receive_data(create_message<NumberData>(3, NumberData{2.0}), 0);
      detector->receive_data(create_message<NumberData>(4, NumberData{2.0}), 0);
      detector->receive_data(create_message<NumberData>(5, NumberData{1.0}), 0);
      detector->execute();

      THEN("No peak is detected") {
        const auto& output = detector->get_output_queue(0);
        REQUIRE(output.empty());
      }
    }
  }
}

SCENARIO("PeakDetector handles state serialization", "[PeakDetector]") {
  GIVEN("A PeakDetector with processed data") {
    auto detector = std::make_unique<PeakDetector>("test", 3);

    // Fill buffer with a peak pattern
    detector->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    detector->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    detector->receive_data(create_message<NumberData>(3, NumberData{1.0}), 0);
    detector->execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = detector->collect();

      // Create new detector and restore state
      auto restored = std::make_unique<PeakDetector>("test", 3);
      auto it = state.cbegin();
      restored->restore(it);

      THEN("Buffer state is preserved") {
        const auto& orig_buf = detector->buffer();
        const auto& rest_buf = restored->buffer();

        REQUIRE(orig_buf.size() == rest_buf.size());
        for (size_t i = 0; i < orig_buf.size(); ++i) {
          REQUIRE(orig_buf[i]->time == rest_buf[i]->time);
          REQUIRE(orig_buf[i]->data.value == rest_buf[i]->data.value);
        }
      }

      AND_WHEN("New data is processed") {
        detector->receive_data(create_message<NumberData>(4, NumberData{0.5}), 0);
        restored->receive_data(create_message<NumberData>(4, NumberData{0.5}), 0);

        detector->execute();
        restored->execute();

        THEN("Both instances produce identical output") {
          const auto& orig_out = detector->get_output_queue(0);
          const auto& rest_out = restored->get_output_queue(0);

          REQUIRE(orig_out.size() == rest_out.size());
          if (!orig_out.empty()) {
            const auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_out[0].get());
            const auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_out[0].get());
            REQUIRE(orig_msg->time == rest_msg->time);
            REQUIRE(orig_msg->data.value == rest_msg->data.value);
          }
        }
      }
    }
  }
}

auto s = SamplePPG("examples/data/ppg.csv");

SCENARIO("PeakDetector works in a PPG analysis pipeline", "[PeakDetector][Integration]") {
#ifdef RTBOT_INSTRUMENTATION
  init_telemetry();
#endif
  auto input = std::make_unique<Input>("i1", std::vector<std::string>{PortType::NUMBER});

  std::cout << "Loaded -> " << s.ti.size() << " samples from PPG data, dt " << s.dt() << std::endl;

  const double dt = s.dt();
  const int short_window = static_cast<int>(std::round(50 / dt));
  const int long_window = static_cast<int>(std::round(2000 / dt));

  auto ma_short = std::make_shared<MovingAverage>("ma1", short_window);
  auto ma_long = std::make_shared<MovingAverage>("ma2", long_window);
  auto minus = std::make_shared<Subtraction>("diff");
  auto peak = std::make_shared<PeakDetector>("peak", 2 * short_window + 1);
  auto join = std::make_shared<Join>("join", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER});
  auto output = std::make_shared<Output>("o1", std::vector<std::string>{PortType::NUMBER});

  // Connect pipeline:
  // input -> ma_short -> minus -> peak -> join -> output
  //      \-> ma_long /                   /
  //      \----------------------------->/

  input->connect(ma_short)->connect(minus, 0, 0)->connect(peak)->connect(join, 0, 0)->connect(output);
  input->connect(ma_long)->connect(minus, 0, 1);
  input->connect(join, 0, 1);

  // create a vector to store the peak timestamps
  std::vector<double> peak_timestamps;
  for (size_t i = 0; i < s.ti.size(); i++) {
    input->receive_data(create_message<NumberData>(s.ti[i], NumberData{s.ppg[i]}), 0);
    // std::cout << "Processing PPG data at " << s.ti[i] << std::endl;
    input->execute();
    const auto& output_queue = join->get_output_queue(0);

    for (const auto& msg : output_queue) {
      const auto* data = dynamic_cast<const Message<NumberData>*>(msg.get());
      // std::cout << "\nPeak detected at " << data->time << std::endl;
      peak_timestamps.push_back(data->time);
    }
    // clear all output queues
    input->clear_all_output_ports();
    ma_short->clear_all_output_ports();
    ma_long->clear_all_output_ports();
    minus->clear_all_output_ports();
    peak->clear_all_output_ports();
    join->clear_all_output_ports();
    output->clear_all_output_ports();
  }
  GIVEN("A configured PPG processing pipeline") { REQUIRE(peak_timestamps.size() == 95); }
}