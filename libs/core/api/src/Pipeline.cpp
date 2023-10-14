#include "rtbot/Pipeline.h"
#include "rtbot/FactoryOp.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace rtbot {

using namespace std;

struct OpConnection {
  string from;
  string to;
  string toPort = "";
  string fromPort = "";
  OpConnection() = default;
  OpConnection(string from, string to, string toPort, string fromPort) {
    this->from = from;
    this->to = to;
    this->toPort = toPort;
    this->fromPort = fromPort;
  }
};

inline void to_json(json& j, const OpConnection& p) {
  j = json{{"from", p.from}, {"to", p.to}, {"toPort", p.toPort}, {"fromPort", p.fromPort}};
}

inline void from_json(const json& j, OpConnection& p) {
  p = OpConnection(j["from"].get<string>(), j["to"].get<string>(), j.value("toPort", ""), j.value("fromPort", ""));
}

template<class T, class V>
Pipeline<T,V>::Pipeline(string const& id, const string& json_prog_)
    : Operator<T, V>(id)
    , json_prog(json_prog_)
{
    //TODO: check the sanity of the json
    auto j = json::parse(json_prog_);

    for (const json& x : j.at("operators")) {
      pair<map<string, Op_ptr<uint64_t, double>>::iterator, bool> it;
      it = all_op.emplace(x["id"], FactoryOp::readOp(x.dump().c_str()));
    }

    // make connections, save the used in/out ports
    set<string> used_input;
    set<string> used_output;
    for (const json& jx : j.at("connections")) {
        OpConnection x=jx;
        Op_ptr<T,V> &op1=all_op.at(x.from);
        Op_ptr<T,V> &op2=all_op.at(x.to);
        if (op1->connect(op2.get(), x.fromPort, x.toPort) == nullptr)
            throw std::runtime_error("Couldn't connect " + x.from + " to " + x.to + " from output port " + x.fromPort +
                                     " to input port " + x.toPort);

        // save the used input ports
        if (op2->dataInputs.size()==1 && op2->controlInputs.size()==0)
            used_input.emplace(x.to+":"+op2->dataInputs.begin()->first);
        else
            used_input.emplace(x.to+":"+x.toPort);

        // save the used output ports
        if (op1->outputIds.size()==1)
            used_output.emplace(x.from+":"+ *op1->outputIds.begin());
        else
            used_output.emplace(x.from+":"+x.fromPort);
    }

    // build the Pipeline input and output ports: the unused ones
    for(const auto& [id,op] : all_op) {
        for(const auto& [key,_] : op->dataInputs)
           if (auto it=used_input.find(id+":"+key); it==used_input.end())
               inputs.emplace(id+":"+key, op.get());

        for(const auto& key : op->outputIds)
           if (auto it=used_output.find(id+":"+key); it==used_output.end())
               outputs.emplace(id+":"+key, op.get());
    }
}


// force the instantiation
template struct Pipeline<uint64_t, double>;

}
