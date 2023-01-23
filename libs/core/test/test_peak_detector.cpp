#include "rtbot/PeakDetector.h"
#include "rtbot/Output.h"
#include "rtbot/Join.h"
#include "rtbot/MovingAverage.h"

#include <catch2/catch.hpp>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

using namespace rtbot;
using namespace std;

TEST_CASE("simple peak detector")
{
    int nlag = 3;
    auto op = PeakDetector("b1", nlag);

    vector<Message<>> msg_l;
    auto o1=Output<double>("o1", [&](Message<double> msg){ msg_l.push_back(msg); });
    connect(&op, &o1);

    SECTION("one peak") {
        for(int i=0; i<10; i++)
            op.receive(Message<>(i, 5-fabs(1.0*i-5)));
        REQUIRE(msg_l.size()==1);
        REQUIRE(msg_l[0] == Message<>(5,5.0));
    }

    SECTION("two peaks") {
        for(int i=0; i<14; i++)
            op.receive(Message<>(i, i%5));
        REQUIRE(msg_l.size()==2);
        REQUIRE(msg_l[0] == Message<>(4,4.0));
        REQUIRE(msg_l[1] == Message<>(9,4.0));
    }
}


struct Difference: public Join<double>
{
    using Join<double>::Join;

    void processData(Message<double> const &msg) override
    {
        emit(Message<>(msg.time, msg.value.at(1)-msg.value.at(0)));
    }
};




struct SamplePPG
{
    vector<int> ti;
    vector<double> ppg;

    SamplePPG(const char filename[])
    {
        ifstream in(filename);
        if (!in.is_open()) throw runtime_error("SamplePPG: file not found");
        string line;
        getline(in,line); // drop first line
        while (getline(in,line))
        {
            auto words=split(line,',');
            ti.push_back(atof(words[0].c_str())*1000);
            ppg.push_back(-atof(words[1].c_str()));
        }
    }

    double dt() const { return (ti.back()-ti.front())/(ti.size()-1); }

private:
    static vector<string> split(string s, char delim=' ')
    {
        vector<string> out;
        istringstream is(s);
        string word;
        while (getline(is,word,delim))
            out.push_back(word);
        return out;
    }
};

TEST_CASE("ppg peak detector")
{
    auto s=SamplePPG("ppg.csv");

    auto i1 = Input<double>("i1");
    auto ma1 = MovingAverage("ma1", round(50/s.dt()) );
    auto ma2 = MovingAverage("ma2", round(2000/s.dt()) );
    auto diff = Difference("diff");
    auto peak = PeakDetector("b1", 2*ma1.n+1);
    auto join = Join<double>("j1");
    ofstream out("peak.txt");
    auto o1 = makeOutput<double>("o1", out);

    connect(&i1, &ma1);
    connect(&i1, &ma2);
    connect(&ma1, &diff);
    connect(&ma2, &diff);
    connect(&diff, &peak);

    connect(&peak ,&join);
    connect(&i1, &join);
    connect(&join, &o1);

    // process the data
    for(auto i=0u; i<s.ti.size(); i++)
        i1.receive(Message<>(s.ti[i], s.ppg[i]));
}
