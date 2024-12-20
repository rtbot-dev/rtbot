#pragma once

#include <filesystem>

#include "args.h"
#include "types.h"

namespace rtbot_cli {

class DataLoader {
 public:
  static std::vector<ProgramInfo> load_programs(const std::string& dir_path);
  static std::vector<std::string> load_csv_files(const std::string& dir_path);
  static CSVData load_csv(const std::string& path, const CLIArguments& args);
};

}  // namespace rtbot_cli