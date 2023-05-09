#ifndef ARBUFFER_H
#define ARBUFFER_H

#include <deque>

#include "Operator.h"

namespace rtbot {
/**
 * A buffer to store the last-n previously computed data
 *
 */
template <class T, class V>
struct ARBuffer : public Operator<T, V>, public std::deque<Message<T, V>> {
  int n = 1;  // number of message to keep in memory

  using Operator<T, V>::Operator;
  ARBuffer(string const& id_, int n_) : n(n_), Operator<T, V>(id_) {}
  virtual ~ARBuffer() = default;

  map<string, std::vector<Message<T, V>>> receive(Message<T, V> const& msg) override {
    while (this->size() < n) this->push_front(Message<T, V>(0, 0));  // boundary conditions=0
    Message<T, V> out = processData(msg);
    this->pop_front();
    this->push_back(out);
    return this->emit(out);
  }

  /**
   * Compute the response using both the incoming message and the previously computed data (stored as a
   * std::deque<Message>) For instance, an auto-regressive model (AR) a0 y(t) + a1 y(t-1) + ... = x(t) where x(t) is the
   * value of the incoming message and y(t) the message to return, would return: Message { t, ( x(t) - a1 at(n-1) -
   * ... )/a0 }
   */
  virtual Message<T, V> processData(Message<T, V> const& msg) = 0;
};

}  // namespace rtbot

#endif  // ARBUFFER_H