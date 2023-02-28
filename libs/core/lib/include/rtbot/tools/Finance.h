#ifndef FINANCE_H
#define FINANCE_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

namespace tools {

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

}

}

#endif // FINANCE_H
