#ifndef COST_FUNCTION_H
#define COST_FUNCTION_H

#include "rtbot/Message.h"
#include "rtbot/Pipeline.h"
#include "rtbot/std/Matching.h"

namespace rtbot {

struct Sample {
    vector<Message<>> msgs;
    vector<std::uint64_t> expected_times;
};

struct CostFunction {
    struct ParamData {
        string op_id;
        string paramName;
        double current, lower, upper;                  ///< current value, lower and upper bound
    };

    string prog_json;
    vector<ParamData> paramsData;
    vector<Sample> data;

    CostFunction(std::string const& prog_json_, std::string const& params_json, vector<Sample> const& data_)
        : prog_json(prog_json_)
        , paramsData(createParamData(prog_json, params_json))
        , data(data_)
    {}

    double operator()(vector<double> const& params) const
    {
        double cost=0;
        string prog2=get_prog_json(params);
        for(Sample const& s : data) {
            Pipeline pipe2(prog2);
            vector<uint64_t> times;
            for(auto msg : s.msgs) {
                auto out=pipe2.receive(msg).at(0);
                if (out) times.push_back(out->time);
            }
            Matching<uint64_t> match {s.expected_times, times};
            cost += match.cost();
        }
        return cost;
    }

    string get_prog_json(vector<double> const& params) const;

    static vector<ParamData> createParamData(std::string const& prog_json, std::string const& params_json);
};


} // end namespace rtbot

#endif // COST_FUNCTION_H
