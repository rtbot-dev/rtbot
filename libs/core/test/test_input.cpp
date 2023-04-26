#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/tools/Input.h"
#include "rtbot/tools/CosineResampler.h"


using namespace rtbot;
using namespace std;

TEST_CASE("Input Cosine test emit at right frecuencies")
{
    auto i1 = CosineResampler("i1",99);
    auto i2 = CosineResampler("i2",99);

    SECTION("emits once")
    {    
        for(int i=0; i< 20; i++) {
            map<string,std::vector<Message<>>> emitted =  i1.receive( Message<>(i*100, i*i) );
            if (i==0) REQUIRE (emitted.empty());
            else REQUIRE( emitted.find("i1")->second.at(0).time == 99*i );       
        }
    }

    SECTION("emits twice")
    {
        for(int i=0; i< 20; i++) {
            map<string,std::vector<Message<>>> emitted =  i2.receive( Message<>(i*200, i*i) );
            if (i==0) REQUIRE (emitted.empty());
            else {            

                REQUIRE( emitted.find("i2")->second.at(0).time == 99*(2*i-1)); 
                REQUIRE( emitted.find("i2")->second.at(1).time == 99*(2*i) );    
            }

        }
    }
}