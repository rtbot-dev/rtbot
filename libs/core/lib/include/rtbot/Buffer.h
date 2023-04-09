#ifndef BUFFER_H
#define BUFFER_H

#include <deque>
#include "Operator.h"

namespace rtbot {

/**
 * A buffer to store the last-n incomming data
 *
 */
template<class T>
class Buffer: public Operator<T>, public std::deque<Message<T>>
{
public:
    int n=1;                  //< number of message to keep in memory

    using Operator<T>::Operator;
    Buffer(string const &id_, int n_): n(n_), Operator<T>(id_) {}
    virtual ~Buffer()=default;

    map<string,Message<T>> receive(Message<T> const& msg) override
    {
        if (this->size()==n) this->pop_front();
        this->push_back(msg);
        if (this->size()==n) return processData();
        return {};
    }

    /**
     *  This is a replacement of Operator::receive but using the Buffer full data (a std::deque<Message>)
     *  It is responsible to emit().
     */
    virtual map<string,Message<T>> processData()=0;
};



/**
 * A buffer to store the last-n previously computed data
 *
 */
template<class T>
struct AutoBuffer: public Operator<T>, public std::deque<Message<T>>
{
    int n=1;                  //< number of message to keep in memory

    using Operator<T>::Operator;
    AutoBuffer(string const &id_, int n_): n(n_), Operator<T>(id_) {}
    virtual ~AutoBuffer()=default;

    void receive(Message<T> const& msg) override
    {
        while (this->size()<n)
            this->push_front( Message<T>(0, vector<T>(msg.value.size(),T{}) ));  // boundary conditions=T{}
        this->pop_front();
        this->push_back(solve(msg));
        this->emit(this->back());
    }

    /**
     *  compute the response using both the incomming message and the previously computed data (stored as a std::deque<Message>)
     *  For instance, an auto-regressive model (AR)
     *      a0 y(t) + a1 y(t-1) + ... = x(t)
     *  where x(t) is the value of the incomming message and y(t) the message to return, whould return:
     *  Message { t, ( x(t) - a1 at(n-1) - ... )/a0 }
     */
    virtual Message<T> solve(Message<T> const& msg) const { return msg; }
};


} // namespace rtbot


#endif // BUFFER_H

