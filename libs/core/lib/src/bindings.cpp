#include "rtbot/bindings.h"
#include "rtbot/FactoryOp.h"

rtbot::FactoryOp factory;

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
