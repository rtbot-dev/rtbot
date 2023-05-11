#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <memory>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Composite : public Operator<T, V>  // TODO: improve from chain to graph
{
  Composite() = default;

  vector<Op_ptr<T, V>> op;
  Composite(string const &id_, vector<Op_ptr<T, V>> &&op_) : Operator<T, V>(id_), op(std::move(op_)) {
    for (auto i = 0u; i + 1 < op.size(); i++) op[i]->connect(op[i + 1].get());
  }

  string typeName() const override { return "Composite"; }

  virtual ~Composite() = default;

  virtual map<string, vector<Message<T, V>>> receive(Message<T, V> const &msg) override {
    auto out = op.front()->receive(msg);

    auto it = out.find(op.back()->id);

    if (it != out.end()) {
      std::cout << "here empty inside" << std::endl;
      return this->emit(it->second);
    } else
      return {};
  }
};

}  // namespace rtbot

#endif  // COMPOSITE_H
