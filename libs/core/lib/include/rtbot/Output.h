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

template <class T>
std::ostream& operator<<(std::ostream& out, Message<T> const& msg) {
  out << msg.time << " " << msg.value;
  return out;
}

template <class T = double>
struct Output_vec : public Operator<T> {
  std::vector<Message<T>>* out = nullptr;

  Output_vec() = default;
  Output_vec(string const& id_, std::vector<Message<T>>& out_) : Operator<T>(id_), out(&out_) {}

  string typeName() const override { return "Output"; }

  map<string, std::vector<Message<T>>> receive(Message<T> const& msg) override {
    out->push_back(msg);
    return this->emit(msg);
  }
};

template <class T = double>
struct Output_opt : public Operator<T> {
  std::optional<Message<T>>* out = nullptr;

  Output_opt() = default;
  Output_opt(string const& id_, std::optional<Message<T>>& out_) : Operator<T>(id_), out(&out_) {}

  string typeName() const override { return "Output"; }

  map<string, std::vector<Message<T>>> receive(Message<T> const& msg) override {
    *out = msg;
    return this->emit(msg);
  }
};

template <class T = double>
struct Output_os : public Operator<T> {
  std::ostream* out = nullptr;

  Output_os() = default;
  Output_os(string const& id_, std::ostream& out_) : Operator<T>(id_), out(&out_) {}

  string typeName() const override { return "Output"; }

  map<string, std::vector<Message<T>>> receive(Message<T> const& msg) override {
    (*out) << this->id << " " << msg << "\n";
    return this->emit(msg);
  }
};

}  // end namespace rtbot

#endif  // OUTPUT_H
