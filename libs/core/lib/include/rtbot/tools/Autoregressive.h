#ifndef AUTOREGRESSIVE_H
#define AUTOREGRESSIVE_H

#include "MovingAverage.h"
#include "rtbot/Buffer.h"
#include "rtbot/Composite.h"
#include "rtbot/FactoryOp.h"

namespace rtbot {

struct AutoRegressive: public AutoBuffer<double>
{
    std::vector<double> coeff;

    AutoRegressive(string const &id_, vector<double> const& coeff_)
        : AutoBuffer<double>(id_, coeff_.size())
        , coeff(coeff_)
    {}

    string typeName() const override { return "AutoRegressive"; }

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
    vector<double> ar_coeff;
    int ma_n;

    void initializeGraph() override
    {
        MovingAverage ma(id+"_ma",ma_n);
        AutoRegressive ar(id+"_ar",ar_coeff);
        ma.connect(ar);
        this->op=FactoryOp::readOps(FactoryOp::writeOps({&ma,&ar}));  // TODO: this is the moment to introduce clone()!!
    }


};


}


#endif // AUTOREGRESSIVE_H
