#ifndef JOIN_H
#define JOIN_H

#include <queue>
#include "Operator.h"

namespace rtbot {


/**
 * class Join is responsible for synchronizing many channels of Messages. This is a simple and intuitive implementation.
 * It uses as many queues as channels.
 *
 * The queues are updated every time a new message arrives:
 * 1. the old messages are removed.
 * 2. If all the incoming messages of the queues match the current time stamp, then a message is generated by concatenating them.
 *
 * To implement any Operator that requires synchronizing many channels of messages
 * the user should just inherit from Join and override the method processData(msg) where the ready-to-use message msg is given.
 */
template<class T>
class Join : public Operator<T>
{
    std::map<const Operator<T> *, std::queue<Message<T>>> data; //< the waiting Messages for each sender
public:
    Join(string const &id_) : Operator<T>(id_) {}

    void addSender(const Operator<T> *sender) override { data[sender]; }

    void receive(Message<T> const &msg, const Operator<T> *sender) override
    {
        // add the incoming message to the correct channel
        data.at(sender).push(msg);

        // remove old messages
        for (auto &x : data)
            while (!x.second.empty() && x.second.front().time < msg.time)
                x.second.pop();

        // check if all queue match the current time
        bool all_ready = true;
        for (const auto &x : data)
            if (x.second.empty() || x.second.front().time > msg.time)
                all_ready = false;

        if (all_ready)
            processData(makeMessage());
    }


    /**
     *  This is a replacement of Operator::receive but using the already synchronized data provided in msg
     *  It is responsible to emit().
     */
    virtual void processData(Message<T> const &msg) { this->emit(msg); };

private:
    // build a message by concatenating all channels front() data. Remove the used data.
    Message<T> makeMessage()
    {
        Message<T> msg;
        msg.time = data.begin()->second.front().time;
        for (const auto &x : data)
            for (const T &xi : x.second.front().value)
                msg.value.push_back(xi);

        for (auto &x : data)
            x.second.pop();
        return msg;
    }
};

} // end namespace rtbot


#endif // JOIN_H
