#ifndef RTBOT_CLI_ARGS_H
#define RTBOT_CLI_ARGS_H

#include <cstdint>
#include <cxxopts.hpp>
#include <optional>
#include <string>

namespace rtbot_cli {

enum class OutputFormat { JSON, YAML, RTBOT_DEBUG };

enum class Mode { INTERACTIVE, REPL, BATCH };

struct CLIArguments {
  Mode mode = Mode::INTERACTIVE;
  std::string program_file;
  std::string data_file;
  std::string output_file;
  double scale_t = 1.0;
  double scale_y = 1.0;
  OutputFormat format = OutputFormat::JSON;
  bool debug = false;
  bool disable_chart = false;
  std::string programs_dir = ".";
  std::string csv_dir = ".";
  std::optional<size_t> head;
  std::optional<size_t> tail;

  static CLIArguments parse(int argc, char* argv[]);
  static void print_usage();

 private:
  static cxxopts::Options create_options();
  static Mode determine_mode(const cxxopts::ParseResult& result);
};

class ArgumentException : public std::runtime_error {
 public:
  explicit ArgumentException(const std::string& msg) : std::runtime_error(msg) {}
};

}  // namespace rtbot_cli

#endif  // RTBOT_CLI_ARGS_H