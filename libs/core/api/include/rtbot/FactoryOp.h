#ifndef FACTORYOP_H
#define FACTORYOP_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Pipeline.h"

namespace rtbot {

using namespace std;

class FactoryOp {
  map<string, Pipeline> pipelines;

 public:
  FactoryOp();

  struct SerializerOp {
    function<Op_ptr<uint64_t, double>(string)> from_string;
    function<string(const Op_ptr<uint64_t, double>&)> to_string;
    function<string()> to_string_default;
  };

  static map<string, SerializerOp>& op_registry() {
    static map<string, SerializerOp> ops;
    return ops;
  }

  template <class Op, class Format>
  static void op_registry_add() {
    auto from_string = [](string const& prog) { return std::make_unique<Op>(Format::parse(prog)); };
    auto to_string = [](Op_ptr<uint64_t, double> const& op) {
      string type = op->typeName();
      auto obj = Format(*dynamic_cast<Op*>(op.get()));
      obj["type"] = type;
      return obj.dump();
    };
    auto to_string_default = []() {
      Op op;
      string type = op.typeName();
      auto obj = Format();
      obj["type"] = type;
      return obj.dump();
    };

    op_registry()[Op().typeName()] = SerializerOp{from_string, to_string, to_string_default};
  }

  static Op_ptr<uint64_t, double> readOp(string const& json_string);
  static string writeOp(Op_ptr<uint64_t, double> const& op);
  static Pipeline createPipeline(string const& json_string) { return Pipeline(json_string); }

  string createPipeline(string const& id, string const& json_program);

  string deletePipeline(string const& id) {
    pipelines.erase(id);
    return "";
  }

  vector<optional<Message<uint64_t, double>>> receiveMessageInPipeline(string const& id,
                                                                       Message<uint64_t, double> const& msg) {
    auto it = pipelines.find(id);
    if (it == pipelines.end()) return {};
    return it->second.receive(msg);
  }

  map<string, map<string, vector<Message<uint64_t, double>>>> receiveMessageInPipelineDebug(
      string const& id, Message<uint64_t, double> const& msg) {
    auto it = pipelines.find(id);
    if (it == pipelines.end()) return {};
    return it->second.receiveDebug(msg);
  }
};

}  // namespace rtbot

#endif  // FACTORYOP_H
