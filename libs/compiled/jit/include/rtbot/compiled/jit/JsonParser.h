#ifndef RTBOT_JIT_JSON_PARSER_H
#define RTBOT_JIT_JSON_PARSER_H

#include <string>

#include "rtbot/compiled/jit/CompiledGraph.h"

namespace rtbot::jit {

// Parse the rtbot Program JSON format into a CompiledGraph.
// Throws std::runtime_error on unknown operator types or missing required fields.
CompiledGraph parse_program_json(const std::string& json_str);

}  // namespace rtbot::jit

#endif  // RTBOT_JIT_JSON_PARSER_H
