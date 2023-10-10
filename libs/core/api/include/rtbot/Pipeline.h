#ifndef PIPELINE_H
#define PIPELINE_H


#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;


template<class T=uint64_t, class V=double>
struct Pipeline : public Operator<T,V> {
    map<string, Op_ptr<T,V>> all_op;  // from id to operator
    map<string, Op_ptr<T,V>> inputs, outputs;

    using Operator<T,V>::Operator;

    Pipeline(string const& id, const string& json_prog);

    string typeName() const override { return "Pipeline"; }

    map<string, vector<Message<T, V>>> processData() override
    {

    }
};

}

#endif // PIPELINE_H
