#include "rtbot/bindings.h"
#include "rtbot/FactoryOp.h"

#include<nlohmann/json.hpp>

rtbot::FactoryOp factory;

namespace rtbot {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Message<>,time,value);

}

std::string createPipeline(std::string const& id, std::string const& json_program)
{
    return factory.createPipeline(id,json_program);
}

std::string deletePipeline(std::string const& id)
{
    return factory.deletePipeline(id);
}

std::vector<std::optional<rtbot::Message<>>> receiveMessageInPipeline(const std::string &id, rtbot::Message<> const& msg)
{
    return factory.receiveMessageInPipeline(id,msg);
}

std::string receiveMessageInPipelineDebug(std::string const& id, rtbot::Message<> const& msg)
{
    auto result = factory.receiveMessageInPipelineDebug(id,msg);
    return nlohmann::json(result).dump();
}
