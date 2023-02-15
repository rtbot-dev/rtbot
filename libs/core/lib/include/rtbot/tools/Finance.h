#ifndef FINANCE_H
#define FINANCE_H

#include"Autoregressive.h"

#include <functional>

namespace rtbot {

template<class T>
struct Count: public Operator<T>
{
    Count(string const &id_)
        : Operator<T>(id_, [c = 0](const T&) mutable {return T(++c);})
{}
};

template<class T>
struct PartialSum: public Operator<T>
{
    PartialSum(string const &id_)
        : Operator<T>(id_, [s = T(0)](const T& x) mutable {return s+=x;})
    {}
};


template<class T>
struct FilterByValue: public Operator<T>
{
    std::function<bool(T)> filter;

    FilterByValue(string const &id_, std::function<bool(T)> filter_)
        : Operator<T>(id_)
        , filter(filter_)
    {}

    void receive(Message<T> const& msg, const Operator<T> *) override
    {
        if (all_of(msg.value.begin(), msg.value.end(), filter) )
            this->emit(msg);
    }
};

struct FilterGreaterThan: public FilterByValue<double>
{
    FilterGreaterThan(string const &id_, double x0=0)
        : FilterByValue<double>(id_, [=](double x){ return x>x0; })
    {}
};

struct FilterLessThan: public FilterByValue<double>
{
    FilterLessThan(string const &id_, double x0=0)
        : FilterByValue<double>(id_, [=](double x){ return x<x0; })
    {}
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

#endif // FINANCE_H
