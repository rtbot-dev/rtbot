#ifndef OUTPUT_H
#define OUTPUT_H

#include "Operator.h"
#include <vector>
#include <functional>

namespace rtbot {

using std::function;

template <class T> class Output : public Operator<T> {
    function<void(int t,Buffer<T>)> callback;
public:
    Output(string const &id_, function<void(int t,Buffer<T>)> callback_)
        : Operator<T>(id_), callback(callback_) {}

    void receive(int t, Buffer<T> const &msg) override { callback(t,msg); }
};


/// helper to print
template<class T, class Ostream>
Output<T> makeOutput(string const &id, Ostream &out) {
    return Output<T>(id, [id,&out](int t, Buffer<T> const &msg) {
        out<<id<<" "<<t<<" "<<msg.channelSize<<" "<<msg.windowSize;
        for(auto j=-1; j>=-msg.actualWindowsSize(); j--)
            for(auto i=0; i<msg.channelSize; i++)
                out<<" "<<msg(i,j);
        out<<"\n";
    } );
}



} // end namespace rtbot

#endif // OUTPUT_H
