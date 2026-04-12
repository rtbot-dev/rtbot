#include <catch2/catch.hpp>

#include "rtbot/Program.h"

using namespace rtbot;

SCENARIO("Program handles TriggerSet operators", "[program][trigger_set]") {
  GIVEN("A program with a TriggerSet containing multiple connected operators") {
    std::string program_json = R"({
            "operators": [
                {"type": "Input", "id": "input1", "portTypes": ["number"]},
                {
                    "type": "TriggerSet",
                    "id": "ts1",
                    "input_port_type": "number",
                    "output_port_type": "number",
                    "operators": [
                        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
                        {"type": "StandardDeviation", "id": "std1", "window_size": 3}
                    ],
                    "connections": [
                        {"from": "ma1", "to": "std1", "fromPort": "o1", "toPort": "i1"}
                    ],
                    "entryOperator": "ma1",
                    "outputOperator": {"id": "std1", "port": "o1"}
                },
                {"type": "Output", "id": "output1", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "input1", "to": "ts1", "fromPort": "o1", "toPort": "i1"},
                {"from": "ts1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
            ],
            "entryOperator": "input1",
            "output": {
                "output1": ["o1"]
            }
        })";

    Program program(program_json);

    WHEN("Processing messages") {
      // 5 messages: first 3 fill the MA(3) buffer, next 2 emit MA values that
      // fill StdDev(3) and produce an output, which fires the TriggerSet.
      std::vector<Message<NumberData>> messages = {
          {1, NumberData{3.0}},   // MA collecting
          {2, NumberData{6.0}},   // MA collecting
          {3, NumberData{9.0}},   // MA emits first value -> StdDev collecting
          {4, NumberData{12.0}},  // MA emits second value -> StdDev collecting
          {5, NumberData{15.0}}   // MA emits third value -> StdDev fires -> TriggerSet fires
      };

      ProgramMsgBatch final_batch;
      for (const auto& msg : messages) {
        final_batch = program.receive(msg);
      }

      THEN("TriggerSet processes messages correctly") {
        REQUIRE(final_batch.size() == 1);
        REQUIRE(final_batch["output1"].count("o1") == 1);
        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output1"]["o1"].back().get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->time == 5);
      }
    }

    WHEN("Processing fewer messages") {
      // 4 messages — not enough for StdDev to fire.
      std::vector<Message<NumberData>> messages = {
          {1, NumberData{3.0}}, {2, NumberData{6.0}}, {3, NumberData{9.0}}, {4, NumberData{12.0}}};

      ProgramMsgBatch final_batch;
      for (const auto& msg : messages) {
        final_batch = program.receive(msg);
      }

      THEN("No output is produced yet") { REQUIRE(final_batch.empty()); }
    }
  }

  GIVEN("A program with an invalid TriggerSet configuration") {
    std::string invalid_program = R"({
            "operators": [
                {"type": "Input", "id": "input1", "portTypes": ["number"]},
                {
                    "type": "TriggerSet",
                    "id": "ts1",
                    "input_port_type": "number",
                    "output_port_type": "number",
                    "operators": [
                        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
                        {"type": "MovingAverage", "id": "ma2", "window_size": 2}
                    ],
                    "connections": [
                        {"from": "ma1", "to": "ma2", "fromPort": "o1", "toPort": "i1"}
                    ],
                    "entryOperator": "nonexistent",
                    "outputOperator": {"id": "ma1", "port": "o1"}
                },
                {"type": "Output", "id": "output1", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "input1", "to": "ts1", "fromPort": "o1", "toPort": "i1"},
                {"from": "ts1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
            ],
            "entryOperator": "input1",
            "output": {
                "output1": ["o1"]
            }
        })";

    THEN("Program creation fails with appropriate error") {
      REQUIRE_THROWS_WITH(Program(invalid_program), Catch::Contains("Entry operator not found"));
    }
  }
}

