#ifndef JOIN_H
#define JOIN_H

#include <queue>
#include "Operator.h"

namespace rtbot {


/**
 * responsible of synchronizing the incomming Messages
 */
template<class T>
class Join : public Operator<T>
{
public:
    std::vector<std::queue<Message<T>>> data; //< the waiting Messages for each channel
    Join(string const &id_) : Operator<T>(id_) {}

    void receive(Message<T> const &msg, const Operator<T> *sender) override
    {
        if (data.empty())   // prepare the data when the first message arrives
            data.resize(this->parents.size());

        // add the incoming message to the correct channel
        int channel = this->parents.at(sender);
        data[channel].push(msg);

        // remove old messages
        for (auto &qi : data)
            while (!qi.empty() && qi.front().time < msg.time)
                qi.pop();

        // check if all queue match the current time
        bool all_ready = true;
        for (const auto &qi : data)
            if (qi.empty() || qi.front().time > msg.time)
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
        msg.time = data.at(0).front().time;
        for (const auto &qi : data)
            for (const T &x : qi.front().value)
                msg.value.push_back(x);

        for (auto &qi : data)
            qi.pop();
        return msg;
    }
};

} // end namespace rtbot


#endif // JOIN_H
