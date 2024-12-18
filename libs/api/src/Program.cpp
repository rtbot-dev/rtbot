#include "rtbot/Program.h"

#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"
#include "rtbot/Input.h"

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

Program::Program(const string& json_string) {
  program_json = json_string;
  init();
}

void Program::init() {
  auto j = json::parse(program_json);
  this->entryOperator = j.value("entryOperator", "");

  if (this->entryOperator.empty()) throw runtime_error("Entry operator was not specified");

  try {
    this->outputFilter = j.at("output").get<map<string, vector<string>>>();
  } catch (...) {
  }

  for (const json& x : j.at("operators")) {
    pair<map<string, Op_ptr<uint64_t, double>>::iterator, bool> it;
    it = all_op.emplace(x["id"], FactoryOp::readOp(x.dump().c_str()));
  }

  // connections
  for (const OpConnection x : j.at("connections")) {
    if (all_op.at(x.from)->connect(all_op.at(x.to).get(), x.fromPort, x.toPort) == nullptr)
      throw std::runtime_error("Couldn't connect " + x.from + " to " + x.to + " from output port " + x.fromPort +
                               " to input port " + x.toPort);
  }

  if (this->all_op.count(this->entryOperator) == 0) {
    this->all_op.clear();
    this->entryOperator.clear();
    this->outputFilter.clear();
    runtime_error("Entry operator was not found");
  }

  if (this->all_op.at(this->entryOperator)->hasControlInputs()) {
    this->all_op.clear();
    this->entryOperator.clear();
    this->outputFilter.clear();
    runtime_error("Entry operator can not have control inputs");
  }
}

Bytes Program::serialize() {
  Bytes bytes;

  // save the program json definition, starting with the size of the string
  uint64_t size = program_json.size();
  bytes.insert(bytes.end(), reinterpret_cast<const unsigned char*>(&size),
               reinterpret_cast<const unsigned char*>(&size) + sizeof(size));
  bytes.insert(bytes.end(), program_json.begin(), program_json.end());

  // save the state of the operators
  for (auto& [opId, op] : all_op) {
    // save the size of the operator id
    size = opId.size();
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char*>(&size),
                 reinterpret_cast<const unsigned char*>(&size) + sizeof(size));
    bytes.insert(bytes.end(), opId.begin(), opId.end());

    // save the operator state
    Bytes opBytes = op->collect();
    bytes.insert(bytes.end(), opBytes.begin(), opBytes.end());
  }

  return bytes;
}

string Program::getProgram() {
  string out = "";
  for (auto it = this->all_op.begin(); it != this->all_op.end(); ++it) {
    out = out + (!out.empty() ? "," : "") + FactoryOp::writeOp(it->second);
  }
  return "[" + out + "]";
}

}  // namespace rtbot
