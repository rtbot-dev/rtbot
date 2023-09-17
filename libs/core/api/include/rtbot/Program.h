#ifndef PIPELINE_H
#define PIPELINE_H

#include <map>
#include <memory>
#include <optional>

#include "rtbot/Operator.h"
#include "rtbot/Output.h"

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

  Program(Program&& other) {
    all_op = move(other.all_op);
    entryOperator = move(other.entryOperator);
    outputFilter = move(other.outputFilter);
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
  map<string, map<string, vector<Message<uint64_t, double>>>> receive(
      const map<string, vector<Message<uint64_t, double>>>& messagesMap) {
    map<string, map<string, vector<Message<uint64_t, double>>>> opResults;
    map<string, map<string, vector<Message<uint64_t, double>>>> toReturn;

    opResults = receiveDebug(messagesMap);

    for (auto op = this->outputFilter.begin(); op != this->outputFilter.end(); ++op) {
      if (opResults.count(op->first) > 0) {
        auto outResults = opResults.at(op->first);
        for (int i = 0; i < op->second.size(); i++) {
          if (outResults.count(op->second.at(i)) > 0) {
            map<string, vector<Message<uint64_t, double>>> opReturn;
            opReturn.emplace(op->second.at(i), outResults.at(op->second.at(i)));
            toReturn.emplace(op->first, opReturn);
          }
        }
      }
    }

    if (this->outputFilter.empty())
      return opResults;
    else
      return toReturn;
  }

  /// return a list of the operator that emitted: id, output message
  map<string, map<string, vector<Message<uint64_t, double>>>> receiveDebug(
      const map<string, vector<Message<uint64_t, double>>>& messagesMap) {
    if (this->all_op.count(this->entryOperator) == 0)
      throw runtime_error("Entry operator " + this->entryOperator + " was not found");

    map<string, map<string, vector<Message<uint64_t, double>>>> opResults;

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

#endif  // PIPELINE_H
