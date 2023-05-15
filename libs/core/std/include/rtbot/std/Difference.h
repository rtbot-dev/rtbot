#ifndef DIFFERENCE_H
#define DIFFERENCE_H

#include "rtbot/Join.h"

namespace rtbot {

/**
 * @brief The Difference class as example of application of Join
 */
template <class T, class V>
struct Difference : public Join<T, V> {
  Difference() = default;
  Difference(string const &id_) : Join<T, V>(id_, 2) {}

  string typeName() const override { return "Difference"; }

  map<string, std::vector<Message<T, V>>> processData(vector<Message<T, V>> const &msgs) override {
    Message<T, V> out(msgs.at(0).time, msgs.at(0).value - msgs.at(1).value);
    return this->emit(out);
  }
};

}  // namespace rtbot

#endif  // DIFFERENCE_H