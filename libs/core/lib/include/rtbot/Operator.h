#ifndef OPERATOR_H
#define OPERATOR_H

#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/Message.h"

namespace rtbot {

using std::function;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

/**
 * Represents a generic operator that can receive a message and forward its
 * computed value to its children. This is one of the main building blocks of
 * rtbot framework.
 *
 * @tparam T Numeric type used for floating computations, (`float`, `double`,
 * etc.).
 */

template <class T>
class Operator;
template <class T = double>
using Op_ptr = unique_ptr<Operator<T>>;

template <class T = double>
class Operator {
  struct Connection {
    Operator<T>* const dest;
    int toPort = -1;
    int fromPort = -1;
  };

  vector<Connection> children;

 public:
  string id;
  function<T(T)> f;

  Operator() = default;
  explicit Operator(string const& id_) : id(id_) {}
  Operator(string const& id_, function<T(T)> f_) : id(id_), f(f_) {}
  virtual ~Operator() = default;

  virtual string typeName() const = 0;

  /**
   * Receives a message emitted from another operator. This method should be
   * implemented in concrete realizations of the `Operator` class. Here is where
   * the main logic of the operator is defined.
   *
   * @param msg {Buffer const &}  The message received by the operator in the
   * current processing cycle.
   * @param t {int} Timestamp of the message.
   */

  virtual map<string, std::vector<Message<T>>> receive(Message<T> const& msg, int port) { return receive(msg); }

  virtual map<string, std::vector<Message<T>>> receive(Message<T> const& msg) {
    auto out = msg;
    if (f) std::transform(msg.value.begin(), msg.value.end(), out.value.begin(), f);
    return emit(out);
  }

  map<string, std::vector<Message<T>>> emit(Message<T> const& msg) const {
    std::map<string, std::vector<Message<T>>> out = {{id,{msg}}};
    for (auto [child, to, _] : children)
      mergeOutput(out, child->receive(msg, to));
    return out;
  }

  map<string, std::vector<Message<T>>> emit(std::vector<Message<T>> const& msgs) const {
    std::map<string, std::vector<Message<T>>> out;
    for (const auto& msg: msgs)
      mergeOutput(out, emit(msg));
    return out;
  }

  Operator<T>& connect(Operator<T>& child, int toPort = -1, int fromPort = -1) {
    children.push_back({&child, toPort, fromPort});
    return child;
  }
  void connect(Operator<T>* const child, int toPort = -1, int fromPort = -1) {
    children.push_back({child, toPort, fromPort});
  }

 protected:
  static void mergeOutput(map<string, std::vector<Message<T>>>& out,
                          map<string, std::vector<Message<T>>> const& x)
  {
    for (const auto& [id,msgs] : x) {
      auto &vec=out[id];
      for (auto resultMessage : msgs)
        vec.push_back(resultMessage);
    }
  }
};

template <class T>
Operator<T>& operator|(Message<T> const& a, Operator<T>& B) {
  B.receive(a, nullptr);
  return B;
}

}  // end namespace rtbot

#endif  // OPERATOR_H
