#include "cxx_bindings.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "libs/rtbot-rs/src/cxx_bindings.rs.h"
#include "rtbot/bindings.h"

using namespace std;
using namespace rtbot;

namespace rtbot {

rust::String create_pipeline(rust::String id, rust::String program) {
  auto result = createPipeline(id.c_str(), program.c_str());
  return rust::String(result);
}

rust::String delete_pipeline(rust::String id) {
  auto result = deletePipeline(id.c_str());
  return rust::String(result);
}


// TODO: once we implement the possibility of returning several outputs
// in the same `receive` call, we should return instead a `Vec<Vec<RtBotMessage>>
// where the order correspond to the same order of the output ids reported
// by the pipeline and in case there is no output in this iteration for
// the given output the vector it's simply empty
rust::Vec<RtBotMessage> receive_message_in_pipeline(rust::String id, RtBotMessage rtBotMessage) {
  rust::Vec<RtBotMessage> r;
  auto t = rtBotMessage.timestamp;
  auto v = rtBotMessage.values[0];
  auto result = receiveMessageInPipeline(id.c_str(), Message<std::uint64_t,double>(t, v));

  if(result[0].has_value()) {
    auto data = result[0].value();
    RtBotMessage p;
    p.timestamp = data.time;
    std::copy(data.value.begin(), data.value.end(), std::back_inserter(p.values));
    r.push_back(p);
  }

  return r;
}

}