#ifndef RTBOT_OPENTELEMETRY_H
#define RTBOT_OPENTELEMETRY_H

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#ifdef RTBOT_INSTRUMENTATION
// #include <opentelemetry/metrics/meter.h>
// #include <opentelemetry/metrics/provider.h>
// #include <opentelemetry/nostd/shared_ptr.h>
// #include <opentelemetry/trace/provider.h>
// #include <opentelemetry/trace/span.h>
#endif

namespace rtbot {

class OpenTelemetry {
 public:
  static void initialize(const std::string& service_name) {
    std::cout << "\033[1;35m[TELEMETRY]\033[0m Initializing service: " << service_name << std::endl;
  }

  static void record_message(const std::string& op_id, const std::string& type_name, std::unique_ptr<BaseMessage> msg) {
    std::cout << "\033[90m" << get_timestamp() << "\033[0m "
              << "\033[1;36m[MSG]\033[0m "
              << "\033[97m" << msg->to_string() << "\033[0m "
              << "\033[90m→\033[0m "
              << "\033[1;33m" << type_name << "(" << op_id << ")\033[0m" << std::endl;
  }

  static void record_message_sent(const std::string& from_id, const std::string& from_type_name,
                                  const std::string& to_id, const std::string& to_type_name,
                                  std::unique_ptr<BaseMessage> msg) {
    std::cout << "\033[90m" << get_timestamp() << "\033[0m "
              << "\033[1;32m[FLOW]\033[0m "
              << "\033[1;33m" << from_type_name << "(" << from_id << ")\033[0m "
              << "\033[1;36m──►\033[0m "
              << "\033[1;33m" << to_type_name << "(" << to_id << ")\033[0m "
              << "\033[90m|\033[0m "
              << "\033[97m" << msg->to_string() << "\033[0m" << std::endl;
  }

  static void record_operator_output(const std::string& op_id, const std::string& type_name, size_t port,
                                     std::unique_ptr<BaseMessage> msg) {
    std::cout << "\033[90m" << get_timestamp() << "\033[0m "
              << "\033[1;34m[OUT]\033[0m "
              << "\033[1;33m" << type_name << "(" << op_id << ")\033[0m"
              << "\033[36m:" << port << "\033[0m "
              << "\033[90m►\033[0m "
              << "\033[97m" << msg->to_string() << "\033[0m" << std::endl;
  }

  static void record_queue_size(uint64_t size) {
    std::cout << "\033[90m" << get_timestamp() << "\033[0m "
              << "\033[1;35m[QUEUE]\033[0m Size: "
              << "\033[1;37m" << size << "\033[0m" << std::endl;
  }

  static void start_span(const std::string& name) {
    // std::cout << "Starting span: " << name << " at: " << get_timestamp() << std::endl;
  }

  static void end_span() {
    // std::cout << "Ending span at: " << get_timestamp() << std::endl;
  }

  static void add_attribute(const std::string& key, const std::string& value) {
    // std::cout << "Attribute: " << key << " = " << value << std::endl;
  }

 private:
  static std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S");
    return ss.str();
  }
};

#ifdef RTBOT_INSTRUMENTATION
#define RTBOT_INIT_TELEMETRY(service) rtbot::OpenTelemetry::initialize(service)
#define RTBOT_RECORD_MESSAGE(id, type_name, msg) rtbot::OpenTelemetry::record_message(id, type_name, msg)
#define RTBOT_RECORD_MESSAGE_SENT(from_id, from_type_name, to_id, to_type_name, msg) \
  rtbot::OpenTelemetry::record_message_sent(from_id, from_type_name, to_id, to_type_name, msg)
#define RTBOT_RECORD_OPERATOR_OUTPUT(op_id, type_name, port, msg) \
  rtbot::OpenTelemetry::record_operator_output(op_id, type_name, port, msg)

#define RTBOT_RECORD_QUEUE_SIZE(size) rtbot::OpenTelemetry::record_queue_size(size)
#define RTBOT_START_SPAN(name) rtbot::OpenTelemetry::start_span(name)
#define RTBOT_END_SPAN() rtbot::OpenTelemetry::end_span()
#define RTBOT_ADD_ATTRIBUTE(key, value) rtbot::OpenTelemetry::add_attribute(key, value)
#else
#define RTBOT_INIT_TELEMETRY(service)
#define RTBOT_RECORD_MESSAGE(id, type_name, msg)
#define RTBOT_RECORD_MESSAGE_SENT(from_id, from_type_name, to_id, to_type_name, msg)
#define RTBOT_RECORD_OPERATOR_OUTPUT(op_id, type_name, port, msg)
#define RTBOT_RECORD_QUEUE_SIZE(size)
#define RTBOT_START_SPAN(name)
#define RTBOT_END_SPAN()
#define RTBOT_ADD_ATTRIBUTE(key, value)
#endif

class SpanScope {
 public:
  SpanScope(const std::string& name) { RTBOT_START_SPAN(name); }
  ~SpanScope() { RTBOT_END_SPAN(); }
};

}  // namespace rtbot

#endif  // RTBOT_OPENTELEMETRY_H