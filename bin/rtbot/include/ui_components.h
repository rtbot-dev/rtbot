#pragma once

#include <ftxui/component/component.hpp>

#include "types.h"

namespace rtbot_cli {

using namespace ftxui;

class UIComponents {
 public:
  static Component create_program_list(AppState& state);
  static Component create_csv_list(AppState& state);
  static Component create_controls(AppState& state);
  static Component create_options(AppState& state);
  static Component create_message_log(const AppState& state);
  static Component create_output_log(const AppState& state);
  static Component create_main_layout(AppState& state);

 private:
  static void process_next_n_messages(AppState& state, size_t n);
};

}  // namespace rtbot_cli