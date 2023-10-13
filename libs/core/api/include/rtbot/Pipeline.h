#ifndef PIPELINE_H
#define PIPELINE_H

#include "rtbot/Operator.h"
#include <sstream>

namespace rtbot {

using namespace std;


template<class T=uint64_t, class V=double>
struct Pipeline : public Operator<T,V> {
    string json_prog;
    map<string, Op_ptr<T,V>> all_op;  // from id to operator
    map<string, Operator<T,V>*> inputs, outputs;

    using Operator<T,V>::Operator;

    Pipeline(string const& id, const string& json_prog_);

    string typeName() const override { return "Pipeline"; }

    string toJson() const;

    void receiveData(Message<T, V> msg, string inputPort = "") override
    {
        auto [id,port]=split2(inputPort);
        if (id.empty() && this->dataInputs.size()==1)
            id=inputs.begin()->first;
        inputs.at(id)->receiveData(msg,port);
        this->toProcess.insert(id);
    }

    map<string, map<string, vector<Message<T, V>>>> executeData() override
    {
        map<string, map<string, vector<Message<T,V>>>> opResults;
        for(auto id : this->toProcess) {
            auto results = inputs.at(id)->executeData();
            Operator<T,V>::mergeOutput(opResults, results);
        }
        return opResults;
    }

    map<string, vector<Message<T, V>>> processData() override { return {}; } // do nothing

private:
    static auto split2(string const& s, char delim = ':')
    {
        std::istringstream is(s);
        string word1;
        getline(is, word1, delim);
        return make_pair(word1, is.str());
    }
};

}

#endif // PIPELINE_H
