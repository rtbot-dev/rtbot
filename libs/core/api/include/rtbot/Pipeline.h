#ifndef PIPELINE_H
#define PIPELINE_H

#include <map>
#include <memory>
#include <optional>

#include "rtbot/Operator.h"
#include "rtbot/Output.h"

namespace rtbot {

struct Pipeline {
  std::map<std::string, Op_ptr<double>> all_op;  // from id to operator
  Operator<double>* input;
  Output_opt<double>* output;
  std::optional<Message<double>> out;

  explicit Pipeline(std::string const& json_string);

  Pipeline(Pipeline const&) = delete;
  void operator=(Pipeline const&) = delete;

  Pipeline(Pipeline&& other) {
    all_op = std::move(other.all_op);
    input = std::move(other.input);
    output = std::move(other.output);
    out = std::move(other.out);
    output->out = &out;
  }

  std::vector<std::optional<Message<>>> receive(const Message<>& msg) {
    out.reset();
    input->receive(msg);
    return {out};
  }

  /// return a list of the operator that emit: id, output message
  map<string, std::vector<Message<>>> receiveDebug(const Message<double>& msg) { return input->receive(msg); }
};

}  // namespace rtbot

#endif  // PIPELINE_H
