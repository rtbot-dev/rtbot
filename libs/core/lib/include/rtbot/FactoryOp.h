#ifndef FACTORYOP_H
#define FACTORYOP_H

#include "Pipeline.h"


#include <map>
#include <functional>
#include <memory>

namespace rtbot {

class FactoryOp
{
public:
    static Op_ptr createOp(const char json_string[]);
    static Pipeline createPipeline(const char json_string[]) { return Pipeline(json_string); }
};


}

#endif // FACTORYOP_H
