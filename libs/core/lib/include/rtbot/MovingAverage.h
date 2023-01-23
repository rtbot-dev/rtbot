#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include"Buffer.h"

namespace rtbot {

struct MovingAverage: public Buffer<double>
{
    using Buffer<double>::Buffer;

    void processData() override
    {
        Message<> out;
        out.time=at(size()/2).time;
        out.value.assign(at(0).value.size(), 0);
        for(auto x : (*this))
            for(auto j=0u; j<x.value.size(); j++)
                out.value[j] += x.value[j]/size();
        emit(out);
    }

};

}

#endif // MOVINGAVERAGE_H
