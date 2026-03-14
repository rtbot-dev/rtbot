#include "data_loader.h"

#include <cmath>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#include "rtbot/bindings.h"

namespace rtbot_cli {

namespace {

bool has_extension(const std::string& filename, const std::string& extension) {
  if (filename.size() <= extension.size()) {
    return false;
  }
  return filename.compare(filename.size() - extension.size(), extension.size(), extension) == 0;
}

std::string file_stem(const std::string& filename) {
  const size_t dot_pos = filename.find_last_of('.');
  if (dot_pos == std::string::npos || dot_pos == 0) {
    return filename;
  }
  return filename.substr(0, dot_pos);
}

std::string join_path(const std::string& dir_path, const std::string& filename) {
  if (dir_path.empty() || dir_path.back() == '/') {
    return dir_path + filename;
  }
  return dir_path + "/" + filename;
}

std::vector<std::string> list_files_with_extension(const std::string& dir_path, const std::string& extension) {
  std::vector<std::string> files;

  DIR* dir = opendir(dir_path.c_str());
  if (dir == nullptr) {
    return files;
  }

  while (const dirent* entry = readdir(dir)) {
    const std::string filename = entry->d_name;
    if (filename == "." || filename == ".." || !has_extension(filename, extension)) {
      continue;
    }

    const std::string path = join_path(dir_path, filename);
    struct stat info;
    if (stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode)) {
      files.push_back(path);
    }
  }

  closedir(dir);
  return files;
}

}  // namespace

std::vector<ProgramInfo> DataLoader::load_programs(const std::string& dir_path) {
  std::vector<ProgramInfo> programs;

  for (const std::string& path : list_files_with_extension(dir_path, ".json")) {
    const size_t slash_pos = path.find_last_of('/');
    const std::string filename = slash_pos == std::string::npos ? path : path.substr(slash_pos + 1);

    ProgramInfo info;
    info.name = file_stem(filename);
    info.path = path;

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

  return programs;
}

std::vector<std::string> DataLoader::load_csv_files(const std::string& dir_path) {
  return list_files_with_extension(dir_path, ".csv");
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
