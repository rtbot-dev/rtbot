#include <yaml-cpp/yaml.h>

#include <fstream>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

#include "args.h"
#include "data_loader.h"
#include "repl.h"
#include "rtbot/bindings.h"
#include "ui_components.h"

using namespace rtbot_cli;
using namespace ftxui;
using json = nlohmann::json;

void handle_interactive_mode(const CLIArguments& args) {
  auto screen = ScreenInteractive::Fullscreen();

  std::string title = "RTBot Interactive Mode";
  if (args.scale_t != 1.0) {
    title += " [t×" + std::to_string(args.scale_t) + "]";
  }
  if (args.scale_y != 1.0) {
    title += " [y×" + std::to_string(args.scale_y) + "]";
  }

  AppState state;
  state.args = args;
  state.programs = DataLoader::load_programs(args.programs_dir);
  state.csv_files = DataLoader::load_csv_files(args.csv_dir);

  auto container = UIComponents::create_main_layout(state);
  auto renderer = Renderer(container, [&] {
    return vbox({text(title) | bold | color(Color::Blue) | hcenter, separator(), container->Render() | flex});
  });

  screen.Loop(renderer);
}

void handle_repl_mode(const CLIArguments& args) {
  // Read program file
  std::ifstream program_file(args.program_file);
  if (!program_file.is_open()) {
    throw std::runtime_error("Failed to open program file: " + args.program_file);
  }

  std::string program_content((std::istreambuf_iterator<char>(program_file)), std::istreambuf_iterator<char>());

  // Create program
  std::string program_id = "repl_program";
  auto result = rtbot::create_program(program_id, program_content);
  if (!result.empty()) {
    throw std::runtime_error("Failed to create program: " + result);
  }

  // Start REPL
  REPL repl(program_id, args);
  repl.run();
}

void handle_batch_mode(const CLIArguments& args) {
  // Read program file
  std::ifstream program_file(args.program_file);
  if (!program_file.is_open()) {
    throw std::runtime_error("Failed to open program file: " + args.program_file);
  }

  std::string program_content((std::istreambuf_iterator<char>(program_file)), std::istreambuf_iterator<char>());

  // Create program
  std::string program_id = "batch_program";
  auto result = rtbot::create_program(program_id, program_content);
  if (!result.empty()) {
    throw std::runtime_error("Failed to create program: " + result);
  }

  // Load and process data
  auto data = DataLoader::load_csv(args.data_file, args);
  auto entry_id = rtbot::get_program_entry_operator_id(program_id);
  std::vector<std::string> ports(data.times.size(), entry_id);

  json output_array = json::array();

  // Process in batches to avoid memory issues
  const size_t BATCH_SIZE = 1000;
  for (size_t i = 0; i < data.times.size(); i += BATCH_SIZE) {
    size_t batch_end = std::min(i + BATCH_SIZE, data.times.size());
    std::vector<uint64_t> batch_times(data.times.begin() + i, data.times.begin() + batch_end);
    std::vector<double> batch_values(data.values.begin() + i, data.values.begin() + batch_end);
    std::vector<std::string> batch_ports(ports.begin() + i, ports.begin() + batch_end);

    std::string batch_result;
    if (args.debug) {
      batch_result = rtbot::process_batch_debug(program_id, batch_times, batch_values, batch_ports);
    } else {
      batch_result = rtbot::process_batch(program_id, batch_times, batch_values, batch_ports);
    }

    // Create input-output pairs
    json batch_json = json::parse(batch_result);
    for (size_t j = 0; j < batch_times.size(); ++j) {
      json pair;
      pair["in"] = {{"time", batch_times[j]}, {"value", batch_values[j]}};
      pair["out"] = batch_json;
      output_array.push_back(pair);
    }
  }

  // Write output
  std::string output_str;
  if (args.format == OutputFormat::JSON) {
    output_str = output_array.dump(2);
  } else {
    YAML::Node yaml;
    yaml = YAML::Load(output_array.dump());
    output_str = YAML::Dump(yaml);
  }

  if (!args.output_file.empty()) {
    std::ofstream output_file(args.output_file);
    output_file << output_str;
  } else {
    std::cout << output_str << std::endl;
  }
}

int main(int argc, char* argv[]) {
  try {
    auto args = CLIArguments::parse(argc, argv);

    switch (args.mode) {
      case Mode::INTERACTIVE:
        handle_interactive_mode(args);
        break;
      case Mode::REPL:
        handle_repl_mode(args);
        break;
      case Mode::BATCH:
        handle_batch_mode(args);
        break;
    }

    return 0;
  } catch (const ArgumentException& e) {
    std::cerr << "Argument error: " << e.what() << std::endl;
    CLIArguments::print_usage();
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}