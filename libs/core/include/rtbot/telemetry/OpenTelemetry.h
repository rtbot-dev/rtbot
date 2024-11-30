#ifndef RTBOT_OPENTELEMETRY_H
#define RTBOT_OPENTELEMETRY_H

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

#ifdef RTBOT_INSTRUMENTATION
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span.h>
#endif

namespace rtbot {

class OpenTelemetry {
 public:
  static void initialize(const std::string& service_name) {
    std::cout << "Initializing telemetry for service: " << service_name << std::endl;
  }

  static void record_message(const std::string& op_id, const std::string& type_name, std::unique_ptr<BaseMessage> msg) {
    std::cout << "    " << msg->to_string() << " -> " << type_name << "(" << op_id << ")" << std::endl;
  }

  static void record_message_sent(const std::string& from_id, const std::string& from_type_name,
                                  const std::string& to_id, const std::string& to_type_name,
                                  std::unique_ptr<BaseMessage> msg) {
    std::cout << " . " << from_type_name << "(" << from_id << ") -> " << to_type_name << "(" << to_id
              << "): " << msg->to_string() << std::endl;
  }

  static void record_operator_output(const std::string& op_id, const std::string& type_name, size_t port,
                                     std::unique_ptr<BaseMessage> msg) {
    std::cout << "      " << type_name << "(" << op_id << "):" << port << " -> " << msg->to_string() << std::endl;
  }

  static void record_queue_size(uint64_t size) {
    std::cout << "Queue size: " << size << " at: " << get_timestamp() << std::endl;
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
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return std::to_string(ms);
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