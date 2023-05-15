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
class Buffer : public Operator<T, V>, public std::deque<Message<T, V>> {
 public:
  size_t n = 1;  // number of message to keep in memory

  using Operator<T, V>::Operator;
  Buffer(string const& id_, size_t n_) : n(n_), Operator<T, V>(id_) {}
  virtual ~Buffer() = default;

  map<string, std::vector<Message<T, V>>> receive(Message<T, V> const& msg) override {
    if (this->size() == n) {
      sum = sum - this->front().value;
      this->pop_front();
    }
    this->push_back(msg);
    sum = sum + this->back().value;
    if (this->size() == n) return processData();
    return {};
  }

  V getSum() { return sum; }

  /**
   *  This is a replacement of Operator::receive but using the Buffer full data (a std::deque<Message>)
   *  It is responsible to emit().
   */
  virtual map<string, std::vector<Message<T, V>>> processData() = 0;

  /*
    This is to store the sum of all the message values in the buffer
  */
 private:
  V sum = 0;
};

}  // namespace rtbot

#endif  // BUFFER_H
