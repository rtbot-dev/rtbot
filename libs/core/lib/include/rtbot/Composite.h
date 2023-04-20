#ifndef COMPOSITE_H
#define COMPOSITE_H

#include "Operator.h"

#include <memory>

namespace rtbot {

template<class T>
struct Composite: public Operator<T>        // TODO: improve from chain to graph
{
    vector<Op_ptr<T>> op;
    Composite(string const &id_, vector<Op_ptr<T>> &&op_)
        : Operator<T>(id_)
        , op(op_)
    {
        for(auto i=0u; i+1<op.size(); i++)
            connect(op[i],  op[i+1]);
    }

    virtual ~Composite()=default;

    virtual map<string,Message<T>> receive(Message<T> const& msg, int port)
    {
        auto out = op.front().receive(msg,port);
        if (auto it=out.find(op.back().id); it!=out.end()) // check if the message reached the last operator of the composite
            emit(it.second);
        else return {};
    }
};



}


#endif // COMPOSITE_H
