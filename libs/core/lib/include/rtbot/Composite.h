#ifndef COMPOSITE_H
#define COMPOSITE_H

#include "Operator.h"

#include <memory>

namespace rtbot {

template<class T>
struct Composite: public Operator<T>        // TODO: improve from chain to graph
{
    vector<Op_ptr<T>> op;

    using Operator<T>::Operator;

    virtual ~Composite()=default;

    virtual void initializeGraph()=0;

    map<string,Message<T>> receive(Message<T> const& msg, int port)
    {
        if (op.empty()) op=initializeGraph();
        auto out = op.front().receive(msg,port);
        if (auto it=out.find(op.back().id); it!=out.end()) // check if the message reached the last operator of the composite
            emit(it.second);
        else return {};
    }
};



}


#endif // COMPOSITE_H
