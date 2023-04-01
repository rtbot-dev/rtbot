#ifndef OUTPUT_H
#define OUTPUT_H

#include "Operator.h"
#include <vector>
#include <functional>
#include <iostream>
#include <fstream>
#include <variant>
#include <optional>

namespace rtbot {

using std::function;

template<class T>
std::ostream& operator<<(std::ostream& out, Message<T> const& msg)
{
    out<<msg.time;
    for (auto x:msg.value)
        out<<" "<<x;
    return out;
}

template<class T, class Out> struct Output : public Operator<T>
{
    Out* out=nullptr;

    Output()=default;
    Output(string const &id_, Out& out_)
        : Operator<T>(id_), out(&out_) {}

    string typeName() const override { return "Output"; }
    map<string,Message<T>> receive(Message<T> const &msg, const Operator<T> *sender=nullptr) override { out->push_back(msg); return this->emit(msg); }
};


using Output_vec=Output<double, std::vector<Message<>>>;
using Output_opt=Output<double, std::optional<Message<>>>;
using Output_os=Output<double, std::ostream>;


template<>
inline map<string,Message<>> Output_os::receive(Message<> const &msg, const Operator<> *sender)
{
    (*out)<<id<<" "<<msg<<"\n";
    return emit(msg);
}

template<>
inline map<string,Message<>> Output_opt::receive(Message<> const &msg, const Operator<> *sender)
{
    *out=msg;
    return emit(msg);
}

} // end namespace rtbot

#endif // OUTPUT_H
