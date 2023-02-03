#include "bindings.h"
#include "libs/redis-module/src/cxx_bindings.rs.h"
#include <string>
#include <vector>
#include <map>

#include "rtbot/FactoryOp.h"
#include "rtbot/Pipeline.h"


using namespace std;
using namespace rtbot;

map<rust::Str, rtbot::Pipeline*> pipelines;

namespace rtbot {

rust::Str createPipeline(rust::Str  id, rust::Str program) {
  auto parsedProgram = nlohmann::json::parse(program.data());
  auto p = FactoryOp::createPipeline(parsedProgram);

  pipelines[id] = &p;
  return rust::Str("");
}

rust::Str deletePipeline(rust::Str id) {
  pipelines.erase(id);
  // TODO: do we have to do something else here?
  return rust::Str("");
}


rust::Vec<RtBotMessage> receiveMessageInPipeline(rust::Str id, ::uint64_t timestamp, rust::Slice<const double> values) {
  rust::Vec<RtBotMessage> r;
  if (pipelines[id]) {
    auto values_arr = values.data();
    vector<double> v(values_arr, values_arr + values.length());
    auto result = pipelines[id]->receive(Message(timestamp, v));
    if(result) {
      // we are copying the value here, in the future we should explore the possibility
      // to pass a pointer to the original message directly to avoid the copy
      auto data = result.value();
      RtBotMessage msg;
      msg.timestamp = data.time;
      r.push_back(msg);
    }
  }
  return r;
}

}