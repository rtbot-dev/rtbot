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
        if (this->size()==n) processData();
    }

    /**
     *  This is a replacement of Operator::receive but using the Buffer full data (a std::deque<Message>)
     *  It is responsible to emit().
     */
    virtual void processData()=0;
};


} // namespace rtbot


#endif // BUFFER_H

