#ifndef PROGRAM_H
#define PROGRAM_H

#include <map>
#include <memory>
#include <optional>

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

struct Program {
 private:
  map<string, Op_ptr<uint64_t, double>> all_op;  // from id to operator
  string entryOperator;                          // id of the entry operator
  map<string, vector<string>> outputFilter;

 public:
  explicit Program(string const& json_string);

  Program(Program const&) = delete;
  void operator=(Program const&) = delete;
  Program(Program&& other) = default;

  Bytes collect() {
    Bytes bytes;
    for (auto it = this->all_op.begin(); it != this->all_op.end(); ++it) {
      Bytes opBytes = it->second->collect();
      bytes.insert(bytes.end(), opBytes.begin(), opBytes.end());
    }

    return bytes;
  }

  void restore(Bytes const& bytes) {
    Bytes::const_iterator bytes_it = bytes.begin();
    for (auto it = this->all_op.begin(); it != this->all_op.end(); ++it) {
      it->second->restore(bytes_it);
    }
  }

  string debug() {
    string toReturn;
    for (auto it = this->all_op.begin(); it != this->all_op.end(); ++it) {
      toReturn += "\n  " + it->second->debug("");
    }
    return toReturn;
  }

  string getProgramEntryOperatorId() { return entryOperator; }

  vector<string> getProgramEntryPorts() {
    if (this->entryOperator.empty()) throw runtime_error("Entry operator was not defined");
    if (this->all_op.count(this->entryOperator) == 0)
      throw runtime_error("Entry operator " + this->entryOperator + " was not found");

    return this->all_op.at(entryOperator)->getDataInputs();
  }

  map<string, vector<string>> getProgramOutputFilter() { return this->outputFilter; }

  /// return a list of output ports from the output operator that emitted: id, output message
  ProgramMessage<uint64_t, double> receive(const OperatorMessage<uint64_t, double>& messagesMap) {
    ProgramMessage<uint64_t, double> opResults;
    ProgramMessage<uint64_t, double> toReturn;

    opResults = receiveDebug(messagesMap);

    for (auto op = this->outputFilter.begin(); op != this->outputFilter.end(); ++op) {
      if (opResults.count(op->first) > 0) {
        auto outResults = opResults.at(op->first);
        for (int i = 0; i < op->second.size(); i++) {
          if (outResults.count(op->second.at(i)) > 0) {
            OperatorMessage<uint64_t, double> opReturn;
            opReturn.emplace(op->second.at(i), outResults.at(op->second.at(i)));
            if (toReturn.count(op->first) == 0)
              toReturn.emplace(op->first, opReturn);
            else
              Operator<uint64_t, double>::mergeOutput(toReturn[op->first], opReturn);
          }
        }
      }
    }

    if (this->outputFilter.empty()) {
      return opResults;
    } else {
      return toReturn;
    }
  }

  /// return a list of the operator that emitted: id, output message
  ProgramMessage<uint64_t, double> receiveDebug(const OperatorMessage<uint64_t, double>& messagesMap) {
    if (this->all_op.count(this->entryOperator) == 0)
      throw runtime_error("Entry operator " + this->entryOperator + " was not found");

    ProgramMessage<uint64_t, double> opResults;

    for (auto it = messagesMap.begin(); it != messagesMap.end(); ++it) {
      for (int j = 0; j < it->second.size(); j++) {
        this->all_op.at(this->entryOperator)->receiveData(it->second.at(j), it->first);
        auto results = this->all_op.at(this->entryOperator)->executeData();
        if (!results.empty()) {
          Operator<uint64_t, double>::mergeOutput(opResults, results);
        }
      }
    }

    return opResults;
  }

  string getProgram();
};

}  // namespace rtbot

#endif  // PROGRAM_H
