#ifndef CHAIN_H
#define CHAIN_H

#include "Operator.h"

#include <memory>

namespace rtbot {

template<class T>
using Op_ptr=std::unique_ptr<Operator<T>>;

template<class T>
struct Chain: public Operator<T>
{
    vector<Op_ptr<T>> op;
    Chain(string const &id_, vector<Op_ptr<T>> &&op_)
        : Operator<T>(id_)
        , op(op_)
    {
        for(auto i=0u; i+1<op.size(); i++)
            connect(op[i],  op[i+1]);
    }

    virtual ~Chain()=default;

protected:
    void addChildren(Operator<T>* child) override { op.back()->addChildren(child); }
    void addSender(const Operator<T>* sender) override { op.front()->addSender(sender); }
};



}


#endif // CHAIN_H
