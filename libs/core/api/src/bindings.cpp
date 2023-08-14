#include "rtbot/bindings.h"

#include <stdio.h>

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

std::string validateOperator(std::string const& type, std::string const& json_op) {
  json_validator validator(nullptr, nlohmann::json_schema::default_string_format_check);  // create validator

  try {
    // look for the schema for the give operator type
    std::optional<nlohmann::json> schema = std::nullopt;
    for (auto it : rtbot_schema["properties"]["operators"]["items"]["oneOf"]) {
      if (type.compare(it["properties"]["type"]["enum"][0]) == 0) {
        schema = std::optional{it};
      }
    }

    if (schema)
      validator.set_root_schema(schema.value());
    else
      return nlohmann::json({{"valid", false}, {"error", "Unknown operator type: " + type}}).dump();

  } catch (const std::exception& e) {
    std::cout << "Unable to set the rtbot schema as root: " << e.what() << "\n";
    return nlohmann::json({{"valid", false}, {"error", e.what()}}).dump();
  }

  try {
    validator.validate(
        nlohmann::json::parse(json_op));  // validate the document - uses the default throwing error-handler
  } catch (const std::exception& e) {
    return nlohmann::json({{"valid", false}, {"error", e.what()}}).dump();
  }

  return nlohmann::json({{"valid", true}}).dump();
}

std::string validate(std::string const& json_program) {
  json_validator validator(nullptr, nlohmann::json_schema::default_string_format_check);  // create validator

  try {
    validator.set_root_schema(rtbot_schema);  // insert root-schema
  } catch (const std::exception& e) {
    std::cout << "Unable to set the rtbot schema as root: " << e.what() << "\n";
    return nlohmann::json({{"valid", false}, {"error", e.what()}}).dump();
  }

  try {
    validator.validate(
        nlohmann::json::parse(json_program));  // validate the document - uses the default throwing error-handler
  } catch (const std::exception& e) {
    return nlohmann::json({{"valid", false}, {"error", e.what()}}).dump();
  }

  return nlohmann::json({{"valid", true}}).dump();
}

std::string createProgram(std::string const& id, std::string const& json_program) {
  // first validate it
  std::string validation = validate(json_program);

  if (nlohmann::json::parse(validation)["valid"])
    return factory.createProgram(id, json_program);
  else
    return validation;
}

std::string deleteProgram(std::string const& id) { return factory.deleteProgram(id); }

std::vector<std::optional<rtbot::Message<std::uint64_t, double>>> processMessage(
    const std::string& id, rtbot::Message<std::uint64_t, double> const& msg) {
  return factory.processMessage(id, msg);
}

std::string processMessageDebug(std::string const& id, unsigned long time, double value) {
  rtbot::Message msg = rtbot::Message<std::uint64_t, double>(time, value);

  auto result = factory.processMessageDebug(id, msg);
  return nlohmann::json(result).dump();
}
