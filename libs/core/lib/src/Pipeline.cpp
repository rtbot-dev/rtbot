#include "rtbot/Pipeline.h"
#include "rtbot/FactoryOp.h"

#include<nlohmann/json.hpp>

namespace rtbot {

Pipeline::Pipeline(const std::string &json_string)
{
    auto json = nlohmann::json::parse(json_string);
    for(const nlohmann::json& x : json.at("operators")) {
        auto it = all_op.emplace(x["id"], FactoryOp::readOp(x.dump().c_str()) );
        if (x.at("type")=="Input")
            input=it.first->second.get();
        else if (x.at("type")=="Output") {
            output=dynamic_cast<decltype(output)>(it.first->second.get());
            output->out=&out;
        }
    }

    // connections
    for(const nlohmann::json& x : json.at("connections"))
        connect(all_op.at(x.at("from")).get(), all_op.at(x.at("to")).get());

}

}
