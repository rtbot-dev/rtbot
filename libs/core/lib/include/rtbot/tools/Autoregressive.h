#ifndef AUTOREGRESSIVE_H
#define AUTOREGRESSIVE_H

#include "MovingAverage.h"
#include "rtbot/Buffer.h"
#include "rtbot/Chain.h"

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


struct ARMA: public Chain<double>
{
    ARMA(string const &id_, vector<double> const& ar_, vector<double> const& ma_)
        : Chain<double>(id_, {
                        std::make_unique<AutoRegressive>(id_+"_ar",ar_),
                        std::make_unique<MovingAverage>(id_+"_ma",ma_) })
    {}

    ARMA(string const &id_, vector<double> const& ar_, int n_ma)
        : Chain<double>(id_, {
                        std::make_unique<AutoRegressive>(id_+"_ar",ar_),
                        std::make_unique<MovingAverage>(id_+"_ma",n_ma) })
    {}
};


//-------------------------------- Some examples ------------

struct TotalSum: public AutoRegressive
{
    TotalSum(string const &id_) : AutoRegressive(id_,{1.0}) {}
};

struct Finance_si: public AutoBuffer<double>
{
    int nPeriod;

    Finance_si(string const &id_, int nPeriod_)
        : AutoBuffer<double>(id_,1), nPeriod(nPeriod_)
    {}

    Message<> solve(Message<> const& msg) const override
    {
        if (c < nPeriod) {
            c++;
            sum += msg.value[0];
            return Message<> {msg.time, sum/c};
        }
        return {msg.time,
                msg.value[0]*(1.0/nPeriod)+ back().value[0]*(1-1.0/nPeriod)};
    }
private:
    mutable int c=0;
    mutable double sum=0;
};


}


#endif // AUTOREGRESSIVE_H