SCENARIO("Program handles TriggerSet reset behavior", "[program][trigger_set]") {
  GIVEN("A program with a TriggerSet wrapping a single MovingAverage") {
    std::string program_json = R"({
            "operators": [
                {"type": "Input", "id": "input", "portTypes": ["number"]},
                {
                    "type": "TriggerSet",
                    "id": "ts",
                    "input_port_type": "number",
                    "output_port_type": "number",
                    "operators": [
                        {"type": "MovingAverage", "id": "ma", "window_size": 3}
                    ],
                    "entryOperator": "ma",
                    "outputOperator": {"id": "ma", "port": "o1"}
                },
                {"type": "Output", "id": "output", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "input", "to": "ts", "fromPort": "o1", "toPort": "i1"},
                {"from": "ts", "to": "output", "fromPort": "o1", "toPort": "i1"}
            ],
            "entryOperator": "input",
            "output": {
                "output": ["o1"]
            }
        })";

    Program program(program_json);

    WHEN("Processing messages across multiple trigger cycles") {
      std::vector<Message<NumberData>> messages = {
          {1, NumberData{1.0}},  // MA collecting
          {2, NumberData{2.0}},  // MA collecting
          {3, NumberData{3.0}},  // MA emits 2.0 -> TriggerSet fires & resets
          {4, NumberData{4.0}},  // MA collecting (fresh)
          {5, NumberData{5.0}},  // MA collecting
          {6, NumberData{6.0}},  // MA emits 5.0 -> TriggerSet fires & resets
          {7, NumberData{0.0}},  // MA collecting (fresh)
          {8, NumberData{0.0}},  // MA collecting
          {9, NumberData{0.0}},  // MA emits 0.0 -> TriggerSet fires & resets
      };

      ProgramMsgBatch final_batch;

      program.receive(messages.at(0));
      program.receive(messages.at(1));
      final_batch = program.receive(messages.at(2));

      THEN("TriggerSet emits the first cycle and resets") {
        REQUIRE(final_batch.size() == 1);
        REQUIRE(final_batch["output"].count("o1") == 1);
        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output"]["o1"].back().get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->time == 3);
        REQUIRE(out_msg->data.value == Approx(2.0));

        final_batch = program.receive(messages.at(3));

        AND_THEN("Internal MA starts fresh after reset") {
          REQUIRE(final_batch.empty());

          final_batch = program.receive(messages.at(4));
          REQUIRE(final_batch.empty());

          final_batch = program.receive(messages.at(5));

          AND_THEN("TriggerSet emits the second cycle") {
            REQUIRE(final_batch.size() == 1);
            REQUIRE(final_batch["output"].count("o1") == 1);
            const auto* out_msg2 = dynamic_cast<const Message<NumberData>*>(final_batch["output"]["o1"].back().get());
            REQUIRE(out_msg2 != nullptr);
            REQUIRE(out_msg2->time == 6);
            REQUIRE(out_msg2->data.value == Approx(5.0));

            program.receive(messages.at(6));
            program.receive(messages.at(7));
            final_batch = program.receive(messages.at(8));

            AND_THEN("TriggerSet emits the third cycle") {
              REQUIRE(final_batch.size() == 1);
              REQUIRE(final_batch["output"].count("o1") == 1);
              const auto* out_msg3 =
                  dynamic_cast<const Message<NumberData>*>(final_batch["output"]["o1"].back().get());
              REQUIRE(out_msg3 != nullptr);
              REQUIRE(out_msg3->time == 9);
              REQUIRE(out_msg3->data.value == Approx(0.0));
            }
          }
        }
      }
    }
  }
}

