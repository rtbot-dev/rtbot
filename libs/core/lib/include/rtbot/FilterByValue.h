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

    void receive(Message<T> const& msg, const Operator<T> *) override
    {
        if (all_of(msg.value.begin(), msg.value.end(), filter) )
            this->emit(msg);
    }
};
}

#endif // FILTERBYVALUE_H
