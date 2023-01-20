#ifndef OUTPUT_H
#define OUTPUT_H

#include "Operator.h"
#include <vector>
#include <functional>

namespace rtbot {

using std::function;

template <class T> class Output : public Operator<T> {
    function<void(Message<T>)> callback;
public:
    Output(string const &id_, function<void(Message<T>)> callback_)
        : Operator<T>(id_), callback(callback_) {}

    void receive(Message<T> const &msg, const Operator<T> *sender=nullptr) override { callback(msg); }
};


/// helper to print
template<class T, class Ostream>
Output<T> makeOutput(string const &id, Ostream &out) {
    return Output<T>(id, [id,&out](Message<T> const &msg) {
        out<<id<<" "<<msg.time;
        for (auto x:msg.value)
                out<<" "<<x;
        out<<"\n";
    } );
}



} // end namespace rtbot

#endif // OUTPUT_H
