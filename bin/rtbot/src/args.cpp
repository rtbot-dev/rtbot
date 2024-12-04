#include "args.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace rtbot_cli {

CLIArguments CLIArguments::parse(int argc, char* argv[]) {
  CLIArguments args;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "--programs-dir" && i + 1 < argc) {
      args.programs_dir = argv[++i];
    } else if (arg == "--csv-dir" && i + 1 < argc) {
      args.csv_dir = argv[++i];
    } else if (arg == "--scale-t" && i + 1 < argc) {
      try {
        args.scale_t = std::stod(argv[++i]);
        if (args.scale_t <= 0) {
          throw std::runtime_error("Time scale must be positive");
        }
      } catch (const std::exception& e) {
        std::cerr << "Error parsing scale-t: " << e.what() << std::endl;
        print_usage();
        exit(1);
      }
    } else if (arg == "--scale-y" && i + 1 < argc) {
      try {
        args.scale_y = std::stod(argv[++i]);
        if (args.scale_y == 0) {
          throw std::runtime_error("Y scale cannot be zero");
        }
      } catch (const std::exception& e) {
        std::cerr << "Error parsing scale-y: " << e.what() << std::endl;
        print_usage();
        exit(1);
      }
    } else if (arg == "--no-ts-chart") {  // Add this block
      args.disable_chart = true;
    } else if (arg == "--help" || arg == "-h") {
      print_usage();
      exit(0);
    }
  }

  return args;
}

void CLIArguments::print_usage() {
  std::cout << "Usage: rtbot-cli [OPTIONS]\n"
            << "Options:\n"
            << "  --programs-dir DIR   Directory containing program JSON files (default: .)\n"
            << "  --csv-dir DIR        Directory containing CSV data files (default: .)\n"
            << "  --scale-t VALUE      Scale time values by VALUE (default: 1.0)\n"
            << "  --scale-y VALUE      Scale y-axis values by VALUE (default: 1.0)\n"
            << "  --no-ts-chart        Disable time series chart display\n"  // Add this line
            << "  -h, --help           Print this help message\n";
}

}  // namespace rtbot_cli