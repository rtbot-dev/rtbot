#ifndef FILTERBYVALUE_H
#define FILTERBYVALUE_H

#include "Operator.h"

#include <functional>

namespace rtbot {
template<class T>
struct FilterByValue: public Operator<T>
{
    std::function<bool(T)> filter;

    FilterByValue(string const &id_, std::function<bool(T)> filter_)
        : Operator<T>(id_)
        , filter(filter_)
    {}

    map<string,Message<T>> receive(Message<T> const& msg) override
    {
        if (all_of(msg.value.begin(), msg.value.end(), filter) )
            return this->emit(msg);
        return {};
    }
};
}

#endif // FILTERBYVALUE_H
