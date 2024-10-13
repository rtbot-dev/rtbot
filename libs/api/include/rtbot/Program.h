#ifndef PROGRAM_H
#define PROGRAM_H

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

struct Program {
 private:
  string program_json;
  map<string, Op_ptr<uint64_t, double>> all_op;  // from id to operator
  string entryOperator;                          // id of the entry operator
  map<string, vector<string>> outputFilter;

 public:
  explicit Program(string const& json_string);
  explicit Program(Bytes const& bytes) {
    // create an iterator
    Bytes::const_iterator bytes_it = bytes.begin();
    // read the size of the program json
    uint64_t size = *reinterpret_cast<const uint64_t*>(&(*bytes_it));
    bytes_it += sizeof(size);
    // read the program json
    program_json = string(bytes_it, bytes_it + size);
    bytes_it += size;

    // init the program
    init();

    // load the state of the operators
    while (bytes_it != bytes.end()) {
      // read the size of the operator id
      size = *reinterpret_cast<const uint64_t*>(&(*bytes_it));
      bytes_it += sizeof(size);
      string opId(bytes_it, bytes_it + size);
      bytes_it += size;

      // read the operator state
      all_op.at(opId)->restore(bytes_it);
    }
  }

  Program(Program const&) = delete;
  void operator=(Program const&) = delete;
  Program(Program&& other) = default;

  void init();

  Bytes serialize();

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
