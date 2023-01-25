#ifndef PIPELINE_H
#define PIPELINE_H

#include "rtbot/Operator.h"
#include "Output.h"

#include <memory>
#include <map>
#include <optional>

namespace rtbot {

using Op_ptr=std::unique_ptr<Operator<double>>;

struct Pipeline {

    std::map<std::string, Op_ptr> all_op;  // from id to operator
    Operator<double> *input;
    Output<double> *output;
    std::optional<Message<double>> out;

    std::optional<Message<double>> receive(const Message<double>& msg)
    {
        out.reset();
        input->receive(msg);
        return out;
    }
};

}


#endif // PIPELINE_H
