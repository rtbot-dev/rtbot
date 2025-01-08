#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "rtbot/Message.h"
#include "rtbot/OperatorJson.h"
#include "rtbot/bindings.h"

using namespace emscripten;
using json = nlohmann::json;
using timestamp_t = rtbot::timestamp_t;

namespace emscripten {
namespace internal {

// Vector binding type for converting between C++ vectors and JavaScript arrays
template <typename T, typename Allocator>
struct BindingType<std::vector<T, Allocator>> {
  using ValBinding = BindingType<val>;
  using WireType = ValBinding::WireType;

  static WireType toWireType(const std::vector<T, Allocator>& vec) { return ValBinding::toWireType(val::array(vec)); }

  static std::vector<T, Allocator> fromWireType(WireType value) {
    return vecFromJSArray<T>(ValBinding::fromWireType(value));
  }
};

// TypeID specialization for vectors to ensure proper type handling
template <typename T>
struct TypeID<
    T, typename std::enable_if_t<std::is_same<typename Canonicalized<T>::type,
                                              std::vector<typename Canonicalized<T>::type::value_type,
                                                          typename Canonicalized<T>::type::allocator_type>>::value>> {
  static constexpr TYPEID get() { return TypeID<val>::get(); }
};

}  // namespace internal
}  // namespace emscripten

// Helper function to process batches with 32-bit timestamps
std::string processBatch32(const std::string& programId, const std::vector<uint32_t>& times32,
                           const std::vector<double>& values, const std::vector<std::string>& ports) {
  // Convert 32-bit timestamps to 64-bit
  std::vector<uint64_t> times(times32.begin(), times32.end());
  return rtbot::process_batch(programId, times, values, ports);
}

// Debug version of batch processing
std::string processBatch32Debug(const std::string& programId, const std::vector<uint32_t>& times32,
                                const std::vector<double>& values, const std::vector<std::string>& ports) {
  std::vector<uint64_t> times(times32.begin(), times32.end());
  return rtbot::process_batch_debug(programId, times, values, ports);
}

// Helper to add a single message to the buffer
std::string addMessage(const std::string& programId, const std::string& portId, uint32_t time, double value) {
  return rtbot::add_to_message_buffer(programId, portId, static_cast<uint64_t>(time), value);
}

namespace {
// Helper functions for message creation and access
timestamp_t getMessage_getTime(const rtbot::Message<rtbot::NumberData>& msg) { return msg.time; }

void getMessage_setTime(rtbot::Message<rtbot::NumberData>& msg, timestamp_t t) { msg.time = t; }

const rtbot::NumberData& getMessage_getData(const rtbot::Message<rtbot::NumberData>& msg) { return msg.data; }

void getMessage_setData(rtbot::Message<rtbot::NumberData>& msg, const rtbot::NumberData& data) { msg.data = data; }
}  // namespace

EMSCRIPTEN_BINDINGS(RtBot) {
  // Register NumberData type first
  value_object<rtbot::NumberData>("NumberData").field("value", &rtbot::NumberData::value);

  // Register Message type with manual accessors to avoid base class issues
  class_<rtbot::Message<rtbot::NumberData>>("Message")
      .constructor<timestamp_t, const rtbot::NumberData&>()
      .property("time", &getMessage_getTime, &getMessage_setTime)
      .property("data", &getMessage_getData, &getMessage_setData);

  // Core program management functions
  function("createProgram", &rtbot::create_program);
  function("deleteProgram", &rtbot::delete_program);
  function("validateProgram", &rtbot::validate_program);
  function("validateOperator", &rtbot::validate_operator);

  // Message handling
  function("addToMessageBuffer", &addMessage);
  function("processMessageBuffer", &rtbot::process_message_buffer);
  function("processMessageBufferDebug", &rtbot::process_message_buffer_debug);

  // Program information
  function("getProgramEntryOperatorId", &rtbot::get_program_entry_operator_id);
  function("getProgramEntryPorts", optional_override([](const std::string& programId) -> std::string {
             try {
               auto entry_id = rtbot::get_program_entry_operator_id(programId);
               if (entry_id.empty()) return "[]";

               // By default, return ["i1"] as the entry port
               json ports = {"i1"};
               return ports.dump();
             } catch (const std::exception& e) {
               return "[]";
             }
           }));

  // Batch processing
  function("processBatch", &processBatch32);
  function("processBatchDebug", &processBatch32Debug);

  // State management
  // TODO: Serialize and deserialize program state
}