#ifndef FACTORYOP_H
#define FACTORYOP_H

#include "Pipeline.h"


#include <map>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace rtbot {

class FactoryOp
{
public:
    static Op_ptr createOp(nlohmann::json const& json);
    static Pipeline createPipeline(nlohmann::json const& json);
};


}

#endif // FACTORYOP_H
