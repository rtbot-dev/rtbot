#ifndef PIPELINE_H
#define PIPELINE_H

#include "rtbot/Operator.h"

#include <memory>
#include <map>

namespace rtbot {

using Op_ptr=std::unique_ptr<Operator<double>>;

struct Pipeline {

    std::map<std::string, Op_ptr> all_op;  // from id to operator
    Operator<double> *input;

    void receive(const Message<double>& msg) { input->receive(msg); }
};

}


#endif // PIPELINE_H
