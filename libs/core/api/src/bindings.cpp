#include "rtbot/bindings.h"

#include <algorithm>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <ostream>

#include "rtbot/FactoryOp.h"

// notice that this file is generated
// and bazel will guarantee it will be there
// at build time

#if __has_include(<jsonschema>)
#include "rtbot/jsonschema.hpp"
#define have_jsonschema 1
#endif

rtbot::FactoryOp factory;

using json = nlohmann::json;
using nlohmann::json_schema::json_validator;

namespace rtbot {

template <class T, class V>
void to_json(json& j, const Message<T, V>& p) {
  j = json{{"time", p.time}, {"value", p.value}};
}

template <class T, class V>
void from_json(const json& j, Message<T, V>& p) {
  j.at("time").get_to(p.time);
  j.at("value").get_to(p.value);
}

}  // namespace rtbot

using namespace rtbot;

#ifdef have_jsonschema

string validateOperator(string const& type, string const& json_op) {
  json_validator validator(nullptr, nlohmann::json_schema::default_string_format_check);  // create validator

  try {
    // look for the schema for the give operator type
    optional<nlohmann::json> schema = std::nullopt;
    for (auto it : rtbot_schema["properties"]["operators"]["items"]["oneOf"]) {
      if (type.compare(it["properties"]["type"]["enum"][0]) == 0) {
        schema = std::optional{it};
      }
    }

    if (schema)
      validator.set_root_schema(schema.value());
    else
      return nlohmann::json({{"valid", false}, {"error", "Unknown operator type: " + type}}).dump();

  } catch (const exception& e) {
    cout << "Unable to set the rtbot schema as root: " << e.what() << "\n";
    return nlohmann::json({{"valid", false}, {"error", e.what()}}).dump();
  }

  try {
    validator.validate(
        nlohmann::json::parse(json_op));  // validate the document - uses the default throwing error-handler
  } catch (const exception& e) {
    return nlohmann::json({{"valid", false}, {"error", e.what()}}).dump();
  }

  return nlohmann::json({{"valid", true}}).dump();
}

string validate(string const& json_program) {
  json_validator validator(nullptr, nlohmann::json_schema::default_string_format_check);  // create validator

  try {
    validator.set_root_schema(rtbot_schema);  // insert root-schema
  } catch (const std::exception& e) {
    cout << "Unable to set the rtbot schema as root: " << e.what() << "\n";
    return nlohmann::json({{"valid", false}, {"error", e.what()}}).dump();
  }

  try {
    validator.validate(
        nlohmann::json::parse(json_program));  // validate the document - uses the default throwing error-handler
  } catch (const exception& e) {
    return nlohmann::json({{"valid", false}, {"error", e.what()}}).dump();
  }

  return nlohmann::json({{"valid", true}}).dump();
}

#endif

string createProgram(string const& programId, string const& json_program) {
  // first validate it
#ifdef have_jsonschema
  string validation = validate(json_program);

  if (!nlohmann::json::parse(validation)["valid"])
      return validation;
  else
#endif
    return factory.createProgram(programId, json_program);
}

string deleteProgram(string const& programId) { return to_string(factory.deleteProgram(programId)); }

string addToMessageBuffer(const string& programId, const string& portId, unsigned long long time, double value) {
  return to_string(factory.addToMessageBuffer(programId, portId, Message<uint64_t, double>(time, value)));
}

string processMessageBuffer(const string& programId) {
  auto result = factory.processMessageBuffer(programId);
  return nlohmann::json(result).dump();
}

string processMessageBufferDebug(const string& programId) {
  auto result = factory.processMessageBufferDebug(programId);
  return nlohmann::json(result).dump();
}

string getProgramEntryOperatorId(const string& programId) {
  auto result = factory.getProgramEntryOperatorId(programId);
  return result;
}

string getProgramEntryPorts(const string& programId) {
  auto result = factory.getProgramEntryPorts(programId);
  return nlohmann::json(result).dump();
}

string getProgramOutputFilter(const string& programId) {
  auto result = factory.getProgramOutputFilter(programId);
  return nlohmann::json(result).dump();
}

string processMessageMap(const string& programId, const map<string, vector<Message<uint64_t, double>>>& messagesMap) {
  auto result = factory.processMessageMap(programId, messagesMap);
  return nlohmann::json(result).dump();
}

string processMessageMapDebug(string const& programId,
                              const map<string, vector<Message<uint64_t, double>>>& messagesMap) {
  auto result = factory.processMessageMapDebug(programId, messagesMap);
  return nlohmann::json(result).dump();
}
