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

    Pipeline(const Pipeline& p) : Pipeline(p.id, p.json_prog) {}

    Pipeline& operator=(Pipeline p)
    {
        Operator<T,V>& op1=*this;
        Operator<T,V>& op2=p;
        swap(op1,op2);
        std::swap(json_prog, p.json_prog);
        std::swap(all_op, p.all_op);
        std::swap(inputs, p.inputs);
        std::swap(outputs, p.outputs);
        return *this;
    }

    string typeName() const override { return "Pipeline"; }

    void receiveData(Message<T, V> msg, string inputPort = "") override
    {
        if (inputPort.empty() && inputs.size()==1)
            inputPort=inputs.begin()->first ;
        auto [id,port]=split2(inputPort);
        inputs.at(inputPort)->receiveData(msg,port);
        this->toProcess.insert(inputPort);
    }

    map<string, map<string, vector<Message<T, V>>>> executeData() override
    {
        map<string, map<string, vector<Message<T,V>>>> opResults;
        for(auto id : this->toProcess) {
            auto results = inputs.at(id)->executeData();
            Operator<T,V>::mergeOutput(opResults, results);
        }
        this->toProcess.clear();
        if (opResults.empty()) return opResults;

        // add the prefix of the pipeline: {id, {port,value}} --> {id:port, value}
        map<string, vector<Message<T,V>>> output;
        for(auto [id, op1] : opResults)
            for(auto [port, value] : op1)
                output.emplace(id+":"+port, value);
        return {{this->id, output}};
    }

    map<string, vector<Message<T, V>>> processData() override { return {}; } // do nothing

private:
    static auto split2(string const& s, char delim = ':')
    {
        std::istringstream is(s);
        string word1, word2;
        getline(is, word1, delim);
        getline(is, word2, char(0));
        return make_pair(word1, word2);
    }
};

}

#endif // PIPELINE_H
