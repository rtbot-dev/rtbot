#ifndef INPUT_H
#define INPUT_H

#include <vector>
#include <cmath>
#include <cstdint>

namespace rtbot {

enum Type { cosine, hermite, chebyshev };

struct Input: public Buffer<double>
{
    Input()=default;

    Input(string const &id_, Type type_ , unsigned int dt_=100)
        : Buffer<double>(id_, 4),type(type_), dt(dt_), carryOver(0)
    {}

    string typeName() const override { return "Input"; }

    map<string,std::vector<Message<>>> processData() override
    {
        switch (type) {
            case Type::cosine: return cosine();
            case Type::hermite: return hermite();                
            case Type::chebyshev: return chebyshev();
            default: return cosine();
        }
    }

    map<string,std::vector<Message<>>> cosine()
    {
        if (size() < 2) return {};
        else if (at(1).time - at(0).time <= 0) return {};

        int j = 1;

        std::vector<Message<>> toEmit;

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

    
    map<string,std::vector<Message<>>> hermite()
    {
        return {};
    }

    map<string,std::vector<Message<>>> chebyshev()
    {
        return {};
    }

    private:        
        Type type;
        unsigned int dt;
        std::uint64_t carryOver;


    double cosineInterpolate(double y1,double y2,double mu)
    {
        double mu2;
        mu2 = (1-std::cos(mu * 3.14159265 ))/2;
        return(y1*(1-mu2)+y2*mu2);
    }

};

}

#endif // INPUT_H
