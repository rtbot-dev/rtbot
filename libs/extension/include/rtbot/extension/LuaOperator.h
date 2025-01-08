#ifndef LUA_OPERATOR_H
#define LUA_OPERATOR_H

#include <sol/sol.hpp>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class LuaOperator : public Operator {
 public:
  // Constructor takes operator ID and Lua code string
  LuaOperator(std::string id, const std::string& lua_code) : Operator(std::move(id)) {
    // Create Lua state and configure security sandbox
    lua_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    // Block dangerous functions
    lua_["os"] = nullptr;
    lua_["io"] = nullptr;
    lua_["require"] = nullptr;
    lua_["dofile"] = nullptr;
    lua_["loadfile"] = nullptr;

    // Register message creation helper
    lua_["create_message"] = [](timestamp_t time, double value) {
      return create_message<NumberData>(time, NumberData{value});
    };

    // Create port management functions
    lua_["add_data_port"] = [this]() { this->add_data_port<NumberData>(); };
    lua_["add_control_port"] = [this]() { this->add_control_port<NumberData>(); };
    lua_["add_output_port"] = [this]() { this->add_output_port<NumberData>(); };

    // Register process_data callback
    try {
      lua_.script(lua_code);
      process_data_fn_ = lua_["process_data"];
      if (!process_data_fn_.valid()) {
        throw std::runtime_error("Lua code must define process_data function");
      }
    } catch (const sol::error& e) {
      throw std::runtime_error("Invalid Lua code: " + std::string(e.what()));
    }
  }

  std::string type_name() const override { return "LuaOperator"; }

 protected:
  void process_data() override {
    // Get input queues
    std::vector<std::vector<double>> input_values;
    std::vector<std::vector<timestamp_t>> input_times;

    for (size_t i = 0; i < num_data_ports(); ++i) {
      std::vector<double> port_values;
      std::vector<timestamp_t> port_times;
      auto& queue = get_data_queue(i);

      for (const auto& msg : queue) {
        if (auto* num_msg = dynamic_cast<const Message<NumberData>*>(msg.get())) {
          port_values.push_back(num_msg->data.value);
          port_times.push_back(num_msg->time);
        }
      }

      input_values.push_back(std::move(port_values));
      input_times.push_back(std::move(port_times));
    }

    // Call Lua process_data function
    try {
      auto results = process_data_fn_(input_values, input_times);

      // Process results - expecting a table of {port_index, messages} pairs
      sol::table result_table = results;
      for (const auto& kvp : result_table) {
        size_t port_index = kvp.first.as<size_t>();
        sol::table messages = kvp.second;

        auto& output_queue = get_output_queue(port_index);
        for (const auto& msg_pair : messages) {
          sol::table msg = msg_pair.second;
          timestamp_t time = msg["time"];
          double value = msg["value"];
          output_queue.push_back(create_message<NumberData>(time, NumberData{value}));
        }
      }
    } catch (const sol::error& e) {
      throw std::runtime_error("Error in Lua process_data: " + std::string(e.what()));
    }

    // Clear input queues
    for (size_t i = 0; i < num_data_ports(); ++i) {
      get_data_queue(i).clear();
    }
  }

 private:
  sol::state lua_;
  sol::function process_data_fn_;
};

}  // namespace rtbot

#endif  // LUA_OPERATOR_H