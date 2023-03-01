#ifndef FACTORYOP_H
#define FACTORYOP_H

#include "Pipeline.h"


#include <map>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <string>

namespace rtbot {

class FactoryOp
{
    std::map<string, Pipeline> pipelines;
public:

    FactoryOp();

    struct SerializerOp {
        function<Op_ptr<>(string)> read;
        function<string(const Op_ptr<>&)> write;
    };

    static map<string, SerializerOp>& op_registry()
    {
        static map<string, SerializerOp> ops;
        return ops;
    }

    static Op_ptr<> readOp(std::string const& json_string);
    static std::string writeOp(Op_ptr<> const& op);
    static Pipeline createPipeline(std::string const& json_string) { return Pipeline(json_string); }

    std::string createPipeline(std::string const& id, std::string const& json_program);

    std::string deletePipeline(std::string const& id) { pipelines.erase(id); return ""; }

    std::vector<std::optional<Message<>>> receiveMessageInPipeline(std::string const& id, Message<> const& msg) {
      auto it = pipelines.find(id);
      if (it == pipelines.end()) return {};
      return it->second.receive(msg);
    }
};


}

#endif // FACTORYOP_H
