#ifndef OUTPUT_H
#define OUTPUT_H

#include "Operator.h"
#include <vector>
#include <functional>
#include <iostream>
#include <fstream>

namespace rtbot {

using std::function;

template <class T> class Output : public Operator<T> {
    function<void(Message<T>)> callback;
public:
    Output(string const &id_, function<void(Message<T>)> callback_)
        : Operator<T>(id_), callback(callback_) {}

    Output(string const &id_, std::ostream &out=std::cout)
        :Output<T>(id_, makeCallback(id_,out))
    {}

    Output(string const &id, string const& filename): Output<T>(id, out) { out.open(filename); }

    void receive(Message<T> const &msg, const Operator<T> *sender=nullptr) override { callback(msg); }

private:
    std::ofstream out; //< used just in case the output goes to a file

    static function<void(Message<T>)> makeCallback(string const &id,std::ostream& out)
    {
        return [id,&out](Message<T> const &msg) {
            out<<id<<" "<<msg.time;
            for (auto x:msg.value)
                    out<<" "<<x;
            out<<"\n";
        };
    }

};

} // end namespace rtbot

#endif // OUTPUT_H