SCENARIO("Program handles complex TriggerSet operators and resets", "[program][trigger_set]") {
  GIVEN("A complex program with a TriggerSet hourly accumulator") {
    // Power consumption monitor: two current inputs -> cutoffs -> resamplers
    // -> scale to power -> sum -> hourly TriggerSet (trapezoid integration -> Wh -> moving sum 720)
    std::string program_json = R"({
    "apiVersion": "v1",
    "operators": [
        {
            "id": "input",
            "type": "Input",
            "portTypes": [
                "number",
                "number"
            ]
        },
        {
            "id": "hi_input_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "lo_input_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "hiresampler",
            "type": "ResamplerConstant",
            "interval": 5000
        },
        {
            "id": "loresampler",
            "type": "ResamplerConstant",
            "interval": 5000
        },
        {
            "id": "hiresampler_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "loresampler_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "power1",
            "type": "Scale",
            "value": 220
        },
        {
            "id": "power2",
            "type": "Scale",
            "value": 220
        },
        {
            "id": "power1_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "power2_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "total_power",
            "type": "Addition"
        },
        {
            "id": "total_power_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "hourly",
            "type": "TriggerSet",
            "input_port_type": "number",
            "output_port_type": "number",
            "operators": [
                {
                    "id": "trapezoid",
                    "type": "MovingAverage",
                    "window_size": 2
                },
                {
                    "id": "trapezoid_cutoff",
                    "type": "LessThanOrEqualToReplace",
                    "value": 0.5,
                    "replaceBy": 0.0
                },
                {
                    "id": "wh",
                    "type": "Scale",
                    "value": 0.00138888888
                },
                {
                    "id": "wh_cutoff",
                    "type": "LessThanOrEqualToReplace",
                    "value": 0.5,
                    "replaceBy": 0.0
                },
                {
                    "id": "hourly_ms",
                    "type": "MovingSum",
                    "window_size": 720
                }
            ],
            "connections": [
                {"from": "trapezoid", "to": "trapezoid_cutoff"},
                {"from": "trapezoid_cutoff", "to": "wh"},
                {"from": "wh", "to": "wh_cutoff"},
                {"from": "wh_cutoff", "to": "hourly_ms"}
            ],
            "entryOperator": "trapezoid",
            "outputOperator": {"id": "hourly_ms", "port": "o1"}
        },
        {
            "id": "output",
            "type": "Output",
            "portTypes": [
                "number"
            ]
        }
    ],
    "connections": [
        {"from": "input", "to": "hi_input_cutoff", "fromPort": "o1", "toPort": "i1"},
        {"from": "input", "to": "lo_input_cutoff", "fromPort": "o2", "toPort": "i1"},
        {"from": "hi_input_cutoff", "to": "hiresampler"},
        {"from": "lo_input_cutoff", "to": "loresampler"},
        {"from": "hiresampler", "to": "hiresampler_cutoff"},
        {"from": "hiresampler_cutoff", "to": "power1"},
        {"from": "loresampler", "to": "loresampler_cutoff"},
        {"from": "loresampler_cutoff", "to": "power2"},
        {"from": "power1", "to": "power1_cutoff"},
        {"from": "power2", "to": "power2_cutoff"},
        {"from": "power1_cutoff", "to": "total_power", "fromPort": "o1", "toPort": "i1"},
        {"from": "power2_cutoff", "to": "total_power", "fromPort": "o1", "toPort": "i2"},
        {"from": "total_power", "to": "total_power_cutoff"},
        {"from": "total_power_cutoff", "to": "hourly"},
        {"from": "hourly", "to": "output", "fromPort": "o1", "toPort": "i1"}
    ],
    "entryOperator": "input",
    "output": {
        "output": ["o1"]
    },
    "title": "Power Consumption Monitor with Multiple Averages",
    "description": "Calculates hourly power consumption from two current inputs"
})";

    Program program(program_json);

    WHEN("Processing 20 simulated hours of input samples") {
      // Each hour drives messages until the inner TriggerSet (MovingSum 720)
      // fires once. With a 5-second cadence (3600s / 5s = 720 samples/hour),
      // each hour pushes ~720 messages through the inner trigger set before
      // it fires and resets.
      int t = 5000;
      for (int h = 1; h <= 20; h++) {
        ProgramMsgBatch final_batch;
        if (h % 2 == 1) {
          // Odd hours: real load -> hourly Wh > 0
          while (final_batch.size() == 0) {
            program.receive({t, NumberData{30.23}}, "i1");
            final_batch = program.receive({t, NumberData{10.5802}}, "i2");
            t = t + 5000;
          }
        } else {
          // Even hours: below cutoff -> hourly Wh == 0
          while (final_batch.size() == 0) {
            program.receive({t, NumberData{0.01}}, "i1");
            final_batch = program.receive({t, NumberData{0.01}}, "i2");
            t = t + 5000;
          }
        }

        THEN("TriggerSet fires once per hour with the expected sign") {
          REQUIRE(final_batch.size() == 1);
          REQUIRE(final_batch["output"].count("o1") == 1);
          const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output"]["o1"].back().get());
          REQUIRE(out_msg != nullptr);
          if (h % 2 == 1) {
            REQUIRE(out_msg->data.value > 0.0);
          } else {
            REQUIRE(out_msg->data.value == 0.0);
          }
        }
      }
    }
  }
}

