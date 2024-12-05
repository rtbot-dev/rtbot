#include "ui_components.h"

#include <algorithm>
#include <filesystem>
#include <numeric>
#include <vector>

#include "data_loader.h"

namespace rtbot_cli {

using namespace ftxui;

class MultiSeriesGraphFunction {
 public:
  MultiSeriesGraphFunction(const std::deque<GraphDataPoint>& data, const std::vector<std::string>& series)
      : data_(data), series_(series) {}

  std::map<std::string, std::vector<int>> operator()(int width, int height) const {
    std::map<std::string, std::vector<int>> outputs;
    if (data_.empty() || width == 0 || height == 0) {
      return outputs;
    }

    // Find global min/max across all series for consistent scaling
    double global_min = std::numeric_limits<double>::max();
    double global_max = std::numeric_limits<double>::lowest();

    std::map<std::string, std::vector<double>> series_values;
    for (const auto& series : series_) {
      std::vector<double>& values = series_values[series];
      for (const auto& point : data_) {
        auto it = point.values.find(series);
        if (it != point.values.end()) {
          values.push_back(it->second);
          global_min = std::min(global_min, it->second);
          global_max = std::max(global_max, it->second);
        }
      }
    }

    double range = std::max(global_max - global_min, 1e-6);

    // Generate points for each series
    for (const auto& [series, values] : series_values) {
      if (values.empty()) continue;

      std::vector<int> output(width);
      size_t step = values.size() / width + 1;

      for (int i = 0; i < width; ++i) {
        size_t idx = std::min(i * step, values.size() - 1);
        double normalized = (values[idx] - global_min) / range;
        output[i] = static_cast<int>(normalized * (height - 1));
      }

      outputs[series] = output;
    }

    return outputs;
  }

 private:
  const std::deque<GraphDataPoint>& data_;
  const std::vector<std::string>& series_;
};

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
      try {
        state.log("Initializing program...");
        state.log("Selected program: " + state.programs[state.selected_program].name);

        auto program_json = nlohmann::json::parse(state.programs[state.selected_program].content);
        state.log("Program JSON parsed. Operator count: " + std::to_string(program_json["operators"].size()));

        auto entry_operator = program_json["entryOperator"].get<std::string>();
        state.operators.clear();

        for (const auto& op : program_json["operators"]) {
          std::string op_info =
              "Adding operator: " + op["type"].get<std::string>() + " (" + op["id"].get<std::string>() + ")";
          state.log(op_info);
          state.operators.push_back({op["type"].get<std::string>(), op["id"].get<std::string>(), false});
        }

        rtbot::create_program("program1", state.programs[state.selected_program].content);
        state.current_data = DataLoader::load_csv(state.csv_files[state.selected_csv], state.args);
        state.program_initialized = true;
        state.entry_operator = entry_operator;
        state.current_message_index = 0;

        state.log("Initialization complete. Total operators: " + std::to_string(state.operators.size()));
      } catch (const std::exception& e) {
        state.log("Error during initialization: " + std::string(e.what()));
      }
    } else {
      state.log("Cannot initialize: programs or CSV files missing");
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
    elements.push_back(text("Latest Values:") | bold);

    // Show latest values from graph_data
    if (!state.graph_data.empty()) {
      const auto& latest = state.graph_data.back();
      for (const auto& [series_name, value] : latest.values) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(6);
        ss << series_name << ": " << value;
        elements.push_back(text(ss.str()));
      }
    }

    return vbox(std::move(elements)) | vscroll_indicator | frame;
  });
}

// Helper function to draw a line using Bresenham's algorithm
void drawLine(std::vector<std::vector<bool>>& pixels, int x1, int y1, int x2, int y2) {
  int dx = std::abs(x2 - x1);
  int dy = std::abs(y2 - y1);
  int sx = x1 < x2 ? 1 : -1;
  int sy = y1 < y2 ? 1 : -1;
  int err = dx - dy;

  while (true) {
    if (x1 >= 0 && x1 < pixels.size() && y1 >= 0 && y1 < pixels[0].size()) {
      pixels[x1][y1] = true;
    }

    if (x1 == x2 && y1 == y2) break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x1 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y1 += sy;
    }
  }
}

