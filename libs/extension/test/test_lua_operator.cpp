#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "rtbot/extension/LuaOperator.h"

using namespace rtbot;

SCENARIO("LuaOperator handles basic configuration and execution", "[LuaOperator]") {
  GIVEN("A LuaOperator with a simple passthrough script") {
    const std::string lua_code = R"(
            add_data_port()
            add_output_port()

            function process_data(input_values, input_times)
                if #input_values[1] == 0 then
                    return {[0] = {}}
                end

                local results = {}
                for i = 1, #input_values[1] do
                    table.insert(results, {
                        time = input_times[1][i],
                        value = input_values[1][i]
                    })
                end
                return {[0] = results}
            end
        )";

    auto op = std::make_unique<LuaOperator>("lua1", lua_code);

    WHEN("Processing a single message") {
      op->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      op->execute();

      THEN("Message is passed through unchanged") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 42.0);
      }
    }

    WHEN("Processing multiple messages") {
      op->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      op->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      op->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      op->execute();

      THEN("All messages are processed correctly") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 3);

        for (size_t i = 0; i < output.size(); ++i) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
          REQUIRE(msg->time == i + 1);
          REQUIRE(msg->data.value == static_cast<double>(i + 1));
        }
      }
    }
  }
}

SCENARIO("LuaOperator implements moving average correctly", "[LuaOperator]") {
  GIVEN("A LuaOperator implementing moving average") {
    const std::string lua_code = R"(
            add_data_port()
            add_output_port()

            function process_data(input_values, input_times)
                if #input_values[1] == 0 then
                    return {[0] = {}}
                end

                local values = input_values[1]
                local times = input_times[1]
                local window_size = 3

                -- Initialize results table
                local results = {}

                -- Only process if we have enough values
                if #values >= window_size then
                    -- Calculate sum for first window
                    local sum = 0
                    for i = 1, window_size do
                        sum = sum + values[i]
                    end
                    
                    -- Add first average
                    table.insert(results, {
                        time = times[window_size],
                        value = sum / window_size
                    })
                end
                
                return {[0] = results}
            end
        )";

    auto op = std::make_unique<LuaOperator>("lua_ma", lua_code);

    WHEN("Processing enough points to fill window") {
      op->receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      op->receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
      op->receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);
      op->execute();

      THEN("Moving average is calculated correctly") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == 4.0);  // (2 + 4 + 6) / 3
      }
    }

    WHEN("Processing not enough points") {
      op->receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      op->receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
      op->execute();

      THEN("No output is produced") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.empty());
      }
    }
  }
}

SCENARIO("LuaOperator handles error conditions", "[LuaOperator]") {
  SECTION("Invalid Lua syntax") {
    const std::string invalid_code = R"(
            function process_data(
                this is invalid
            end
        )";

    REQUIRE_THROWS_WITH(LuaOperator("invalid", invalid_code), Catch::Contains("Invalid Lua code"));
  }

  SECTION("Missing process_data function") {
    const std::string incomplete_code = R"(
            add_data_port()
            add_output_port()
            -- No process_data function defined
        )";

    REQUIRE_THROWS_WITH(LuaOperator("incomplete", incomplete_code),
                        Catch::Contains("must define process_data function"));
  }

  SECTION("Runtime error in process_data") {
    const std::string error_code = R"(
            add_data_port()
            add_output_port()

            function process_data(input_values, input_times)
                error("Intentional error")
                return {[0] = {}}
            end
        )";

    auto op = std::make_unique<LuaOperator>("error", error_code);
    op->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
    REQUIRE_THROWS_WITH(op->execute(), Catch::Contains("Error in Lua process_data"));
  }
}

SCENARIO("LuaOperator maintains security restrictions", "[LuaOperator]") {
  SECTION("File system access is blocked") {
    const std::string file_access_code = R"(
            add_data_port()
            add_output_port()

            function process_data(input_values, input_times)
                return {[0] = {}} -- Should fail before reaching here
            end

            -- This should fail during initialization
            local file = io.open("test.txt", "w")
        )";

    REQUIRE_THROWS(LuaOperator("file_access", file_access_code));
  }

  SECTION("OS execution is blocked") {
    const std::string os_access_code = R"(
            add_data_port()
            add_output_port()

            function process_data(input_values, input_times)
                return {[0] = {}} -- Should fail before reaching here
            end

            -- This should fail during initialization
            os.execute("echo test")
        )";

    REQUIRE_THROWS(LuaOperator("os_access", os_access_code));
  }
}