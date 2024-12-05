#include "repl.h"

#include <termios.h>
#include <yaml-cpp/yaml.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "data_loader.h"
#include "linenoise.hpp"
#include "rtbot/bindings.h"

namespace rtbot_cli {

const std::string JSON_NULL = "\033[1;30mnull\033[0m";
const std::string JSON_STRING = "\033[0;32m\"\033[0m";
const std::string JSON_NUMBER = "\033[0;33m";
const std::string JSON_BOOL_TRUE = "\033[0;35mtrue\033[0m";
const std::string JSON_BOOL_FALSE = "\033[0;35mfalse\033[0m";
const std::string JSON_BRACKETS = "\033[0;36m";
const std::string COLOR_RESET = "\033[0m";

REPL::REPL(const std::string& program_id, const CLIArguments& args) : program_id_(program_id), args_(args) {
  args_.debug = true;                 // Debug true by default
  args_.format = OutputFormat::YAML;  // YAML default format

  linenoise::SetHistoryMaxLen(1000);
  linenoise::LoadHistory(".rtbot_history");

  show_scale_warning();
}

void REPL::show_scale_warning() {
  if (args_.scale_t != 1.0 || args_.scale_y != 1.0) {
    std::cout << "\033[1;33mScaling factors:\033[0m\n"
              << "Time: × " << args_.scale_t << "\n"
              << "Value: × " << args_.scale_y << "\n"
              << std::endl;
  }
}

void REPL::print_help() const {
  std::cout << "Available commands:\n"
            << "  .help     - Show this help message\n"
            << "  .quit     - Exit the REPL\n"
            << "  .debug    - Toggle debug mode (current: " << (args_.debug ? "on" : "off") << ")\n"
            << "  .format   - Toggle output format (current: " << (args_.format == OutputFormat::JSON ? "JSON" : "YAML")
            << ")\n"
            << "  .load_csv <path> - Load data from CSV file\n"
            << "  .next [N] - Process next N messages (default: 1)\n"
            << "  .scale_t <value> - Set time scale factor\n"
            << "  .scale_y <value> - Set value scale factor\n"
            << std::endl;
}

REPL::~REPL() { linenoise::SaveHistory(".rtbot_history"); }

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

    // Only apply scaling for direct input, not for loaded CSV data
    if (loaded_filename_.empty()) {
      time = static_cast<uint64_t>(time * args_.scale_t);
      value *= args_.scale_y;
    }

    rtbot::add_to_message_buffer(program_id_, entry_operator_, time, value);

    std::string result;
    if (args_.debug) {
      result = rtbot::process_message_buffer_debug(program_id_);
    } else {
      result = rtbot::process_message_buffer(program_id_);
    }

    std::cout << std::endl;
    print_result(result);
    std::cout << std::endl;
    std::cout.flush();

  } catch (const std::exception& e) {
    std::cerr << "\033[0;31mError processing message: " << e.what() << COLOR_RESET << std::endl;
    std::cerr.flush();
  }
}

void REPL::print_prompt() const {
  if (!loaded_filename_.empty()) {
    std::cout << "\033[0;36m[" << loaded_filename_ << ":" << current_data_index_ << "/" << loaded_data_.times.size()
              << "]\033[0m ";
  }
  std::cout << "rtbot> ";
}

bool REPL::process_command(const std::string& input) {
  std::istringstream iss(input);
  std::string cmd;
  iss >> cmd;

  if (cmd == ".scale_t" || cmd == ".scale_y") {
    std::string value_str;
    iss >> value_str;
    try {
      double value = std::stod(value_str);
      if (cmd == ".scale_t") {
        args_.scale_t = value;
      } else {
        args_.scale_y = value;
      }
      show_scale_warning();
    } catch (...) {
      std::cerr << "\033[0;31mInvalid scale value\033[0m" << std::endl;
    }
    return true;
  }

  if (cmd == ".load_csv") {
    std::string path;
    std::getline(iss >> std::ws, path);
    if (path.empty()) {
      std::cerr << "\033[0;31mUsage: .load_csv <path>\033[0m" << std::endl;
    } else {
      handle_load_csv(path);
    }
    return true;
  }

  if (cmd == ".next") {
    std::string count_str;
    iss >> count_str;
    size_t count = 1;
    if (!count_str.empty()) {
      try {
        count = std::stoull(count_str);
      } catch (...) {
        std::cerr << "\033[0;31mInvalid count value\033[0m" << std::endl;
        return true;
      }
    }
    process_n_messages(count);
    return true;
  }

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

std::string REPL::get_prompt() const {
  std::stringstream prompt;
  if (!loaded_filename_.empty()) {
    prompt << "\033[0;36m[" << loaded_filename_ << ":" << current_data_index_ << "/" << loaded_data_.times.size()
           << "]\033[0m ";
  }
  prompt << "rtbot> ";
  return prompt.str();
}

void REPL::run() {
  std::cout << "\033[1;34mRTBot REPL Mode\033[0m\n"
            << "Type \033[0;32m.help\033[0m for available commands\n"
            << std::endl;

  while (true) {
    std::string line;
    auto quit = linenoise::Readline(get_prompt().c_str(), line);

    if (quit) {
      break;
    }

    std::string input(line);

    if (!input.empty()) {
      linenoise::AddHistory(line.c_str());

      if (input[0] == '.') {
        if (!process_command(input)) {
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
  }
}

void REPL::handle_load_csv(const std::string& args) {
  try {
    loaded_data_ = DataLoader::load_csv(args, args_);
    loaded_filename_ = args;
    current_data_index_ = 0;

    std::cout << "\033[0;32mLoaded " << loaded_data_.times.size() << " records from " << args << COLOR_RESET
              << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "\033[0;31mError loading CSV: " << e.what() << COLOR_RESET << std::endl;
  }
}

void REPL::process_n_messages(size_t n) {
  if (loaded_data_.times.empty()) {
    std::cerr << "\033[0;31mNo data loaded. Use .load_csv to load a data file\033[0m" << std::endl;
    return;
  }

  if (current_data_index_ >= loaded_data_.times.size()) {
    std::cout << "\033[0;33mEnd of data reached\033[0m" << std::endl;
    return;
  }

  size_t remaining = loaded_data_.times.size() - current_data_index_;
  size_t to_process = std::min(n, remaining);

  for (size_t i = 0; i < to_process; i++) {
    nlohmann::json msg = {{"time", loaded_data_.times[current_data_index_]},
                          {"value", loaded_data_.values[current_data_index_]}};
    process_message(msg);
    current_data_index_++;
  }
}

}  // namespace rtbot_cli