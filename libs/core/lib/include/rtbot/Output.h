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
    void receive(Message<T> const &msg, const Operator<T> *sender=nullptr) override { out->push_back(msg); }
};


using Output_vec=Output<double, std::vector<Message<>>>;
using Output_opt=Output<double, std::optional<Message<>>>;
using Output_os=Output<double, std::ostream>;


template<>
inline void Output_os::receive(Message<> const &msg, const Operator<> *sender)
{
    (*out)<<id<<" "<<msg<<"\n";
}

template<>
inline void Output_opt::receive(Message<> const &msg, const Operator<> *sender)
{
    *out=msg;
}

} // end namespace rtbot

#endif // OUTPUT_H
