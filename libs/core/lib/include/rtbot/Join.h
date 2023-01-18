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
        if (data.empty())
            data.resize(this->parents.size());
        int channel = this->parents.at(sender); // will throw if not found
        data[channel].push(msg);

        bool all_ready = true;
        for (auto &qi : data) {
            while (!qi.empty() && qi.front().time < msg.time)
                qi.pop();
            if (qi.empty() || qi.front().time > msg.time)
                all_ready = false;
        }
        if (all_ready) {
            Message<T> bigMsg;
            bigMsg.time = msg.time;
            for (const auto &qi : data)
                for (const T &x : qi.front().value)
                    bigMsg.value.push_back(x);
            for (auto &qi : data)
                qi.pop();
            processData(bigMsg);
        }
    }

    /**
     *  This is a replacement of Operator::receive but using the already synchronized data provided in msg
     *  It is responsible to emit().
     */
    virtual void processData(Message<T> const &msg) { this->emit(msg); };
};

} // end namespace rtbot


#endif // JOIN_H
