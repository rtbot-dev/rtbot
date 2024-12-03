#ifndef RTBOT_LOGGER_H
#define RTBOT_LOGGER_H

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace rtbot {

class Logger {
 public:
  enum class Level { INFO, DBG, ERROR };

  static Logger& instance() {
    static Logger instance;
    return instance;
  }

  template <typename... Args>
  void log(Level level, const char* file, int line, Args&&... args) {
#ifdef RTBOT_INSTRUMENTATION
    std::stringstream ss;
    std::string filename = std::filesystem::path(file).filename().string();

    // Timestamp in yellow
    ss << "\033[33m" << get_timestamp() << "\033[0m ";

    // Level indicator with unique colors per level
    ss << get_level_color(level) << get_level_string(level) << "\033[0m ";

    // Filename:line in magenta
    ss << "\033[1;35m[" << filename << ":" << line << "]\033[0m ";

    // Message in bright white
    ss << "\033[97m";
    (ss << ... << std::forward<Args>(args));

    // Reset colors and flush
    ss << "\033[0m" << std::flush;
    std::cout << ss.str() << std::endl;
#endif
  }

 private:
  Logger() = default;

  std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S");
    return ss.str();
  }

  const char* get_level_color(Level level) {
    switch (level) {
      case Level::INFO:
        return "\033[1;32m";  // Bright Green
      case Level::DBG:
        return "\033[1;36m";  // Bright Cyan
      case Level::ERROR:
        return "\033[1;31m";  // Bright Red
      default:
        return "\033[0m";
    }
  }

  const char* get_level_string(Level level) {
    switch (level) {
      case Level::INFO:
        return "[INFO]";
      case Level::DBG:
        return "[DEBUG]";
      case Level::ERROR:
        return "[ERROR]";
      default:
        return "[UNKNOWN]";
    }
  }

  const char* get_level_symbol(Level level) {
    switch (level) {
      case Level::INFO:
        return "\033[1;32m●\033[0m";  // Green dot
      case Level::DBG:
        return "\033[1;36m◆\033[0m";  // Cyan diamond
      case Level::ERROR:
        return "\033[1;31m✖\033[0m";  // Red cross
      default:
        return "○";
    }
  }
};

#define RTBOT_LOG_INFO(...) rtbot::Logger::instance().log(rtbot::Logger::Level::INFO, __FILE__, __LINE__, __VA_ARGS__)

#define RTBOT_LOG_DEBUG(...) rtbot::Logger::instance().log(rtbot::Logger::Level::DBG, __FILE__, __LINE__, __VA_ARGS__)

#define RTBOT_LOG_ERROR(...) rtbot::Logger::instance().log(rtbot::Logger::Level::ERROR, __FILE__, __LINE__, __VA_ARGS__)

}  // namespace rtbot

#endif  // RTBOT_LOGGER_H