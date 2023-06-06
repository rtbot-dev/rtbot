#ifndef MINUS_H
#define MINUS_H

#include "rtbot/Joint.h"

namespace rtbot {

/**
 * @brief The Difference class as example of application of Join
 */
template <class T, class V>
struct Minus : public Joint<T, V> {
  Minus() = default;
  Minus(string const &id_) {
    this->id = id_;
    this->addInput("i1");
    this->addInput("i2");
    this->addOutput("o1");
  }

  string typeName() const override { return "Minus"; }

  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> m1 = this->getMessage("i2", 0);
    Message<T, V> m0 = this->getMessage("i1", 0);
    Message<T, V> out(m0.time, m0.value - m1.value);
    map<string, vector<Message<T, V>>> toEmit;
    vector<Message<T, V>> v;
    v.push_back(out);
    toEmit.emplace("o1", v);
    return toEmit;
  }
};

}  // namespace rtbot

#endif  // MINUS_H