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
  Group(int channelSize_, int windowSize_)
      : buffer(channelSize_, windowSize_) {}

  void receive(int t, Buffer<T> const &msg) override {
    if (msg.channelSize != buffer.channelSize)
      throw std::invalid_argument("Group::receive: channelSize doesn't match");

    vector<T> data(msg.channelSize);
    for (auto i = 0u; i < data.size(); i++)
      data[i] = msg(i, -1);
    buffer.add(data);
    this->emit(buffer);
  }
};

} // end namespace rtbot