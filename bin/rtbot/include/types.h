// types.h
#pragma once

#include <string>
#include <vector>

#include "args.h"
#include "rtbot/Program.h"
#include "rtbot/bindings.h"

namespace rtbot_cli {

struct ProgramInfo {
  std::string name;
  std::string path;
  std::string content;
  bool is_valid;
};

struct CSVData {
  std::vector<uint64_t> times;
  std::vector<double> values;
};

struct ProcessedMessage {
  uint64_t time;
  double value;
  std::string output;
};

struct GraphDataPoint {
  double timestamp;
  std::unordered_map<std::string, double> values;  // operator_id -> value
};
struct OperatorInfo {
  std::string type;
  std::string id;
  bool selected;
};
struct AppState {
  std::vector<ProgramInfo> programs;
  std::vector<std::string> csv_files;
  std::vector<std::string> program_display_names;
  std::vector<std::string> csv_display_names;
  std::vector<ProcessedMessage> messages;
  std::vector<std::string> available_operators;
  std::set<std::string> selected_operators;
  bool show_all_outputs{false};
  int selected_program{0};
  int selected_csv{0};
  size_t current_message_index{0};
  bool program_initialized{false};
  CSVData current_data;
  CLIArguments args;
  std::deque<GraphDataPoint> graph_data{};
  static const size_t MAX_GRAPH_POINTS = 1000;
  std::string entry_operator;

  std::vector<OperatorInfo> operators;
  std::vector<std::string> log_messages;

  void log(const std::string& msg) {
    log_messages.push_back(msg);
    if (log_messages.size() > 100) {
      log_messages.erase(log_messages.begin());
    }
  }

  AppState() {
    // Add initial sine wave data for testing
    for (int i = 0; i < 10; ++i) {
      double x = i * 0.1;
      graph_data.push_back({x, {{"input", std::sin(x)}}});
      // graph_values.push_back(std::sin(x));
      // graph_labels.push_back(std::to_string(i));
    }
  }
};

}  // namespace rtbot_cli