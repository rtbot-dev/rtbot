#include "rtbot/bindings.h"

#include <algorithm>
#include <chrono>
#include <memory>
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
/* using std::chrono::duration; */
/* using std::chrono::duration_cast; */
/* using std::chrono::high_resolution_clock; */
/* using std::chrono::nanoseconds; */

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

Bytes collect(string const& programId) { return factory.collect(programId); }

void restore(string const& programId, Bytes const& bytes) { factory.restore(programId, bytes); }

ProgramMessage<uint64_t, double> processMessageMapNative(string const& programId,
                                                         const OperatorMessage<uint64_t, double>& messagesMap) {
  return factory.processMessageMap(programId, messagesMap);
}

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

string createProgram(string const& programId, string const& json_program) {
  // first validate it
  string validation = validate(json_program);

  if (!nlohmann::json::parse(validation)["valid"])
    return validation;
  else
    return factory.createProgram(programId, json_program);
}

string deleteProgram(string const& programId) { return to_string(factory.deleteProgram(programId)); }

string addToMessageBuffer(const string& programId, const string& portId, unsigned long time, double value) {
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

string processMessageMap(const string& programId, const OperatorMessage<uint64_t, double>& messagesMap) {
  auto result = factory.processMessageMap(programId, messagesMap);
  return nlohmann::json(result).dump();
}

string processMessageMapDebug(string const& programId, const OperatorMessage<uint64_t, double>& messagesMap) {
  auto result = factory.processMessageMapDebug(programId, messagesMap);
  return nlohmann::json(result).dump();
}

// helper function to load the batch into the input buffers
void addBatchToMessageBuffers(string const& programId, vector<uint64_t> times, vector<double> values,
                              vector<string> const& ports) {
  if (times.size() != values.size() || times.size() != ports.size() || values.size() != ports.size())
    throw std::runtime_error("vectors passed to processBatch are not of the same length");

  // fill the message buffers
  for (size_t i = 0; i < times.size(); i++) {
    auto time = times[i];
    auto value = values[i];
    auto portId = ports[i];
    factory.addToMessageBuffer(programId, portId, Message<uint64_t, double>(time, value));
  }
}

string processBatch(string const& programId, vector<uint64_t> times, vector<double> values,
                    vector<string> const& ports) {
  /* auto t1 = high_resolution_clock::now(); */
  addBatchToMessageBuffers(programId, times, values, ports);
  /* auto t2 = high_resolution_clock::now(); */
  /* auto dt1 = duration_cast<nanoseconds>(t2 - t1); */
  /* cout << "[processBatch][" << dt1.count() << " ns] Added " << times.size() << " entries to message buffers" << endl;
   */

  auto result = factory.processMessageBuffer(programId);
  /* auto t3 = high_resolution_clock::now(); */
  /* auto dt2 = duration_cast<nanoseconds>(t3 - t2); */
  /* cout << "[processBatch][" << dt2.count() << " ns] process messages in buffers" << endl; */
  auto json = nlohmann::json(result).dump();
  /* auto t4 = high_resolution_clock::now(); */
  /* auto dt3 = duration_cast<nanoseconds>(t4 - t3); */
  /* cout << "[processBatch][" << dt3.count() << " ns] convert result to json" << endl; */

  return json;
}

string processBatchDebug(string const& programId, vector<uint64_t> times, vector<double> values,
                         vector<string> const& ports) {
  addBatchToMessageBuffers(programId, times, values, ports);
  auto result = factory.processMessageBufferDebug(programId);
  return nlohmann::json(result).dump();
}
