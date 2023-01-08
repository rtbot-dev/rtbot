#ifndef GROUP_H
#define GROUP_H

#include "Buffer.h"
#include "Operator.h"
#include <vector>

namespace rtbot {
using std::vector;

template <class T> class Group : public Operator<T> {
  Buffer<T> buffer;

public:
  /**
   * agregates [t-lag+1,...,t]
   */
  Group(string const &id_, int channelSize_, int windowSize_)
        : Operator<T>(id_), buffer(channelSize_, windowSize_) {}

  void receive(int t, Buffer<T> const &msg) override {
    if (msg.channelSize != buffer.channelSize)
      throw std::invalid_argument("Group::receive: channelSize doesn't match");

    if (buffer.isEmpty())
        buffer=msg;
    else
        buffer.add(msg.getLag(-1));
    if (buffer.isFull())
        this->emit(t,buffer);
  }
};

} // end namespace rtbot


#endif // GROUP_H
