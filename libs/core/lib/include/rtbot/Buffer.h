#ifndef BUFFER_H
#define BUFFER_H

#include <deque>
#include "Operator.h"

namespace rtbot {

template<class T>
class Buffer: public Operator<T>, public std::deque<Message<T>>
{
public:
    int n;                  //< number of message to keep in memory
    Buffer(string const &id_, int n_): n(n_), Operator<T>(id_) {}

    void receive(Message<T> const& msg) override
    {
        if (this->size()==n) this->pop_front();
        this->push_back(msg);
        if (this->size()==n) this->emit(compute());
    }

    /**
     *  Compute the message to send to the children.
     *  For that, the Buffer data is available as s std::deque.
     *  Example: the MA(n) is just
     *  return Message(at(n).time, std::accumulate(begin(), end())/n );
     */
    virtual Message<T> compute() const = 0;
};


} // namespace rtbot


#endif // BUFFER_H

