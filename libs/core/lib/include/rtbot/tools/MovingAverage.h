#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include"rtbot/Buffer.h"

namespace rtbot {

struct MovingAverage: public Buffer<double>
{
    vector<double> coeff;

    MovingAverage(string const &id_, vector<double> const& coeff_)
        : Buffer<double>(id_, coeff_.size())
        , coeff(coeff_)
    {}

    MovingAverage(string const &id_,int n_)
        : Buffer<double>(id_, n_)
        , coeff(n, 1.0/n_)
    {}

    string typeName() const override { return "MovingAverage"; }

    void processData() override
    {
        Message<> out;
        out.time=at(size()/2).time;
        out.value.assign(at(0).value.size(), 0);
        for(auto const& x : (*this))
            for(auto j=0u; j<x.value.size(); j++)
                out.value[j] += x.value[j]/size();
        emit(out);
    }

};

}

#endif // MOVINGAVERAGE_H
