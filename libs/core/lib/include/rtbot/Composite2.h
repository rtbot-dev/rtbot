#ifndef COMPOSITE2_H
#define COMPOSITE2_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Composite2 : public Operator<T, V> {

    using Operator<T,V>::Operator;

    string typeName() const override { return "Composite2"; }

    template<class Oper>
    Operator<T,V>* addOp(Oper op)
    {
        auto [it, _]=all_op.emplace(op.id, make_unique<Oper>(op));
        return it.second.get();
    }

    map<string, vector<Message<T, V>>> processData() override { return {}; }

    void updateInputPorts(); // TODO
    void updateOutputPorts(); //TODO

private:
    map<string, Op_ptr<T,V>> all_op;

};


}


#endif // COMPOSITE2_H
