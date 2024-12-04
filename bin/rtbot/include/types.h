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
};

}  // namespace rtbot_cli