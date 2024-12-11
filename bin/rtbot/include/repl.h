#ifndef RTBOT_CLI_REPL_H
#define RTBOT_CLI_REPL_H

#include <nlohmann/json.hpp>
#include <string>

#include "args.h"
#include "types.h"

namespace rtbot_cli {

class REPL {
 public:
  REPL(const std::string& program_id, const CLIArguments& args);
  ~REPL();
  void run();

 private:
  std::string program_id_;
  std::string program_json_;
  CLIArguments args_;
  std::string entry_operator_;

  CSVData loaded_data_;
  size_t current_data_index_{0};
  std::string loaded_filename_;

  void process_n_messages(size_t n);
  void handle_load_csv(const std::string& args);

  void print_prompt() const;
  bool process_command(const std::string& input);
  void process_message(const nlohmann::json& msg);
  void print_help() const;
  void print_result(const std::string& result) const;
  std::string colorize_json(const nlohmann::json& j, int indent) const;
  void process_csv_input(const std::string& input);
  std::string get_prompt() const;
  void show_scale_warning();
};

}  // namespace rtbot_cli

#endif  // RTBOT_CLI_REPL_H