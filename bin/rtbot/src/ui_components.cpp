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

  return Container::Vertical({Renderer([] { return text("Program list") | bold | hcenter; }),
                              Menu(&state.program_display_names, &state.selected_program)}) |
         border;
}

Component UIComponents::create_csv_list(AppState& state) {
  state.csv_display_names.clear();
  for (const auto& csv : state.csv_files) {
    state.csv_display_names.push_back(std::filesystem::path(csv).filename().string());
  }
  if (state.csv_display_names.empty()) {
    state.csv_display_names.push_back("No CSV files found");
  }

  return Container::Vertical({Renderer([] { return text("CSV list") | bold | hcenter; }),
                              Menu(&state.csv_display_names, &state.selected_csv)}) |
         border;
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

  return Container::Horizontal({
             init_button,
             prev_button,
             next_button,
             next_10_button,
             next_100_button,
             next_1000_button,
         }) |
         border;
}

Component UIComponents::create_options(AppState& state) {
  auto show_all = Checkbox("Show all outputs", &state.show_all_outputs);
  return Container::Vertical({Renderer([] { return text("Options") | bold | hcenter; }), show_all}) | border |
         size(HEIGHT, LESS_THAN, 5);
}

Component UIComponents::create_message_log(const AppState& state) {
  return Renderer([&] {
    std::string header = "Message " + std::to_string(state.current_message_index) + "/" +
                         std::to_string(state.current_data.times.size());
    Elements elements;
    elements.push_back(text(header) | bold | hcenter);
    elements.push_back(separator());

    // Calculate visible range for scrolling
    size_t start = state.current_message_index > 10 ? state.current_message_index - 10 : 0;
    size_t end = std::min(start + 20, state.messages.size());

    for (size_t i = start; i < end; i++) {
      const auto& msg = state.messages[i];
      elements.push_back(text("Time: " + std::to_string(msg.time) + ", Value: " + std::to_string(msg.value)));
    }
    return vbox(std::move(elements)) | vscroll_indicator | frame;
  });
}

Component UIComponents::create_output_log(const AppState& state) {
  return Renderer([&] {
    Elements elements;

    // Calculate visible range for scrolling
    size_t start = state.current_message_index > 10 ? state.current_message_index - 10 : 0;
    size_t end = std::min(start + 20, state.messages.size());

    for (size_t i = start; i < end; i++) {
      const auto& msg = state.messages[i];
      if (!msg.output.empty()) {
        elements.push_back(text(msg.output));
      }
    }
    return vbox(std::move(elements)) | vscroll_indicator | frame;
  });
}

Component UIComponents::create_graph(const AppState& state) {
  class GraphFunction {
   public:
    explicit GraphFunction(const std::deque<double>& values) : values_(values) {}

    std::vector<int> operator()(int width, int height) const {
      if (values_.empty() || width == 0 || height == 0) {
        return std::vector<int>(width, 0);
      }

      std::vector<int> output(width);
      double min_val = *std::min_element(values_.begin(), values_.end());
      double max_val = *std::max_element(values_.begin(), values_.end());
      double range = (max_val - min_val) < 1e-10 ? 1.0 : max_val - min_val;

      for (int i = 0; i < width; ++i) {
        size_t data_idx = i * values_.size() / width;
        if (data_idx >= values_.size()) data_idx = values_.size() - 1;

        double val = values_[data_idx];
        output[i] = static_cast<int>((val - min_val) / range * (height - 2)) + 1;
      }
      return output;
    }

   private:
    const std::deque<double>& values_;
  };

  return Renderer([&] {
    if (state.graph_values.empty()) {
      return vbox({text("Time Series") | bold | hcenter});
    }

    GraphFunction graph_func(state.graph_values);

    return vbox(
        {text("Time Series") | bold | hcenter, graph(std::ref(graph_func)) | flex | color(Color::Green),
         hbox(text("min: " + std::to_string(*std::min_element(state.graph_values.begin(), state.graph_values.end()))) |
                  size(WIDTH, EQUAL, 20),
              text("max: " + std::to_string(*std::max_element(state.graph_values.begin(), state.graph_values.end()))) |
                  size(WIDTH, EQUAL, 20))});
  });
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

      // Update graph data
      state.graph_values.push_back(msg.value);
      state.graph_labels.push_back(std::to_string(msg.time));

      if (state.graph_values.size() > state.MAX_GRAPH_POINTS) {
        state.graph_values.pop_front();
        state.graph_labels.pop_front();
      }

      state.messages.push_back(msg);
      state.current_message_index++;
    } catch (const std::exception& e) {
      msg.output = "Error processing message: " + std::string(e.what());
      state.messages.push_back(msg);
      break;
    }
  }
}

Component UIComponents::create_main_layout(AppState& state) {
  auto left_panel =
      Container::Vertical({Container::Vertical({create_program_list(state), create_csv_list(state)}),
                           Container::Vertical({Container::Vertical({create_message_log(state)}) | flex | border,
                                                create_controls(state)})}) |
      size(WIDTH, EQUAL, 30);

  std::vector<Component> right_panel_elements = {
      create_options(state), Container::Vertical({Renderer([] { return text("Outputs") | bold | hcenter; }),
                                                  create_output_log(state) | flex}) |
                                 flex | border};

  if (!state.args.disable_chart) {
    right_panel_elements.push_back(Container::Vertical({create_graph(state)}) | size(HEIGHT, EQUAL, 15) | border);
  }

  auto right_panel = Container::Vertical(right_panel_elements) | flex;

  return Container::Horizontal({left_panel, right_panel});
}

}  // namespace rtbot_cli