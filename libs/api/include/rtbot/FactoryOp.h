#ifndef FACTORYOP_H
#define FACTORYOP_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Program.h"

namespace rtbot {

using namespace std;

class FactoryOp {
  map<string, Program> programs;
  ProgramMessage<uint64_t, double> messageBuffer;

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
    auto from_string = [](string const& prog) { return make_unique<Op>(Format::parse(prog)); };
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

  static Program createProgram(string const& json_string) { return Program(json_string); }

  Bytes serialize(string const& programId);

  string createProgram(string const& id, Bytes const& bytes);

  string createProgram(string const& id, string const& json_program);

  bool deleteProgram(string const& id) { return (programs.erase(id) == 1) ? true : false; }

  bool addToMessageBuffer(const string& apId, const string& portId, Message<uint64_t, double> msg) {
    if (this->programs.count(apId) == 0) throw runtime_error("Program " + apId + " was not found");
    this->messageBuffer[apId][portId].push_back(msg);
    return true;
  }

  ProgramMessage<uint64_t, double> processMessageBuffer(const string& apId) {
    if (this->programs.count(apId) == 0) throw runtime_error("Program " + apId + " was not found");
    if (this->messageBuffer.count(apId) > 0) {
      if (!this->messageBuffer.at(apId).empty()) {
        auto result = processMessageMap(apId, this->messageBuffer.at(apId));
        this->messageBuffer.at(apId).clear();
        this->messageBuffer.erase(apId);
        if (!result.empty()) return result;
      }
    }
    return {};
  }

  ProgramMessage<uint64_t, double> processMessageBufferDebug(const string& apId) {
    if (this->programs.count(apId) == 0) throw runtime_error("Program " + apId + " was not found");
    if (this->messageBuffer.count(apId) > 0) {
      if (!this->messageBuffer.at(apId).empty()) {
        auto result = processMessageMapDebug(apId, this->messageBuffer.at(apId));
        this->messageBuffer.at(apId).clear();
        this->messageBuffer.erase(apId);
        if (!result.empty()) return result;
      }
    }
    return {};
  }

  string getProgramEntryOperatorId(const string& apId) {
    if (this->programs.count(apId) == 0) throw runtime_error("Program " + apId + " was not found");
    return this->programs.at(apId).getProgramEntryOperatorId();
  }

  vector<string> getProgramEntryPorts(const string& apId) {
    if (this->programs.count(apId) == 0) throw runtime_error("Program " + apId + " was not found");
    return this->programs.at(apId).getProgramEntryPorts();
  }

  map<string, vector<string>> getProgramOutputFilter(const string& apId) {
    if (this->programs.count(apId) == 0) throw runtime_error("Program " + apId + " was not found");
    return this->programs.at(apId).getProgramOutputFilter();
  }

  ProgramMessage<uint64_t, double> processMessageMap(string const& apId,
                                                     const OperatorMessage<uint64_t, double>& messagesMap) {
    auto it = programs.find(apId);
    if (it == programs.end()) return {};
    return it->second.receive(messagesMap);
  }

  ProgramMessage<uint64_t, double> processMessageMapDebug(string const& apId,
                                                          const OperatorMessage<uint64_t, double>& messagesMap) {
    auto it = programs.find(apId);
    if (it == programs.end()) return {};
    return it->second.receiveDebug(messagesMap);
  }
};

}  // namespace rtbot

#endif  // FACTORYOP_H
