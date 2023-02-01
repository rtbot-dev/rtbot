#include "rtbot/bindings.h"
#include "rtbot/FactoryOp.h"
#include "rtbot/Pipeline.h"
#include <map>
#include <vector>

using namespace std;
map<const char*, Pipeline *> pipelines;

int createPipeline(const char* id, const char* program) {
  nlohmann::json parsedProgram = nlohmann::json::parse(program);
  Pipeline p = FactoryOp::createPipeline(parsedProgram);
  pipelines[id] = &p;
  return 0;
}

int deletePipeline(const char* id) {
  pipelines.erase(id);
  // TODO: do we have to do something else here?
  return 0;
}

int sendMessageToPipeline(const char* id, long long timestamp, double* values, uint size) {
  if(pipelines[id]) {
     vector<double> v(values, values + size);
    pipelines[id]->receive(Message(timestamp, v));
    return 0;
  }
  // indicate failure
  return 1;
}