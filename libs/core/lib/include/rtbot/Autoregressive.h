#ifndef AUTOREGRESSIVE_H
#define AUTOREGRESSIVE_H

#include "Buffer.h"
#include "MovingAverage.h"

namespace rtbot {

struct AutoRegressive: public AutoBuffer<double>
{
    std::vector<double> coeff;

    AutoRegressive(string const &id_, vector<double> const& coeff_)
        : AutoBuffer<double>(id_, coeff_.size())
        , coeff(coeff_)
    {}

    Message<> solve(Message<> const& msg) const override
    {
        Message<> out = msg;
        for(auto i=0; i<n; i++)
            for(auto j=0u; j<at(n-1-i).value.size(); j++)
                out.value[j] += coeff[i] * at(n-1-i).value[j];
        return out;
    }
};


struct ARMA: public Operator<double>
{
    AutoRegressive ar;
    MovingAverage ma;

    ARMA(string const &id_, vector<double> const& ar_, vector<double> const& ma_)
        : ar(id_,ar_)
        , ma(id_,ma_)
    {}
};



}


#endif // AUTOREGRESSIVE_H