Component UIComponents::create_graph(const AppState& state) {
  return Renderer([&] {
    if (state.graph_data.empty()) return vbox({text("Time Series") | bold | hcenter});

    // Collect unique series names
    std::set<std::string> series;
    for (const auto& point : state.graph_data) {
      for (const auto& [key, _] : point.values) {
        series.insert(key);
      }
    }

    // Generate colors dynamically
    std::map<std::string, ftxui::Color> series_colors;
    const std::vector<ftxui::Color> colors = {ftxui::Color::White, ftxui::Color::Green, ftxui::Color::Yellow,
                                              ftxui::Color::Red,   ftxui::Color::Blue,  ftxui::Color::Magenta,
                                              ftxui::Color::Cyan};

    size_t color_idx = 0;
    for (const auto& series_name : series) {
      series_colors[series_name] = colors[color_idx % colors.size()];
      color_idx++;
    }

    // Rest of the implementation remains the same...
    double input_min = std::numeric_limits<double>::max();
    double input_max = std::numeric_limits<double>::lowest();
    for (const auto& point : state.graph_data) {
      auto it = point.values.find("input");
      if (it != point.values.end()) {
        input_min = std::min(input_min, it->second);
        input_max = std::max(input_max, it->second);
      }
    }

    double range = std::max(input_max - input_min, 1e-6);
    double padding = range * 0.1;
    double min_value = input_min - padding;
    double max_value = input_max + padding;

    std::vector<Element> graphs;
    for (const auto& series_name : series) {
      auto graph_func = [&](int width, int height) {
        std::vector<int> output(width, 0);
        if (width == 0 || height == 0) return output;

        std::vector<double> values;
        for (const auto& point : state.graph_data) {
          auto it = point.values.find(series_name);
          if (it != point.values.end()) {
            values.push_back(it->second);
          }
        }

        if (values.empty()) return output;

        for (int i = 0; i < width; ++i) {
          size_t idx = i * values.size() / width;
          if (idx >= values.size()) idx = values.size() - 1;
          double normalized = (values[idx] - min_value) / (max_value - min_value);
          output[i] = static_cast<int>(normalized * (height - 1));
        }
        return output;
      };

      graphs.push_back(graph(std::move(graph_func)) | ftxui::color(series_colors[series_name]));
    }

    std::vector<Element> legend;
    for (const auto& [series_name, color] : series_colors) {
      legend.push_back(text("■ " + series_name) | ftxui::color(color));
    }

    Element combined = dbox(std::move(graphs));

    std::stringstream range_text;
    range_text << std::fixed << std::setprecision(3) << "Range: " << min_value << " to " << max_value;

    return vbox({text("Time Series") | bold | hcenter, combined | flex | size(HEIGHT, EQUAL, 16),
                 hbox(legend) | hcenter, text(range_text.str())});
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
    size_t current_idx = state.current_message_index;
    uint64_t current_time = state.current_data.times[current_idx];
    double current_value = state.current_data.values[current_idx];

    try {
      rtbot::add_to_message_buffer("program1", state.entry_operator, current_time, current_value);
      auto output_json = nlohmann::json::parse(rtbot::process_message_buffer_debug("program1"));

      // Create graph point
      GraphDataPoint point;
      point.timestamp = current_time;
      point.values[state.entry_operator] = current_value;  // Use actual entry operator ID

      // Add values for selected operators
      for (const auto& op : state.operators) {
        if (op.selected && output_json.contains(op.id)) {
          const auto& op_outputs = output_json[op.id];
          for (const auto& [port_id, messages] : op_outputs.items()) {
            if (!messages.empty()) {
              std::string series_name = op.id + "_" + port_id;
              point.values[series_name] = messages.back()["value"].get<double>();
            }
          }
        }
      }

      if (state.graph_data.size() >= state.MAX_GRAPH_POINTS) {
        state.graph_data.pop_front();
      }
      state.graph_data.push_back(point);
      state.current_message_index++;

    } catch (const std::exception& e) {
      state.log("Error processing message: " + std::string(e.what()));
      break;
    }
  }
}

Component UIComponents::create_logger(const AppState& state) {
  static int selected_log = 0;
  return Container::Vertical(
      {Renderer([] { return text("Logger") | bold | hcenter; }), Renderer([&] {
                                                                   Elements log_elements;
                                                                   for (const auto& msg : state.log_messages) {
                                                                     log_elements.push_back(text(msg));
                                                                   }
                                                                   return vbox(std::move(log_elements));
                                                                 }) | vscroll_indicator |
                                                                     yframe | frame | size(HEIGHT, EQUAL, 8)});
}

Component UIComponents::create_main_layout(AppState& state) {
  auto left_panel =
      Container::Vertical({Container::Vertical({create_program_list(state), create_csv_list(state)}) | flex,
                           Container::Vertical({create_message_log(state) | flex | border}), create_controls(state),
                           create_logger(state) | size(HEIGHT, EQUAL, 10)}) |
      size(WIDTH, EQUAL, 30);

  std::vector<Component> right_panel_elements = {
      create_options(state) | size(HEIGHT, EQUAL, 15),
      Container::Vertical(
          {Renderer([] { return text("Outputs") | bold | hcenter; }), create_output_log(state) | flex}) |
          flex | border};

  if (!state.args.disable_chart) {
    right_panel_elements.push_back(Container::Vertical({create_graph(state)}) | size(HEIGHT, EQUAL, 15) | border);
  }

  auto right_panel = Container::Vertical(right_panel_elements) | flex;
  return Container::Horizontal({left_panel, right_panel});
}

}  // namespace rtbot_cli