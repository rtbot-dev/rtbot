#include "rust/cxx.h"

namespace rtbot {
struct RtBotMessage;
rust::Str createPipeline(rust::Str id, rust::Str program);
rust::Str deletePipeline(rust::Str id);
rust::Vec<RtBotMessage> receiveMessageInPipeline(rust::Str id, ::uint64_t timestamp, rust::Slice<const double> values);
}  // namespace rtbot