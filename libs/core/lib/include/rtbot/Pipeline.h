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

    explicit Pipeline(std::string const& json_string);

    Pipeline(Pipeline const&)=delete;
    void operator=(Pipeline const&)=delete;

    Pipeline(Pipeline &&other){
        all_op=std::move(other.all_op);
        input=std::move(other.input);
        output=std::move(other.output);
        out=std::move(other.out);
        output->callback=[this](Message<> const& msg) { out=msg; };
    }

    std::vector<std::optional<Message<double>>> receive(const Message<double>& msg)
    {
        out.reset();
        //output->callback=[this](Message<> const& msg) { out=msg; };
        input->receive(msg);
        return {out};
    }
};

}


#endif // PIPELINE_H
