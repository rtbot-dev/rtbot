#ifndef BUFFER_H
#define BUFFER_H

#include <deque>

#include "Operator.h"

namespace rtbot {

/**
 * A buffer to store the last-n incoming data
 *
 */
template <class T, class V>
class Buffer : public Operator<T,V>, public std::deque<Message<T,V>> {
 public:
  int n = 1;  // number of message to keep in memory

  using Operator<T,V>::Operator;
  Buffer(string const& id_, int n_) : n(n_), Operator<T,V>(id_) {}
  virtual ~Buffer() = default;

  map<string, std::vector<Message<T,V>>> receive(Message<T,V> const& msg) override {
    if (this->size() == n) 
    {
      sum = sum - this->front().value; 
      this->pop_front(); 
    }
    this->push_back(msg);    
    sum = sum + this->back().value;
    if (this->size() == n) return processData();
    return {};
  }

  V getSum() {
    return sum;
  }

  /**
   *  This is a replacement of Operator::receive but using the Buffer full data (a std::deque<Message>)
   *  It is responsible to emit().
   */
  virtual map<string, std::vector<Message<T,V>>> processData() = 0;

  /*
    This is to store the sum of all the message values in the buffer
  */
  private:
    V sum = 0;
};

/**
 * A buffer to store the last-n previously computed data
 *
 */
template <class T,class V>
struct AutoBuffer : public Operator<T,V>, public std::deque<Message<T,V>> {
  int n = 1;  //< number of message to keep in memory

  using Operator<T,V>::Operator;
  AutoBuffer(string const& id_, int n_) : n(n_), Operator<T,V>(id_) {}
  virtual ~AutoBuffer() = default;

  void receive(Message<T,V> const& msg) override {
    while (this->size() < n)
      this->push_front(Message<T,V>(0, vector<V>(msg.value.size(), V{})));  // boundary conditions=V{}
    this->pop_front();
    this->push_back(solve(msg));
    this->emit(this->back());
  }

  /**
   * Compute the response using both the incoming message and the previously computed data (stored as a
   * std::deque<Message>) For instance, an auto-regressive model (AR) a0 y(t) + a1 y(t-1) + ... = x(t) where x(t) is the
   * value of the incoming message and y(t) the message to return, would return: Message { t, ( x(t) - a1 at(n-1) -
   * ... )/a0 }
   */
  virtual Message<T,V> solve(Message<T,V> const& msg) const { return msg; }
  
};

}  // namespace rtbot

#endif  // BUFFER_H