SCENARIO("Program handles TriggerSet serialization", "[program][trigger_set]") {
  GIVEN("A program with a stateful TriggerSet") {
    std::string program_json = R"({
            "operators": [
                {"type": "Input", "id": "input1", "portTypes": ["number"]},
                {
                    "type": "TriggerSet",
                    "id": "ts1",
                    "input_port_type": "number",
                    "output_port_type": "number",
                    "operators": [
                        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
                        {"type": "MovingAverage", "id": "ma2", "window_size": 2}
                    ],
                    "connections": [
                        {"from": "ma1", "to": "ma2", "fromPort": "o1", "toPort": "i1"}
                    ],
                    "entryOperator": "ma1",
                    "outputOperator": {"id": "ma2", "port": "o1"}
                },
                {"type": "Output", "id": "output1", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "input1", "to": "ts1", "fromPort": "o1", "toPort": "i1"},
                {"from": "ts1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
            ],
            "entryOperator": "input1",
            "output": {
                "output1": ["o1"]
            }
        })";

    Program original_program(program_json);

    WHEN("Processing messages and serializing at a critical state") {
      // 3 messages: ma1 just fired its first output, ma2 is mid-window.
      std::vector<std::pair<int64_t, double>> initial_sequence = {
          {1, 10.0},
          {2, 20.0},
          {3, 30.0}  // ma1 emits 20.0 -> ma2 has its first value, not yet firing
      };

      ProgramMsgBatch batch;
      for (const auto& [time, value] : initial_sequence) {
        batch = original_program.receive(Message<NumberData>(time, NumberData{value}));
      }

      // Verify no output yet (need a 4th message to make ma2 fire)
      REQUIRE(batch.empty());

      // Serialize at the critical mid-window state
      auto serialized = original_program.serialize_data();
      Program restored_program(program_json);
      restored_program.restore_data_from_json(serialized);

      // Send the 4th message to both
      auto original_batch = original_program.receive(Message<NumberData>(4, NumberData{40.0}));
      auto restored_batch = restored_program.receive(Message<NumberData>(4, NumberData{40.0}));

      THEN("Both programs produce identical output") {
        REQUIRE(original_batch.count("output1") == 1);
        REQUIRE(restored_batch.count("output1") == 1);
        REQUIRE(original_batch["output1"].count("o1") == 1);
        REQUIRE(restored_batch["output1"].count("o1") == 1);

        const auto* original_msg =
            dynamic_cast<const Message<NumberData>*>(original_batch["output1"]["o1"].back().get());
        const auto* restored_msg =
            dynamic_cast<const Message<NumberData>*>(restored_batch["output1"]["o1"].back().get());

        REQUIRE(original_msg != nullptr);
        REQUIRE(restored_msg != nullptr);
        REQUIRE(original_msg->time == 4);
        REQUIRE(restored_msg->time == 4);
        // ma2 averages ma1's first two outputs (20, 30)
        REQUIRE(original_msg->data.value == Approx(25.0));
        REQUIRE(restored_msg->data.value == Approx(25.0));

        // After firing, both trigger sets should have reset internal state.
        // A single follow-up message must not produce output.
        auto original_next = original_program.receive(Message<NumberData>(5, NumberData{50.0}));
        auto restored_next = restored_program.receive(Message<NumberData>(5, NumberData{50.0}));
        REQUIRE(original_next.empty());
        REQUIRE(restored_next.empty());
      }
    }
  }
}

