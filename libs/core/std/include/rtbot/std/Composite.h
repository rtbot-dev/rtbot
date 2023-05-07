#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <memory>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Composite : public Operator<T, V>  // TODO: improve from chain to graph
{
  vector<Op_ptr<T, V>> op;
  Composite(string const &id_, vector<Op_ptr<T, V>> &&op_) : Operator<T, V>(id_), op(op_) {
    for (auto i = 0u; i + 1 < op.size(); i++) connect(op[i], op[i + 1]);
  }

  virtual ~Composite() = default;

  virtual map<string, Message<T, V>> receive(Message<T, V> const &msg, int port) {
    auto out = op.front().receive(msg, port);
    if (auto it = out.find(op.back().id);
        it != out.end())  // check if the message reached the last operator of the composite
      emit(it.second);
    else
      return {};
  }
};

}  // namespace rtbot

#endif  // COMPOSITE_H
