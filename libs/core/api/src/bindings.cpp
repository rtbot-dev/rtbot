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
#include "rtbot/jsonschema.hpp"

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

string createProgram(string const& id, string const& json_program) {
  // first validate it
  string validation = validate(json_program);

  if (nlohmann::json::parse(validation)["valid"])
    return factory.createProgram(id, json_program);
  else
    return validation;
}

string deleteProgram(string const& id) { return to_string(factory.deleteProgram(id)); }

string addToMessageBuffer(const string& apId, const string& portId, Message<uint64_t, double> msg) {
  return to_string(factory.addToMessageBuffer(apId, portId, msg));
}

string processMessageBuffer(const string& apId) {
  auto result = factory.processMessageBuffer(apId);
  return nlohmann::json(result).dump();
}

string processMessageBufferDebug(const string& apId) {
  auto result = factory.processMessageBufferDebug(apId);
  return nlohmann::json(result).dump();
}

string getProgramEntryOperatorId(const string& apId) {
  auto result = factory.getProgramEntryOperatorId(apId);
  return result;
}

string getProgramEntryPorts(const string& apId, const string& entryOperatorId) {
  auto result = factory.getProgramEntryPorts(apId);
  return nlohmann::json(result).dump();
}

string getProgramOutputFilter(const string& apId) {
  auto result = factory.getProgramOutputFilter(apId);
  return nlohmann::json(result).dump();
}

string processMessageMap(const string& id, const map<string, vector<Message<uint64_t, double>>>& messagesMap) {
  auto result = factory.processMessageMap(id, messagesMap);
  return nlohmann::json(result).dump();
}

string processMessageMapDebug(string const& id, const map<string, vector<Message<uint64_t, double>>>& messagesMap) {
  auto result = factory.processMessageMapDebug(id, messagesMap);
  return nlohmann::json(result).dump();
}
