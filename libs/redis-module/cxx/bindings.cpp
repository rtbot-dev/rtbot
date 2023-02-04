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


// TODO: once we implement the possibility of returning several outputs
// in the same `receive` call, we should return instead a `Vec<Vec<RtBotMessage>>
// where the order correspond to the same order of the output ids reported
// by the pipeline and in case there is no output in this iteration for
// the given output the vector it's simply empty
rust::Vec<RtBotMessage> receiveMessageInPipeline(rust::String id, RtBotMessage rtBotMessage) {
  rust::Vec<RtBotMessage> r;
  auto it = pipelines.find(id);
  if (it != pipelines.end()) {

    auto values_arr = rtBotMessage.values.data();
    auto value = values_arr[0];
    auto timestamp = int(rtBotMessage.timestamp);
    cout << "Sending values to pipeline " << timestamp << ", " << value << endl;
    std::optional<rtbot::Message<double>> result = it->second.receive(Message(timestamp, value));
    std::cout << "result.has_value(): " << result.has_value() << endl;
    if(result.has_value()) {
      std::cout << "(" <<result.value().time <<", " << result.value().value[0] << ")" <<endl;
      // we are copying the value here, in the future we should explore the possibility
      // to pass a pointer to the original message directly to avoid the copy
      auto data = result.value();
      RtBotMessage msg;
      msg.timestamp = data.time;
//      std::copy(v.begin(), v.end(), std::back_inserter(msg.values));
      r.push_back(msg);
    }
  }
  return r;
}

}