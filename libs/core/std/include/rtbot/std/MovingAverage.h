#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include "rtbot/Buffer.h"

namespace rtbot {

template <class T = double>
struct MovingAverage : public Buffer<T> {

  unsigned int iteration;

  MovingAverage() = default;

  MovingAverage(string const& id_, int n_) 
    : Buffer<T>(id_, n_), iteration(0) {}

  string typeName() const override { return "MovingAverage"; }

  map<string, std::vector<Message<T>>> processData() override {

    std::vector<Message<T>> toEmit;
    Message<T> out;
    T average;    

    average = 0;

    if(iteration == 0) {

        sum = 0;
        
        for (size_t j = 0; j < this->size(); j++) 
        {
            sum = sum + this->at(j).value;
        }
        average = sum / this->size();        
        
    }
    else 
    {
        
        sum = sum + this->back().value; 
        average = sum / this->size();              
                   
    }

    iteration++;    
                   
    sum = sum - this->front().value; 

    out.time = this->back().time;
    out.value = average;

    toEmit.push_back(out);
    return this->emit(toEmit);

  }

  private:

    T sum;

};

}  // namespace rtbot

#endif  // MOVINGAVERAGE_H
