#include "data_loader.h"

#include <cmath>
#include <fstream>
#include <sstream>

#include "rtbot/bindings.h"

namespace rtbot_cli {

std::vector<ProgramInfo> DataLoader::load_programs(const std::string& dir_path) {
  // Existing implementation remains the same
  std::vector<ProgramInfo> programs;
  namespace fs = std::filesystem;

  for (const auto& entry : fs::directory_iterator(dir_path)) {
    if (entry.path().extension() == ".json") {
      ProgramInfo info;
      info.name = entry.path().stem().string();
      info.path = entry.path().string();

      std::ifstream file(info.path);
      if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        info.content = buffer.str();

        auto validation_result = nlohmann::json::parse(rtbot::validate_program(info.content));
        info.is_valid = validation_result["valid"].get<bool>();

        if (info.is_valid) {
          programs.push_back(info);
        }
      }
    }
  }
  return programs;
}

std::vector<std::string> DataLoader::load_csv_files(const std::string& dir_path) {
  // Existing implementation remains the same
  std::vector<std::string> files;
  namespace fs = std::filesystem;

  for (const auto& entry : fs::directory_iterator(dir_path)) {
    if (entry.path().extension() == ".csv") {
      files.push_back(entry.path().string());
    }
  }
  return files;
}

CSVData DataLoader::load_csv(const std::string& path, const CLIArguments& args) {
  CSVData data;
  std::ifstream file(path);
  std::string line;

  // First pass to load all data
  while (std::getline(file, line)) {
    if (line.empty()) continue;

    std::stringstream ss(line);
    std::string time_str, value_str;

    if (std::getline(ss, time_str, ',') && std::getline(ss, value_str, ',')) {
      try {
        double raw_time = std::stod(time_str);
        double raw_value = std::stod(value_str);

        uint64_t timestamp = static_cast<uint64_t>(raw_time * args.scale_t);
        double scaled_value = raw_value * args.scale_y;

        data.times.push_back(timestamp);
        data.values.push_back(scaled_value);
      } catch (const std::exception&) {
        continue;
      }
    }
  }

  // Apply head/tail filters if specified
  if (args.head.has_value() && args.head.value() < data.times.size()) {
    data.times.resize(args.head.value());
    data.values.resize(args.head.value());
  } else if (args.tail.has_value() && args.tail.value() < data.times.size()) {
    size_t start = data.times.size() - args.tail.value();
    std::vector<uint64_t> new_times(data.times.begin() + start, data.times.end());
    std::vector<double> new_values(data.values.begin() + start, data.values.end());
    data.times = std::move(new_times);
    data.values = std::move(new_values);
  }

  return data;
}

}  // namespace rtbot_cli