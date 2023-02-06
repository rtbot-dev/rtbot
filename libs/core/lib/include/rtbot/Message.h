#ifndef MESSAGE_H
#define MESSAGE_H

#include <vector>


namespace rtbot {

template<class T=double>
struct Message {
    int time;
    std::vector<T> value;

    Message()=default;
    Message(int time_, T value_): time(time_), value(1,value_) {}
    Message(int time_, std::vector<T> const& value_): time(time_), value(value_) {}
};

template<class T>
bool operator==(Message<T> const& a, Message<T> const& b) { return a.time==b.time && a.value==b.value; }


}

#endif // MESSAGE_H
