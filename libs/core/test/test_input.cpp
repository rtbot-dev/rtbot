#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/tools/Input.h"


using namespace rtbot;
using namespace std;

TEST_CASE("Input Cosine test emit at right frecuencies")
{
    auto i1 = Input("i1",Type::cosine,99);
    auto i2 = Input("i2",Type::cosine,99);
    
    for(int i=0; i< 20; i++) {
        map<string,std::vector<Message<>>> emitted =  i1.receive( Message<>(i*100, i*i) );
        if (i==0) REQUIRE (emitted.empty());
        else REQUIRE( emitted.find("i1")->second.at(0).time == 99*i );       
    }

    for(int i=0; i< 20; i++) {
        map<string,std::vector<Message<>>> emitted =  i2.receive( Message<>(i*200, i*i) );
        if (i==0) REQUIRE (emitted.empty());
        else {            

            REQUIRE( emitted.find("i2")->second.at(0).time == 99*(2*i-1)); 
            REQUIRE( emitted.find("i2")->second.at(1).time == 99*(2*i) );    
        }

    }
}