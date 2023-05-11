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
 * @tparam V Numeric type used for floating computations, (`float`, `double`,
 * etc.).
 */

template <class T, class V>
class Operator;
template <class T, class V>
using Op_ptr = unique_ptr<Operator<T, V>>;

template <class T, class V>
class Operator {
  struct Connection {
    Operator<T, V>* dest;
    int toPort = -1;
    int fromPort = -1;
  };

  vector<Connection> children;

 public:
  string id;
  function<V(V)> f;

  Operator() = default;
  explicit Operator(string const& id_) : id(id_) {}
  Operator(string const& id_, function<V(V)> f_) : id(id_), f(f_) {}
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

  virtual map<string, std::vector<Message<T, V>>> receive(Message<T, V> const& msg, int port) { return receive(msg); }

  virtual map<string, std::vector<Message<T, V>>> receive(Message<T, V> const& msg) {
    auto out = msg;
    if (f) out.value = f(msg.value);
    return emit(out);
  }

  map<string, std::vector<Message<T, V>>> emit(Message<T, V> const& msg) const {
    std::map<string, std::vector<Message<T, V>>> out = {{id, {msg}}};
    for (auto [child, to, _] : children) mergeOutput(out, child->receive(msg, to));
    return out;
  }

  map<string, std::vector<Message<T, V>>> emit(std::vector<Message<T, V>> const& msgs) const {
    std::map<string, std::vector<Message<T, V>>> out;
    for (const auto& msg : msgs) mergeOutput(out, emit(msg));
    return out;
  }

  map<string, std::vector<Message<T, V>>> emitParallel(vector<Message<T, V>> const& msgs) const {
    std::map<string, std::vector<Message<T, V>>> out = {{id, msgs}};
    for (auto [child, to, from] : children) mergeOutput(out, child->receive(msgs.at(from), to));
    return out;
  }

  Operator<T, V>& connect(Operator<T, V>& child, int toPort = -1, int fromPort = -1) {
    children.push_back({&child, toPort, fromPort});
    return child;
  }

  void connect(Operator<T, V>* const child, int toPort = -1, int fromPort = -1) {
    children.push_back({child, toPort, fromPort});
  }

 protected:
  static void mergeOutput(map<string, std::vector<Message<T, V>>>& out,
                          map<string, std::vector<Message<T, V>>> const& x) {
    for (const auto& [id, msgs] : x) {
      auto& vec = out[id];
      for (auto resultMessage : msgs) vec.push_back(resultMessage);
    }
  }
};

template <class T, class V>
Operator<T, V>& operator|(Message<T, V> const& a, Operator<T, V>& B) {
  B.receive(a, nullptr);
  return B;
}

}  // end namespace rtbot

#endif  // OPERATOR_H
