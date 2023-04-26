#ifndef INPUT_H
#define INPUT_H

#include <vector>
#include <cmath>
#include <cstdint>
 #include <iostream>

#include"rtbot/Buffer.h"
#include "rtbot/Enums.h"

namespace rtbot {

struct Input: public Buffer<double>
{   

    static const int size = 2;

    Input()=default;

    Input(string const &id_)
        : Buffer<double>(id_, Input::size)
    {}

    string typeName() const override { return "Input"; }

    map<string,std::vector<Message<>>> processData() override
    {
        if (at(1).time - at(0).time <= 0) return {};

        Message<> out;
        std::vector<Message<>> toEmit;
            
        out.time = at(0).time;
        out.value = at(0).value;            
        toEmit.push_back(out); 
        return this->emit(toEmit);
    }

};

}

#endif // INPUT_H
