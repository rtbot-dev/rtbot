#include "rust/cxx.h"

namespace rtbot {
struct RtBotMessage;
rust::String create_pipeline(rust::String id, rust::String program);
rust::String delete_pipeline(rust::String id);
rust::Vec<RtBotMessage> receive_message_in_pipeline(rust::String id, RtBotMessage rtBotMessage);
}  // namespace rtbot