#include "rtbot/bindings.h"
#include "rtbot/FactoryOp.h"
#include "rtbot/Pipeline.h"
#include <map>
#include <vector>
#include <string>

using namespace std;

map<string, Pipeline> pipelines;

int createPipeline(const char* id, const char* program) {
  pipelines.emplace(id,FactoryOp::createPipeline(program));
  return 0;
}

int deletePipeline(const char* id) {
  pipelines.erase(id);
  return 0;
}

int sendMessageToPipeline(const char* id, long long timestamp, double* values, uint size) {
    auto it=pipelines.find(id);
    if( it !=pipelines.end() ) {
        vector<double> v(values, values + size);
        it->second.receive(Message(timestamp, v));
        return 0;
    }
    // indicate failure
    return 1;
}
