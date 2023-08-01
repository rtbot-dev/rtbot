#include <emscripten/bind.h>

#include "rtbot/Message.h"
#include "rtbot/bindings.h"

using namespace emscripten;

namespace emscripten {
namespace internal {

template <typename T, typename Allocator>
struct BindingType<std::vector<T, Allocator>> {
  using ValBinding = BindingType<val>;
  using WireType = ValBinding::WireType;

  static WireType toWireType(const std::vector<T, Allocator> &vec) { return ValBinding::toWireType(val::array(vec)); }

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

EMSCRIPTEN_BINDINGS(RtBot) {
  value_object<rtbot::Message<std::uint64_t, double>>("Message")
      .field("time", &rtbot::Message<std::uint64_t, double>::time)
      .field("value", &rtbot::Message<std::uint64_t, double>::value);

  function("createProgram", &createProgram);
  function("deleteProgram", &deleteProgram);
  function("processMessageDebug", &processMessageDebug);
  function("processMessage", &processMessage);
}
