#include "args.h"

#include <filesystem>
#include <iostream>

namespace rtbot_cli {

cxxopts::Options CLIArguments::create_options() {
  cxxopts::Options options("rtbot", "RTBot Command Line Interface");

  options.add_options()("h,help", "Print help")("program", "Program JSON file", cxxopts::value<std::string>())(
      "data", "Input data CSV file", cxxopts::value<std::string>())("o,output", "Output file for batch processing",
                                                                    cxxopts::value<std::string>())(
      "st,scale-t", "Scale time values", cxxopts::value<double>()->default_value("1.0"))(
      "sy,scale-y", "Scale y-axis values", cxxopts::value<double>()->default_value("1.0"))(
      "f,format", "Output format (json/yaml)", cxxopts::value<std::string>()->default_value("json"))(
      "d,debug", "Enable debug mode (show all operator outputs)", cxxopts::value<bool>()->default_value("false"))(
      "no-chart", "Disable time series chart in interactive mode", cxxopts::value<bool>()->default_value("false"))(
      "programs-dir", "Directory containing program JSON files", cxxopts::value<std::string>()->default_value("."))(
      "csv-dir", "Directory containing CSV data files", cxxopts::value<std::string>()->default_value("."))(
      "head", "Process only first N records", cxxopts::value<size_t>())(
      "tail", "Process only last N records", cxxopts::value<size_t>())("interactive", "Start in interactive mode");

  return options;
}

Mode CLIArguments::determine_mode(const cxxopts::ParseResult& result) {
  if (result.count("interactive")) {
    return Mode::INTERACTIVE;
  }

  bool has_program = result.count("program");
  bool has_data = result.count("data");

  if (has_program && has_data) {
    return Mode::BATCH;
  } else if (has_program) {
    return Mode::REPL;
  }

  return Mode::INTERACTIVE;
}

CLIArguments CLIArguments::parse(int argc, char* argv[]) {
  auto options = create_options();
  options.parse_positional({"program", "data"});

  try {
    auto result = options.parse(argc, argv);

    if (result.count("help")) {
      std::cout << options.help() << std::endl;
      exit(0);
    }

    CLIArguments args;
    args.mode = determine_mode(result);

    // Parse common options
    args.scale_t = result["scale-t"].as<double>();
    args.scale_y = result["scale-y"].as<double>();
    args.debug = result["debug"].as<bool>();
    args.disable_chart = result["no-chart"].as<bool>();
    args.programs_dir = result["programs-dir"].as<std::string>();
    args.csv_dir = result["csv-dir"].as<std::string>();
    if (result.count("head")) {
      args.head = result["head"].as<size_t>();
    }
    if (result.count("tail")) {
      args.tail = result["tail"].as<size_t>();
    }
    // Parse format
    std::string format = result["format"].as<std::string>();
    if (format == "json") {
      args.format = OutputFormat::JSON;
    } else if (format == "yaml") {
      args.format = OutputFormat::YAML;
    } else {
      args.format = OutputFormat::RTBOT_DEBUG;
    }

    // Handle mode-specific requirements
    if (args.mode != Mode::INTERACTIVE) {
      if (!result.count("program")) {
        throw ArgumentException("Program file is required");
      }
      args.program_file = result["program"].as<std::string>();

      if (!std::filesystem::exists(args.program_file)) {
        throw ArgumentException("Program file does not exist: " + args.program_file);
      }
    }

    if (args.mode == Mode::BATCH) {
      if (!result.count("data")) {
        throw ArgumentException("Data file is required for batch mode");
      }
      args.data_file = result["data"].as<std::string>();

      if (!std::filesystem::exists(args.data_file)) {
        throw ArgumentException("Data file does not exist: " + args.data_file);
      }
    }

    if (result.count("output")) {
      args.output_file = result["output"].as<std::string>();
    }

    return args;

  } catch (const cxxopts::exceptions::parsing& e) {
    throw ArgumentException(std::string("Error parsing arguments: ") + e.what());
  }
}

void CLIArguments::print_usage() { std::cout << create_options().help() << std::endl; }

}  // namespace rtbot_cli