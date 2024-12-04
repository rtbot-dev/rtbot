#include "ui_components.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#include "data_loader.h"

namespace rtbot_cli {

using namespace ftxui;

Component UIComponents::create_program_list(AppState& state) {
  state.program_display_names.clear();
  for (const auto& prog : state.programs) {
    state.program_display_names.push_back(prog.name);
  }
  if (state.program_display_names.empty()) {
    state.program_display_names.push_back("No programs found");
  }

  auto menu = Menu(&state.program_display_names, &state.selected_program);
  return Container::Vertical({Renderer([] { return text("Program list") | bold | hcenter; }), menu}) | border;
}

Component UIComponents::create_csv_list(AppState& state) {
  state.csv_display_names.clear();
  for (const auto& csv : state.csv_files) {
    state.csv_display_names.push_back(std::filesystem::path(csv).filename().string());
  }
  if (state.csv_display_names.empty()) {
    state.csv_display_names.push_back("No CSV files found");
  }

  auto menu = Menu(&state.csv_display_names, &state.selected_csv);
  return Container::Vertical({Renderer([] { return text("CSV list") | bold | hcenter; }), menu}) | border;
}

void UIComponents::process_next_n_messages(AppState& state, size_t n) {
  if (!state.program_initialized || state.current_data.times.empty() || state.current_data.values.empty() ||
      state.current_message_index >= state.current_data.times.size()) {
    return;
  }

  size_t remaining = state.current_data.times.size() - state.current_message_index;
  size_t to_process = std::min(n, remaining);

  for (size_t i = 0; i < to_process; i++) {
    ProcessedMessage msg{state.current_data.times[state.current_message_index],
                         state.current_data.values[state.current_message_index], ""};

    try {
      rtbot::add_to_message_buffer("program1", "input1", msg.time, msg.value);
      msg.output = state.show_all_outputs ? rtbot::process_message_buffer_debug("program1")
                                          : rtbot::process_message_buffer("program1");

      state.messages.push_back(msg);
      state.current_message_index++;
    } catch (const std::exception& e) {
      msg.output = "Error processing message: " + std::string(e.what());
      state.messages.push_back(msg);
      break;
    }
  }
}

Component UIComponents::create_controls(AppState& state) {
  auto init_button = Button("⏺ Initialize", [&] {
    if (!state.program_initialized && !state.programs.empty() && !state.csv_files.empty()) {
      rtbot::create_program("program1", state.programs[state.selected_program].content);
      state.current_data = DataLoader::load_csv(state.csv_files[state.selected_csv], state.args);
      state.program_initialized = true;
      state.current_message_index = 0;
    }
  });

  auto prev_button = Button("⏮", [&] {
    if (state.current_message_index > 0) {
      state.current_message_index--;
    }
  });

  auto next_button = Button("⏭", [&] { process_next_n_messages(state, 1); });

  auto next_10_button = Button("⏭ x10", [&] { process_next_n_messages(state, 10); });

  auto next_100_button = Button("⏭ x100", [&] { process_next_n_messages(state, 100); });

  auto next_1000_button = Button("⏭ x1000", [&] { process_next_n_messages(state, 1000); });

  return Container::Vertical({Renderer([] { return text("Controls") | bold | hcenter; }), Container::Horizontal({
                                                                                              init_button,
                                                                                              prev_button,
                                                                                              next_button,
                                                                                              next_10_button,
                                                                                              next_100_button,
                                                                                              next_1000_button,
                                                                                          }) | border});
}

Component UIComponents::create_options(AppState& state) {
  auto show_all = Checkbox("Show all outputs", &state.show_all_outputs);
  return Container::Vertical({Renderer([] { return text("Options") | bold | hcenter; }), show_all}) | border |
         size(HEIGHT, LESS_THAN, 5);
}

Component UIComponents::create_message_log(const AppState& state) {
  return Renderer([&] {
    Elements elements;
    for (const auto& msg : state.messages) {
      elements.push_back(text("Time: " + std::to_string(msg.time) + ", Value: " + std::to_string(msg.value)));
    }
    return vbox(std::move(elements)) | vscroll_indicator | frame;
  });
}

Component UIComponents::create_output_log(const AppState& state) {
  return Renderer([&] {
    Elements elements;
    for (const auto& msg : state.messages) {
      if (!msg.output.empty()) {
        elements.push_back(text(msg.output));
      }
    }
    return vbox(std::move(elements)) | vscroll_indicator | frame;
  });
}

Component UIComponents::create_main_layout(AppState& state) {
  auto top_section =
      Container::Horizontal({create_program_list(state), create_csv_list(state), create_options(state)}) |
      size(HEIGHT, LESS_THAN, 10);

  auto message_log = create_message_log(state);
  auto output_log = create_output_log(state);

  auto main_content =
      Container::Horizontal(
          {Container::Vertical(
               {Renderer([] { return text("Input") | bold | hcenter; }), message_log | flex, create_controls(state)}) |
               size(WIDTH, EQUAL, 50),
           Container::Vertical({Renderer([] { return text("Outputs") | bold | hcenter; }), output_log | flex}) |
               flex}) |
      size(HEIGHT, GREATER_THAN, 40);

  return Container::Vertical({top_section, main_content | flex});
}

}  // namespace rtbot_cli