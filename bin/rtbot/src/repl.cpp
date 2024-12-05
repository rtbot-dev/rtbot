#include "repl.h"

#include <termios.h>
#include <yaml-cpp/yaml.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "rtbot/bindings.h"
extern "C" {
#include "linenoise.h"
}

namespace rtbot_cli {

const std::string JSON_NULL = "\033[1;30mnull\033[0m";
const std::string JSON_STRING = "\033[0;32m\"\033[0m";
const std::string JSON_NUMBER = "\033[0;33m";
const std::string JSON_BOOL_TRUE = "\033[0;35mtrue\033[0m";
const std::string JSON_BOOL_FALSE = "\033[0;35mfalse\033[0m";
const std::string JSON_BRACKETS = "\033[0;36m";
const std::string COLOR_RESET = "\033[0m";

REPL::REPL(const std::string& program_id, const CLIArguments& args) : program_id_(program_id), args_(args) {
  entry_operator_ = rtbot::get_program_entry_operator_id(program_id);

  linenoiseHistorySetMaxLen(1000);
  linenoiseHistoryLoad(".rtbot_history");
}

REPL::~REPL() { linenoiseHistorySave(".rtbot_history"); }

std::string REPL::colorize_json(const nlohmann::json& j, int indent = 0) const {
  std::string result;
  if (j.is_null()) {
    result += JSON_NULL;
  } else if (j.is_string()) {
    result += JSON_STRING + j.get<std::string>() + JSON_STRING;
  } else if (j.is_number()) {
    result += JSON_NUMBER + j.dump() + COLOR_RESET;
  } else if (j.is_boolean()) {
    result += j.get<bool>() ? JSON_BOOL_TRUE : JSON_BOOL_FALSE;
  } else if (j.is_object()) {
    result += JSON_BRACKETS + "{" + COLOR_RESET + "\n";
    bool first = true;
    for (auto it = j.begin(); it != j.end(); ++it) {
      if (!first) result += ",\n";
      result += std::string(indent + 2, ' ');
      result += JSON_STRING + it.key() + JSON_STRING + ": ";
      result += colorize_json(it.value(), indent + 2);
      first = false;
    }
    result += "\n" + std::string(indent, ' ') + JSON_BRACKETS + "}" + COLOR_RESET;
  } else if (j.is_array()) {
    result += JSON_BRACKETS + "[" + COLOR_RESET + "\n";
    bool first = true;
    for (const auto& element : j) {
      if (!first) result += ",\n";
      result += std::string(indent + 2, ' ');
      result += colorize_json(element, indent + 2);
      first = false;
    }
    result += "\n" + std::string(indent, ' ') + JSON_BRACKETS + "]" + COLOR_RESET;
  }
  return result;
}

void REPL::print_result(const std::string& result) const {
  try {
    // First parse as JSON to ensure consistent input
    auto parsed_json = nlohmann::json::parse(result);

    if (args_.format == OutputFormat::YAML) {
      // Convert JSON to YAML nodes recursively
      std::function<YAML::Node(const nlohmann::json&)> to_yaml = [&](const nlohmann::json& j) {
        YAML::Node node;
        if (j.is_object()) {
          for (auto it = j.begin(); it != j.end(); ++it) {
            node[it.key()] = to_yaml(it.value());
          }
        } else if (j.is_array()) {
          for (const auto& elem : j) {
            node.push_back(to_yaml(elem));
          }
        } else if (j.is_string()) {
          node = j.get<std::string>();
        } else if (j.is_number()) {
          node = j.get<double>();
        } else if (j.is_boolean()) {
          node = j.get<bool>();
        } else if (j.is_null()) {
          node = YAML::Node(YAML::NodeType::Null);
        }
        return node;
      };

      YAML::Node yaml = to_yaml(parsed_json);
      std::cout << "\033[0;36m" << YAML::Dump(yaml) << COLOR_RESET << std::endl;
    } else {
      std::cout << colorize_json(parsed_json) << std::endl;
    }
    std::cout.flush();
  } catch (const std::exception& e) {
    std::cerr << "\033[0;31mError formatting output: " << e.what() << COLOR_RESET << std::endl;
    std::cerr.flush();
  }
}

void REPL::process_message(const nlohmann::json& msg) {
  try {
    uint64_t time = msg["time"];
    double value = msg["value"];

    time = static_cast<uint64_t>(time * args_.scale_t);
    value *= args_.scale_y;

    rtbot::add_to_message_buffer(program_id_, entry_operator_, time, value);

    std::string result;
    if (args_.debug) {
      result = rtbot::process_message_buffer_debug(program_id_);
    } else {
      result = rtbot::process_message_buffer(program_id_);
    }

    std::cout << std::endl;  // Add line break before output
    print_result(result);
    std::cout << std::endl;  // Add line break after output
    std::cout.flush();

  } catch (const std::exception& e) {
    std::cerr << "\033[0;31mError processing message: " << e.what() << COLOR_RESET << std::endl;
    std::cerr.flush();
  }
}

void REPL::print_prompt() const { std::cout << "rtbot> "; }

void REPL::print_help() const {
  std::cout << "Available commands:\n"
            << "  .help     - Show this help message\n"
            << "  .quit     - Exit the REPL\n"
            << "  .debug    - Toggle debug mode (current: " << (args_.debug ? "on" : "off") << ")\n"
            << "  .format   - Toggle output format (current: " << (args_.format == OutputFormat::JSON ? "JSON" : "YAML")
            << ")\n"
            << "\n"
            << "Or enter a JSON message in the format:\n"
            << "  {\"time\": <timestamp>, \"value\": <number>}\n"
            << std::endl;
}

bool REPL::process_command(const std::string& input) {
  if (input == ".help") {
    print_help();
    return true;
  }
  if (input == ".quit") {
    return false;
  }
  if (input == ".debug") {
    args_.debug = !args_.debug;
    std::cout << "Debug mode " << (args_.debug ? "enabled" : "disabled") << std::endl;
    std::cout.flush();
    return true;
  }
  if (input == ".format") {
    args_.format = (args_.format == OutputFormat::JSON) ? OutputFormat::YAML : OutputFormat::JSON;
    std::cout << "Output format set to " << (args_.format == OutputFormat::JSON ? "JSON" : "YAML") << std::endl;
    std::cout.flush();
    return true;
  }
  return true;
}

void REPL::process_csv_input(const std::string& input) {
  std::stringstream ss(input);
  std::string time_str, value_str;

  if (std::getline(ss, time_str, ',') && std::getline(ss, value_str)) {
    try {
      uint64_t time = std::stoull(time_str);
      double value = std::stod(value_str);
      nlohmann::json msg = {{"time", time}, {"value", value}};
      process_message(msg);
    } catch (const std::exception& e) {
      std::cerr << "\033[0;31mError parsing CSV input: " << e.what() << COLOR_RESET << std::endl;
    }
  }
}

void REPL::run() {
  std::cout << "\033[1;34mRTBot REPL Mode\033[0m\n"
            << "Type \033[0;32m.help\033[0m for available commands\n"
            << std::endl;

  char* line;
  while ((line = linenoise("\033[0;32mrtbot>\033[0m")) != nullptr) {
    std::string input(line);

    if (!input.empty()) {
      linenoiseHistoryAdd(line);

      if (input[0] == '.') {
        if (!process_command(input)) {
          free(line);
          break;
        }
      } else if (input.find(',') != std::string::npos) {
        process_csv_input(input);
      } else {
        try {
          auto msg = nlohmann::json::parse(input);
          process_message(msg);
        } catch (const nlohmann::json::parse_error& e) {
          std::cerr << "\033[0;31mInvalid input format. Use JSON or CSV (time,value)\033[0m" << std::endl;
        }
      }
    }

    free(line);
  }
}

}  // namespace rtbot_cli