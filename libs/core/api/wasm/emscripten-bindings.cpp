#include <emscripten/bind.h>
#include "rtbot/bindings.h"
#include "rtbot/Message.h"

using namespace emscripten;

namespace emscripten {
namespace internal {

template <typename T, typename Allocator>
struct BindingType<std::vector<T, Allocator>> {
  using ValBinding = BindingType<val>;
  using WireType = ValBinding::WireType;

  static WireType toWireType(const std::vector<T, Allocator> &vec) {
    return ValBinding::toWireType(val::array(vec));
  }

  static std::vector<T, Allocator> fromWireType(WireType value) {
    return vecFromJSArray<T>(ValBinding::fromWireType(value));
  }
};

template <typename T>
struct TypeID<T,
              typename std::enable_if_t<std::is_same<
                  typename Canonicalized<T>::type,
                  std::vector<typename Canonicalized<T>::type::value_type,
                              typename Canonicalized<T>::type::allocator_type>>::value>> {
  static constexpr TYPEID get() { return TypeID<val>::get(); }
};

}  // namespace internal
}  // namespace emscripten

EMSCRIPTEN_BINDINGS(RtBot) {
  value_object<rtbot::Message<double>>("Message")
      .field("time", &rtbot::Message<double>::time)
      .field("value", &rtbot::Message<double>::value);

    function("createPipeline", &createPipeline);
    function("deletePipeline", &deletePipeline);
    function("receiveMessageInPipelineDebug", &receiveMessageInPipelineDebug);
    function("receiveMessageInPipeline", &receiveMessageInPipeline);
}