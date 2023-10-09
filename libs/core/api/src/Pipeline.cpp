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

void to_json(json& j, const OpConnection& p) {
  j = json{{"from", p.from}, {"to", p.to}, {"toPort", p.toPort}, {"fromPort", p.fromPort}};
}

void from_json(const json& j, OpConnection& p) {
  p = OpConnection(j["from"].get<string>(), j["to"].get<string>(), j.value("toPort", ""), j.value("fromPort", ""));
}

template<class T, class V>
Pipeline<T,V>::Pipeline(string const& id, const string& json_prog)
    : Operator<T, V>(id)
{
    //TODO: check the sanity of the json
    auto j = json::parse(json_prog);

    for (const json& x : j.at("operators")) {
      pair<map<string, Op_ptr<uint64_t, double>>::iterator, bool> it;
      it = all_op.emplace(x["id"], FactoryOp::readOp(x.dump().c_str()));
    }

    // connections, save the used id,key
    set<string> all_id_key;
    for (const OpConnection x : j.at("connections")) {
      if (all_op.at(x.from)->connect(all_op.at(x.to).get(), x.fromPort, x.toPort) == nullptr)
        throw std::runtime_error("Couldn't connect " + x.from + " to " + x.to + " from output port " + x.fromPort +
                                 " to input port " + x.toPort);
      all_id_key.emplace(x.to+":"+x.toPort);
    }

    // build the inputs
    for(auto [id,op] : all_op)
        for(auto [key,input] : op->dataInputs) // map<string, Input> Op::dataInputs;
           if (auto it=all_id_key.find(id+":"+key); it!=all_id_key.end())
               inputs.emplace(id+":"+key, op);

    //build the outputs
}

}
