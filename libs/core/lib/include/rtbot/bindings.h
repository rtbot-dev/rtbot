#include <string>
#include "Pipeline.h"

using namespace rtbot;

int createPipeline(const char* id, const char* program);
int deletePipeline(const char* id);
int sendMessageToPipeline(const char* id, long long timestamp, double* values, uint size);