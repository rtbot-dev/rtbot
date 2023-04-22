#ifndef INPUT_H
#define INPUT_H

#include <vector>
#include <cmath>
#include <cstdint>

#include"rtbot/Buffer.h"
#include "rtbot/Enums.h"

namespace rtbot {

struct Input: public Buffer<double>
{   

    Type iType;
    unsigned int dt;

    Input()=default;

    static int getSize(Type iType_)
    {      
        
        switch (iType_) {
            case Type::cosine: return 2;
            case Type::hermite: return 4;                
            case Type::chebyshev: return 4;
            default: return 2;
        }
    };

    Input(string const &id_, Type iType_ , unsigned int dt_)
        : Buffer<double>(id_, Input::getSize(iType_)),iType(iType_), dt(dt_), carryOver(0)
    {}

    string typeName() const override { return "Input"; }

    map<string,std::vector<Message<>>> processData() override
    {
        switch (iType) {
            case Type::cosine: return cosineDef();
            case Type::hermite: return hermiteDef();                
            case Type::chebyshev: return chebyshevDef();
            default: return cosineDef();
        }
    }

    map<string,std::vector<Message<>>> cosineDef()
    {
        std::vector<Message<>> toEmit;
        
        if (at(1).time - at(0).time <= 0) return {};

        int j = 1;        

        while (at(1).time - at(0).time >= (j * dt) - carryOver) {
            Message<> out;
            double mu = ((j * dt) - carryOver)/(at(1).time - at(0).time);
            for(size_t i = 0; i < at(0).value.size(); i++) {
                out.value.push_back(cosineInterpolate(at(0).value.at(i), at(1).value.at(i), mu));
            }
            out.time = at(0).time + ((j * dt) - carryOver);            
            toEmit.push_back(out);
            j++;            
        }

        carryOver = at(1).time - (at(0).time + (((j-1) * dt) - carryOver));

        if (toEmit.size() > 0) return this->emit(toEmit); else return {};
    }

    
    map<string,std::vector<Message<>>> hermiteDef()
    {
        return {};
    }

    map<string,std::vector<Message<>>> chebyshevDef()
    {
        return {};
    }

    private:       
        
        std::uint64_t carryOver;


    double cosineInterpolate(double y1,double y2,double mu)
    {
        double mu2;
        mu2 = (1-std::cos(mu * 3.141592653589 ))/2;
        return(y1*(1-mu2)+y2*mu2);
    }

};

}

#endif // INPUT_H
