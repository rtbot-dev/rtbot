#include <emscripten/bind.h>

#include "RtBot.pb.h"
#include "rtbot/Message.h"
#include "rtbot/bindings.h"

using namespace emscripten;

namespace emscripten {
namespace internal {

template <typename T, typename Allocator>
struct BindingType<std::vector<T, Allocator>> {
  using ValBinding = BindingType<val>;
  using WireType = ValBinding::WireType;

  static WireType toWireType(const std::vector<T, Allocator>& vec) { return ValBinding::toWireType(val::array(vec)); }

  static std::vector<T, Allocator> fromWireType(WireType value) {
    return vecFromJSArray<T>(ValBinding::fromWireType(value));
  }
};

template <typename T>
struct TypeID<
    T, typename std::enable_if_t<std::is_same<typename Canonicalized<T>::type,
                                              std::vector<typename Canonicalized<T>::type::value_type,
                                                          typename Canonicalized<T>::type::allocator_type>>::value>> {
  static constexpr TYPEID get() { return TypeID<val>::get(); }
};

}  // namespace internal
}  // namespace emscripten

void test(rtbot::api::proto::Input const& input) { std::cout << "Input: " << input.DebugString() << std::endl; }

string processBatch32(string const& programId, vector<uint32_t> times32, vector<double> values,
                      vector<string> const& ports) {
  // translate passed 32 bit timestamp into 64 bit internal type
  vector<uint64_t> times(times32.begin(), times32.end());
  return processBatch(programId, times, values, ports);
}

string processBatch32Debug(string const& programId, vector<uint32_t> times32, vector<double> values,
                           vector<string> const& ports) {
  // translate passed 32 bit timestamp into 64 bit internal type
  vector<uint64_t> times(times32.begin(), times32.end());
  return processBatchDebug(programId, times, values, ports);
}

EMSCRIPTEN_BINDINGS(RtBot) {
  value_object<rtbot::Message<std::uint64_t, double>>("Message")
      .field("time", &rtbot::Message<std::uint64_t, double>::time)
      .field("value", &rtbot::Message<std::uint64_t, double>::value);

  emscripten::function("validate", &validate);
  emscripten::function("validateOperator", &validateOperator);

  emscripten::function("createProgram", &createProgram);
  emscripten::function("deleteProgram", &deleteProgram);

  emscripten::function("addToMessageBuffer", &addToMessageBuffer);
  emscripten::function("processMessageBuffer", &processMessageBuffer);
  emscripten::function("processMessageBufferDebug", &processMessageBufferDebug);

  emscripten::function("getProgramEntryOperatorId", &getProgramEntryOperatorId);
  emscripten::function("getProgramEntryPorts", &getProgramEntryPorts);
  emscripten::function("getProgramOutputFilter", &getProgramOutputFilter);

  emscripten::function("processBatch", &processBatch32);
  emscripten::function("processBatchDebug", &processBatch32Debug);
}
