#ifndef INPUT_H
#define INPUT_H

#include <vector>
#include <cmath>
#include <cstdint>

namespace rtbot {

template<class T=double>
struct InputCosine: public Operator<T>
{
    
    using Operator<T>::Operator;    

    InputCosine(string const &id_,unsigned int dt_ = 1000)
        : Operator<T>(id_),dt(dt_),anchorTime(0),lastTime(0),carryOver(0)
    {}

    string typeName() const override { return "InputCosine"; }

    map<string,std::vector<Message<T>>> receive(Message<T> const& msg) override 
    {        

        if (msg.time < lastTime) return {};

        lastTime = msg.time;

        if (anchorValue.size() == 0)  {
            anchorTime = msg.time;
            anchorValue = std::move(msg.value);
            return {};
        }
        
        int j = 1;

        std::vector<Message<T>> toEmit;

        while (lastTime - anchorTime >= (j * dt) - carryOver) {
            Message<T> out;
            double mu = ((j * dt) - carryOver)/(lastTime - anchorTime);
            for(auto i = 0; i < msg.value.size(); i++) {
                out.value.push_back(cosineInterpolate(anchorValue.at(i), msg.value.at(i), mu));
            }
            out.time = anchorTime + ((j * dt) - carryOver);            
            toEmit.push_back(out);
            j++;            
        }

        if (toEmit.size() > 0) {
            carryOver = lastTime - (anchorTime + (((j-1) * dt) - carryOver));
            anchorTime = lastTime;
            anchorValue = std::move(msg.value);            
            return this->emit(toEmit);
        }
        else return {}; 
        
    }

    double cosineInterpolate(double y1,double y2,double mu)
    {
        double mu2;
        mu2 = (1-std::cos(mu * 3.14159265 ))/2;
        return(y1*(1-mu2)+y2*mu2);
    }

    private:
        std::uint64_t anchorTime;
        std::vector<T> anchorValue;
        std::uint64_t lastTime;
        unsigned int dt;
        unsigned int carryOver;

};

}

#endif // INPUT_H