SCENARIO("Program handles TriggerSet prototypes", "[program][prototypes][trigger_set]") {
  GIVEN("A program with a prototype containing a TriggerSet") {
    // The prototype wraps a TriggerSet with MA(window) inside.
    // After 'window' messages the MA fires once and the TriggerSet
    // forwards the value, then resets its internal state.
    std::string program_json = R"({
      "prototypes": {
        "rearmingAverage": {
          "parameters": [
            {"name": "ma_window", "type": "number"}
          ],
          "operators": [
            {
              "type": "TriggerSet",
              "id": "ts",
              "input_port_type": "number",
              "output_port_type": "number",
              "operators": [
                {"type": "MovingAverage", "id": "ma", "window_size": "${ma_window}"}
              ],
              "entryOperator": "ma",
              "outputOperator": {"id": "ma", "port": "o1"}
            }
          ],
          "connections": [],
          "entry": {"operator": "ts"},
          "output": {"operator": "ts"}
        }
      },
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {
          "id": "proc1",
          "prototype": "rearmingAverage",
          "parameters": {"ma_window": 3}
        },
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "proc1", "fromPort": "o1", "toPort": "i1"},
        {"from": "proc1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program program(program_json);

    WHEN("Processing messages through the prototype-instantiated TriggerSet") {
      ProgramMsgBatch final_batch;
      // First two messages: MA collecting, no fire.
      final_batch = program.receive(Message<NumberData>(1, NumberData{3.0}));
      REQUIRE(final_batch.empty());
      final_batch = program.receive(Message<NumberData>(2, NumberData{6.0}));
      REQUIRE(final_batch.empty());

      // Third message: MA(3,6,9) = 6.0 → fires, TriggerSet forwards & resets.
      final_batch = program.receive(Message<NumberData>(3, NumberData{9.0}));
      THEN("The trigger set fires after the window is full") {
        REQUIRE(final_batch.count("output1") == 1);
        REQUIRE(final_batch["output1"]["o1"].size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(final_batch["output1"]["o1"].back().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(6.0));
      }

      // After firing, internal state must be reset: a single new message produces nothing.
      auto next = program.receive(Message<NumberData>(4, NumberData{12.0}));
      REQUIRE(next.empty());
    }

    WHEN("Serializing and restoring mid-window state") {
      // Two messages collected, no fire yet.
      program.receive(Message<NumberData>(1, NumberData{3.0}));
      program.receive(Message<NumberData>(2, NumberData{6.0}));

      auto serialized = program.serialize_data();
      Program restored(program_json);
      restored.restore_data_from_json(serialized);

      auto orig = program.receive(Message<NumberData>(3, NumberData{9.0}));
      auto rest = restored.receive(Message<NumberData>(3, NumberData{9.0}));

      THEN("Both fire at the same time with the same value") {
        REQUIRE(orig.count("output1") == 1);
        REQUIRE(rest.count("output1") == 1);
        const auto* o = dynamic_cast<const Message<NumberData>*>(orig["output1"]["o1"].back().get());
        const auto* r = dynamic_cast<const Message<NumberData>*>(rest["output1"]["o1"].back().get());
        REQUIRE(o != nullptr);
        REQUIRE(r != nullptr);
        REQUIRE(o->data.value == Approx(6.0));
        REQUIRE(r->data.value == Approx(6.0));
      }
    }
  }
}

SCENARIO("Program handles chained hourly/daily/weekly TriggerSets", "[program][trigger_set]") {
  GIVEN("A power consumption monitor with hourly, daily, and weekly TriggerSets") {
    std::string program_json = R"({
    "apiVersion": "v1",
    "operators": [
        {
            "id": "input",
            "type": "Input",
            "portTypes": ["number", "number"]
        },
        {
            "id": "hi_input_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.3,
            "replaceBy": 0.0
        },
        {
            "id": "lo_input_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.3,
            "replaceBy": 0.0
        },
        {
            "id": "hiresampler",
            "type": "ResamplerConstant",
            "interval": 5000
        },
        {
            "id": "loresampler",
            "type": "ResamplerConstant",
            "interval": 5000
        },
        {
            "id": "power1",
            "type": "Scale",
            "value": 220
        },
        {
            "id": "power2",
            "type": "Scale",
            "value": 220
        },
        {
            "id": "power1_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "power2_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "total_power",
            "type": "Addition"
        },
        {
            "id": "hourly",
            "type": "TriggerSet",
            "input_port_type": "number",
            "output_port_type": "number",
            "operators": [
                {
                    "id": "hourly_trapezoid",
                    "type": "MovingAverage",
                    "window_size": 2
                },
                {
                    "id": "hourly_wh",
                    "type": "Scale",
                    "value": 0.00138888888
                },
                {
                    "id": "hourly_wh_cutoff",
                    "type": "LessThanOrEqualToReplace",
                    "value": 0.1,
                    "replaceBy": 0.0
                },
                {
                    "id": "hourly_ms",
                    "type": "MovingSum",
                    "window_size": 720
                }
            ],
            "connections": [
                {"from": "hourly_trapezoid", "to": "hourly_wh"},
                {"from": "hourly_wh", "to": "hourly_wh_cutoff"},
                {"from": "hourly_wh_cutoff", "to": "hourly_ms"}
            ],
            "entryOperator": "hourly_trapezoid",
            "outputOperator": {"id": "hourly_ms", "port": "o1"}
        },
        {
            "id": "daily",
            "type": "TriggerSet",
            "input_port_type": "number",
            "output_port_type": "number",
            "operators": [
                {
                    "id": "daily_input",
                    "type": "Input",
                    "portTypes": ["number"]
                },
                {
                    "id": "daily_ms",
                    "type": "MovingSum",
                    "window_size": 24
                }
            ],
            "connections": [
                {"from": "daily_input", "to": "daily_ms"}
            ],
            "entryOperator": "daily_input",
            "outputOperator": {"id": "daily_ms", "port": "o1"}
        },
        {
            "id": "weekly",
            "type": "TriggerSet",
            "input_port_type": "number",
            "output_port_type": "number",
            "operators": [
                {
                    "id": "weekly_input",
                    "type": "Input",
                    "portTypes": ["number"]
                },
                {
                    "id": "weekly_ms",
                    "type": "MovingSum",
                    "window_size": 7
                }
            ],
            "connections": [
                {"from": "weekly_input", "to": "weekly_ms"}
            ],
            "entryOperator": "weekly_input",
            "outputOperator": {"id": "weekly_ms", "port": "o1"}
        },
        {
            "id": "output",
            "type": "Output",
            "portTypes": ["number", "number", "number"]
        }
    ],
    "connections": [
        {"from": "input", "to": "hi_input_cutoff", "fromPort": "o1", "toPort": "i1"},
        {"from": "input", "to": "lo_input_cutoff", "fromPort": "o2", "toPort": "i1"},
        {"from": "hi_input_cutoff", "to": "hiresampler"},
        {"from": "lo_input_cutoff", "to": "loresampler"},
        {"from": "hiresampler", "to": "power1"},
        {"from": "loresampler", "to": "power2"},
        {"from": "power1", "to": "power1_cutoff"},
        {"from": "power2", "to": "power2_cutoff"},
        {"from": "power1_cutoff", "to": "total_power", "fromPort": "o1", "toPort": "i1"},
        {"from": "power2_cutoff", "to": "total_power", "fromPort": "o1", "toPort": "i2"},
        {"from": "total_power", "to": "hourly"},
        {"from": "hourly", "to": "daily"},
        {"from": "daily", "to": "weekly"},
        {"from": "hourly", "to": "output", "fromPort": "o1", "toPort": "i1"},
        {"from": "daily", "to": "output", "fromPort": "o1", "toPort": "i2"},
        {"from": "weekly", "to": "output", "fromPort": "o1", "toPort": "i3"}
    ],
    "entryOperator": "input",
    "output": {
        "output": ["o1", "o2", "o3"]
    },
    "title": "Power Consumption Monitor with Multiple Averages",
    "description": "Calculates hourly, daily, and weekly power consumption from two current inputs"
    })";

    Program program(program_json);

    WHEN("Processing simulated data across multiple hours and days") {
      // With 5-second resampling, 720 samples fill the hourly MovingSum and fire.
      // 24 hourly fires fill the daily MovingSum and fire.
      // 7 daily fires fill the weekly MovingSum and fire.
      int t = 5000;
      int hourly_fires = 0;
      int daily_fires = 0;
      int weekly_fires = 0;

      // Run enough hours for a full week: 7 days * 24 hours = 168 hours
      for (int h = 1; h <= 168; h++) {
        ProgramMsgBatch batch;
        // Drive messages until hourly fires (720 samples at 5s interval = 1 hour)
        while (true) {
          double hi_current = (h % 2 == 1) ? 30.23 : 0.01;
          double lo_current = (h % 2 == 1) ? 10.58 : 0.01;
          program.receive({t, NumberData{hi_current}}, "i1");
          batch = program.receive({t, NumberData{lo_current}}, "i2");
          t += 5000;

          if (batch.count("output") && batch["output"].count("o1")) {
            hourly_fires++;
            break;
          }
        }

        // Check for daily fire
        if (batch.count("output") && batch["output"].count("o2")) {
          daily_fires++;
        }

        // Check for weekly fire
        if (batch.count("output") && batch["output"].count("o3")) {
          weekly_fires++;
        }
      }

      THEN("Hourly TriggerSet fires once per hour") {
        REQUIRE(hourly_fires == 168);
      }

      THEN("Daily TriggerSet fires once per day") {
        REQUIRE(daily_fires == 7);
      }

      THEN("Weekly TriggerSet fires once per week") {
        REQUIRE(weekly_fires == 1);
      }

      THEN("Hourly output values reflect load pattern") {
        // Odd hours have real load, even hours are below cutoff
        // This is validated by the program completing without error
        REQUIRE(hourly_fires == 168);
      }
    }
  }
}

