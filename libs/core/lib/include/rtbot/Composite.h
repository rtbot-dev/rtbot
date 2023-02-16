#ifndef COMPOSITE_H
#define COMPOSITE_H

#include "Operator.h"

#include <memory>

namespace rtbot {

template<class T>
using Op_ptr=std::unique_ptr<Operator<T>>;

template<class T>
struct Composite: public Operator<T>
{
    using Operator<T>::Operator;
    virtual ~Composite()=default;

    virtual Operator<T>& front()=0;
    virtual Operator<T>& back()=0;

protected:
    void addChildren(Operator<T>* child) override { back()->addChildren(child); }
    void addSender(const Operator<T>* sender) override { front()->addSender(sender); }
};



}


#endif // COMPOSITE_H
