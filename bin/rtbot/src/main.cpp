#include <yaml-cpp/yaml.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

#include "args.h"
#include "data_loader.h"
#include "repl.h"
#include "rtbot/bindings.h"
#include "ui_components.h"

struct BatchProgress {
  std::atomic<size_t> processed_records{0};
  std::atomic<bool> finished{false};
  std::atomic<bool> error{false};
  std::string error_message;
  std::mutex error_mutex;
};

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

void write_batch_output(std::ostream& out, const json& batch_json, const std::vector<uint64_t>& times,
                        const std::vector<double>& values, OutputFormat format, bool is_first_batch) {
  for (size_t j = 0; j < times.size(); ++j) {
    if (!is_first_batch || j > 0) {
      out << ",\n";
    }

    json pair;
    pair["in"] = {{"time", times[j]}, {"value", values[j]}};
    pair["out"] = batch_json;

    if (format == OutputFormat::JSON) {
      out << pair.dump(2);
    } else {
      YAML::Node yaml = YAML::Load(pair.dump());
      out << YAML::Dump(yaml);
    }
  }
}

void show_progress_bar(const BatchProgress& progress, size_t total_records) {
  const int bar_width = 50;
  while (!progress.finished && !progress.error) {
    float percentage = static_cast<float>(progress.processed_records) / total_records;
    int pos = static_cast<int>(bar_width * percentage);

    std::cout << "\r[";
    for (int i = 0; i < bar_width; ++i) {
      if (i < pos)
        std::cout << "=";
      else if (i == pos)
        std::cout << ">";
      else
        std::cout << " ";
    }
    std::cout << "] " << int(percentage * 100.0) << "% " << progress.processed_records << "/" << total_records;
    std::cout.flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::cout << std::endl;
}

void process_data(std::ostream& out, const CSVData& data, const std::string& program_id, const std::string& entry_id,
                  const CLIArguments& args, BatchProgress& progress) {
  try {
    const size_t BATCH_SIZE = 1000;
    std::vector<std::string> ports(data.times.size(), entry_id);
    bool is_first_batch = true;

    out << "[\n";

    for (size_t i = 0; i < data.times.size(); i += BATCH_SIZE) {
      size_t batch_end = std::min(i + BATCH_SIZE, data.times.size());
      std::vector<uint64_t> batch_times(data.times.begin() + i, data.times.begin() + batch_end);
      std::vector<double> batch_values(data.values.begin() + i, data.values.begin() + batch_end);
      std::vector<std::string> batch_ports(ports.begin() + i, ports.begin() + batch_end);

      std::string batch_result = args.debug
                                     ? rtbot::process_batch_debug(program_id, batch_times, batch_values, batch_ports)
                                     : rtbot::process_batch(program_id, batch_times, batch_values, batch_ports);

      write_batch_output(out, json::parse(batch_result), batch_times, batch_values, args.format, is_first_batch);

      is_first_batch = false;
      progress.processed_records += batch_end - i;
    }

    out << "\n]" << std::endl;
    progress.finished = true;

  } catch (const std::exception& e) {
    std::lock_guard<std::mutex> lock(progress.error_mutex);
    progress.error = true;
    progress.error_message = e.what();
  }
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

  std::ofstream output_file;
  std::ostream& out = args.output_file.empty() ? std::cout : (output_file.open(args.output_file), output_file);

  BatchProgress progress;

  // Start processing thread
  std::thread process_thread(process_data, std::ref(out), std::ref(data), program_id, entry_id, std::ref(args),
                             std::ref(progress));

  // Show progress bar in main thread
  show_progress_bar(progress, data.times.size());

  // Wait for processing to complete
  process_thread.join();

  if (progress.error) {
    std::lock_guard<std::mutex> lock(progress.error_mutex);
    throw std::runtime_error("Processing error: " + progress.error_message);
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