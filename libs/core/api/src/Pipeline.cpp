#include "rtbot/Pipeline.h"

#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"
#include "rtbot/Input.h"

namespace rtbot {

struct OpConnection {
  string from;
  string to;
  string toPort = "";
  string fromPort = "";
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(OpConnection, from, to, toPort, fromPort);

Pipeline::Pipeline(const std::string& json_string) {
  auto json = nlohmann::json::parse(json_string);

  for (const nlohmann::json& x : json.at("operators")) {
    std::pair<std::map<std::string, rtbot::Op_ptr<std::uint64_t, double>>::iterator, bool> it;

    it = all_op.emplace(x["id"], FactoryOp::readOp(x.dump().c_str()));

    if (x.at("type") == "Input") {
      input = it.first->second.get();
    } else if (x.at("type") == "Output") {
      output = dynamic_cast<decltype(output)>(it.first->second.get());
      output->out = &out;
    }
  }

  // connections
  for (const OpConnection x : json.at("connections")) {
    if (all_op.at(x.from)->connect(all_op.at(x.to).get(), x.fromPort, x.toPort) == nullptr)
      throw std::runtime_error("Couldn't connect " + x.from + " to " + x.to + "from output port " + x.fromPort +
                               " to input port " + x.toPort);
  }
}

std::string Pipeline::getProgram() {
  std::string out = "";
  for (auto it = this->all_op.begin(); it != this->all_op.end(); ++it) {
    out = out + (!out.empty() ? "," : "") + FactoryOp::writeOp(it->second);
  }
  return "[" + out + "]";
}

}  // namespace rtbot