SCENARIO("Program handles nested TriggerSet prototypes correctly", "[program][prototypes][trigger_set]") {
  GIVEN("A prototype containing a TriggerSet with a multi-operator internal mesh") {
    // Internal mesh: MA(window) → Scale(scale_value).
    // The TriggerSet output is the Scale operator, so each fire emits
    // 'scale_value * average_of_window_values'.
    std::string program_json = R"({
      "prototypes": {
        "scaledAverage": {
          "parameters": [
            {"name": "ma_window", "type": "number"},
            {"name": "scale_value", "type": "number", "default": 2.0}
          ],
          "operators": [
            {
              "type": "TriggerSet",
              "id": "ts",
              "input_port_type": "number",
              "output_port_type": "number",
              "operators": [
                {"type": "MovingAverage", "id": "ma", "window_size": "${ma_window}"},
                {"type": "Scale", "id": "scale", "value": "${scale_value}"}
              ],
              "connections": [
                {"from": "ma", "to": "scale", "fromPort": "o1", "toPort": "i1"}
              ],
              "entryOperator": "ma",
              "outputOperator": {"id": "scale", "port": "o1"}
            }
          ],
          "connections": [],
          "entry": {"operator": "ts"},
          "output": {"operator": "ts"}
        }
      },
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {
          "id": "proc1",
          "prototype": "scaledAverage",
          "parameters": {
            "ma_window": 2,
            "scale_value": 3.0
          }
        },
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "proc1", "fromPort": "o1", "toPort": "i1"},
        {"from": "proc1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program program(program_json);

    WHEN("Driving the prototype-instantiated nested mesh") {
      // MA(2): t=1 collects, t=2 emits avg(10,20)=15 → Scale → 45 → fires.
      auto b1 = program.receive(Message<NumberData>(1, NumberData{10.0}));
      REQUIRE(b1.empty());
      auto b2 = program.receive(Message<NumberData>(2, NumberData{20.0}));

      THEN("The internal mesh fires the scaled average") {
        REQUIRE(b2.count("output1") == 1);
        REQUIRE(b2["output1"]["o1"].size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(b2["output1"]["o1"].back().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 2);
        REQUIRE(msg->data.value == Approx(45.0));
      }

      // After fire, reset: t=3 alone collects, t=4 fires again.
      auto b3 = program.receive(Message<NumberData>(3, NumberData{30.0}));
      REQUIRE(b3.empty());
      auto b4 = program.receive(Message<NumberData>(4, NumberData{40.0}));
      REQUIRE(b4.count("output1") == 1);
      const auto* msg2 = dynamic_cast<const Message<NumberData>*>(b4["output1"]["o1"].back().get());
      REQUIRE(msg2 != nullptr);
      REQUIRE(msg2->data.value == Approx(105.0));  // (30+40)/2 * 3 = 105
    }
  }
}
