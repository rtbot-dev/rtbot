#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/tools/Input.h"


using namespace rtbot;
using namespace std;

TEST_CASE("Input test emit at right frecuencies")
{
    auto i1 = Input("i1");
    auto i2 = Input("i2");

    SECTION("emits every other time")
    {    
        for(int i=1; i< 20; i++) {
            map<string,std::vector<Message<>>> emitted =  i1.receive( Message<>( (1-(i%2)) * i * 10, i*i) );
            if (i % 2 == 1 ) REQUIRE (emitted.empty());
            else {                
                REQUIRE( emitted.find("i1")->second.at(0).time == 0 ); 
            }      
        }
    }

    SECTION("emits every time")
    {
        for(int i=0; i< 20; i++) {
            map<string,std::vector<Message<>>> emitted =  i2.receive( Message<>(i*200, i*i) );
            if (i==0) REQUIRE (emitted.empty());
            else {
                REQUIRE( emitted.find("i2")->second.at(0).time == (i-1)*200);  
            }

        }
    }

    SECTION("never emits")
    {
        for(int i=0; i< 20; i++) {
            map<string,std::vector<Message<>>> emitted =  i2.receive( Message<>(200, i*i) );
            REQUIRE (emitted.empty());            

        }
    }
}
