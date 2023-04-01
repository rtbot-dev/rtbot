#ifndef COMPOSITE_H
#define COMPOSITE_H

#include "Operator.h"

#include <memory>

namespace rtbot {

template<class T>
struct Composite: public Operator<T>
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

protected:
    void addChildren(Operator<T>* child) override { op.back()->addChildren(child); }
    void addSender(const Operator<T>* sender) override { op.front()->addSender(sender); }
};



}


#endif // COMPOSITE_H
