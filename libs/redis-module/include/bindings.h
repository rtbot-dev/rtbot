#include "rust/cxx.h"

namespace rtbot {
struct RtBotMessage;
rust::String createPipeline(rust::String id, rust::String program);
rust::String deletePipeline(rust::String id);
rust::Vec<RtBotMessage> receiveMessageInPipeline(rust::String id, ::uint64_t timestamp, rust::Slice<const double> values);
}  // namespace rtbot