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
  map<string, Op_ptr<uint64_t, double>> all_op;  // from id to operator
  Operator<uint64_t, double>* input;
  Output_opt<uint64_t, double>* output;
  optional<Message<uint64_t, double>> out;

  explicit Program(string const& json_string);

  Program(Program const&) = delete;
  void operator=(Program const&) = delete;

  Program(Program&& other) {
    all_op = move(other.all_op);
    input = move(other.input);
    output = move(other.output);
    out = move(other.out);
    output->out = &out;
  }

  vector<optional<Message<uint64_t, double>>> receive(const Message<uint64_t, double>& msg) {
    out.reset();
    input->receiveData(msg);
    return {out};
  }

  /// return a list of the operator that emit: id, output message
  map<string, map<string, vector<Message<uint64_t, double>>>> receiveDebug(const Message<uint64_t, double>& msg) {
    input->receiveData(msg);
    return input->executeData();
  }

  string getProgram();
};

}  // namespace rtbot

#endif  // PIPELINE_H
