#ifndef OUTPUT_H
#define OUTPUT_H

#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <variant>
#include <vector>

#include "Operator.h"

namespace rtbot {

using std::function;

template <class T, class V>
std::ostream& operator<<(std::ostream& out, Message<T, V> const& msg) {
  out << msg.time << " " << msg.value;
  return out;
}

template <class T, class V>
struct Output_vec : public Operator<T, V> {
  Output_vec() = default;
  Output_vec(string const& id_, size_t n_) : Operator<T, V>(id_) {
    this->addInput("i1", n_);
    this->addOutput("o1");
  }

  string typeName() const override { return "Output"; }

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> toEmit = this->getLastMessage(inputPort);
    return this->emit(toEmit);
  }
};

template <class T, class V>
struct Output_opt : public Operator<T, V> {
  std::optional<Message<T, V>>* out = nullptr;

  Output_opt() = default;
  Output_opt(string const& id_, std::optional<Message<T, V>>& out_) : Operator<T, V>(id_), out(&out_) {}
  Output_opt(string const& id_) : Operator<T, V>(id_) {}

  string typeName() const override { return "Output"; }

  map<string, std::vector<Message<T, V>>> receive(Message<T, V> const& msg) override {
    *out = msg;
    return this->emit(msg);
  }
};

template <class T, class V>
struct Output_os : public Operator<T, V> {
  std::ostream* out = nullptr;

  Output_os() = default;
  Output_os(string const& id_, std::ostream& out_) : Operator<T, V>(id_), out(&out_) {
    this->addInput("i1", 1);
    this->addOutput("o1");
  }

  string typeName() const override { return "Output"; }

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> toEmit = this->getLastMessage(inputPort);
    (*out) << this->id << " " << toEmit << "\n";
    return this->emit(toEmit);
  }
};

}  // end namespace rtbot

#endif  // OUTPUT_H
