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
    std::vector<T> average;

    size_t size = this->at(0).value.size();

    average.assign(size, 0);    

    if(iteration == 0) {

        sum.assign(size, 0);

        for (size_t i = 0; i < sum.size(); i++)
        {
            for (size_t j = 0; j < this->size(); j++) 
            {
                sum[i] += this->at(j).value[i];
            }
            average[i] = sum[i] / this->size();
        }
        
    }
    else 
    {
        for (size_t i = 0; i < sum.size(); i++)
        {                
            sum[i] += this->back().value[i]; 
            average[i] = sum[i] / this->size();              
        }            
    }

    iteration++;

    for (size_t i = 0; i < sum.size(); i++)
    {                
        sum[i] -= this->front().value[i];                       
    }

    out.time = this->at(this->size() - 1).time;
    out.value = std::move(average);

    toEmit.push_back(out);
    return this->emit(toEmit);

  }

  private:

    std::vector<T> sum;

};

}  // namespace rtbot

#endif  // MOVINGAVERAGE_H
