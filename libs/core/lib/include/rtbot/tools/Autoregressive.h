#ifndef AUTOREGRESSIVE_H
#define AUTOREGRESSIVE_H

#include "MovingAverage.h"
#include "rtbot/Buffer.h"
#include "rtbot/Composite.h"

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


struct ARMA: public Composite<double>
{
    MovingAverage ma;
    AutoRegressive ar;

    ARMA(string const &id_, vector<double> const& ar_, vector<double> const& ma_)
        : Composite<double>(id_)
        , ma(id_+"_ma",ma_)
        , ar(id_+"_ar",ar_)
    {}

    ARMA(string const &id_, vector<double> const& ar_, int n_ma)
        : Composite<double>(id_)
        , ma(id_+"_ma",n_ma)
        , ar(id_+"_ar",ar_)
    {}

    Operator<double>& front() override { return ma; }
    Operator<double>& back() override { return ar; }
};


}


#endif // AUTOREGRESSIVE_H
