#include "bindings.h"
#include "libs/redis-module/src/cxx_bindings.rs.h"
#include <string>
#include <vector>
#include <map>

#include "rtbot/FactoryOp.h"
#include "rtbot/Pipeline.h"


using namespace std;
using namespace rtbot;

map<rust::String, rtbot::Pipeline> pipelines;

namespace rtbot {

rust::String createPipeline(rust::String id, rust::String program) {
  try
  {
    auto parsedProgram = nlohmann::json::parse(program);
    pipelines.emplace(id, FactoryOp::createPipeline(parsedProgram));
    return rust::String("");
  }
  catch (nlohmann::json::parse_error& e)
  {
    // output exception information
    std::cout << "message: " << e.what() << '\n'
              << "exception id: " << e.id << '\n'
              << "byte position of error: " << e.byte << std::endl;
    return rust::String("Unable to parse program: "s + e.what());
  }
}

rust::String deletePipeline(rust::String id) {
  pipelines.erase(id);
  // TODO: do we have to do something else here?
  return rust::String("");
}


rust::Vec<RtBotMessage> receiveMessageInPipeline(rust::String id, ::uint64_t timestamp, rust::Slice<const double> values) {
  rust::Vec<RtBotMessage> r;
  auto it = pipelines.find(id);
  if (it != pipelines.end()) {
    auto values_arr = values.data();
    vector<double> v(values_arr, values_arr + values.length());
    auto result = it->second.receive(Message(timestamp, v));
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