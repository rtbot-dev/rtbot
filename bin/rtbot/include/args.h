#pragma once

#include <cstdint>
#include <string>

namespace rtbot_cli {

struct CLIArguments {
  std::string programs_dir = ".";
  std::string csv_dir = ".";
  double scale_t = 1.0;
  double scale_y = 1.0;

  static CLIArguments parse(int argc, char* argv[]);
  static void print_usage();
};

}  // namespace rtbot_cli