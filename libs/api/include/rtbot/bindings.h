#ifndef RTBOT_BINDINGS_H
#define RTBOT_BINDINGS_H

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "rtbot/Message.h"

namespace rtbot {

using json = nlohmann::json;
using Bytes = std::vector<uint8_t>;
using PortMsgBatch = std::vector<std::unique_ptr<BaseMessage>>;
using OperatorMsgBatch = std::unordered_map<std::string, PortMsgBatch>;
using ProgramMsgBatch = std::unordered_map<std::string, OperatorMsgBatch>;

// JSON serialization for message batches
void to_json(json& j, const ProgramMsgBatch& batch);

// Program serialization and lifecycle
Bytes serialize_program(const std::string& program_id);
void create_program_from_bytes(const std::string& program_id, const Bytes& bytes);
std::string create_program(const std::string& program_id, const std::string& json_program);
std::string delete_program(const std::string& program_id);
std::string get_program_entry_operator_id(const std::string& program_id);

// Validation functions
std::string validate_program(const std::string& json_program);
std::string validate_operator(const std::string& type, const std::string& json_op);

// Message handling
std::string add_to_message_buffer(const std::string& program_id, const std::string& port_id, uint64_t time,
                                  double value);
std::string process_message_buffer(const std::string& program_id);
std::string process_message_buffer_debug(const std::string& program_id);

// Batch operations
void add_batch_to_message_buffers(const std::string& program_id, const std::vector<uint64_t>& times,
                                  const std::vector<double>& values, const std::vector<std::string>& ports);
std::string process_batch(const std::string& program_id, const std::vector<uint64_t>& times,
                          const std::vector<double>& values, const std::vector<std::string>& ports);
std::string process_batch_debug(const std::string& program_id, const std::vector<uint64_t>& times,
                                const std::vector<double>& values, const std::vector<std::string>& ports);

// Pretty printing
std::string pretty_print(const std::string& json_output);
std::string pretty_print(const ProgramMsgBatch& batch);
std::string pretty_print_validation_error(const std::string& validation_result);
}  // namespace rtbot

#endif  // RTBOT_BINDINGS_H