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
    std::vector<Message<>> msgs;
    msgs.push_back(out);
    return emit(msgs);
  }

  map<string, std::vector<Message<T>>> emit(std::vector<Message<T>> const& msgs) const {
    std::map<string, std::vector<Message<T>>> out;
    out.insert(std::pair<string, std::vector<Message<T>>>(id, msgs));
    for (unsigned int i = 0; i < msgs.size(); i++) {
      for (auto [child, to, from] : children) {
        auto outi = child->receive(msgs.at(i), to);
        for (const auto& it : outi) {
          // first time we insert an output of a child operator
          if (auto it2=out.find(it.first); it2 == out.end()) {
            out.emplace(it);
          } else {
            // entry already on map, push the result to the correspondent vector
            for (auto resultMessage : it.second) {
              it2->second.push_back(resultMessage);
            }
          }
        }
      }
    }
    return out;
  }

  Operator<T>& connect(Operator<T>& child, int toPort = -1, int fromPort = -1) {
    children.push_back({&child, toPort, fromPort});
    return child;
  }
  void connect(Operator<T>* const child, int toPort = -1, int fromPort = -1) {
    children.push_back({child, toPort, fromPort});
  }
};

template <class T>
Operator<T>& operator|(Message<T> const& a, Operator<T>& B) {
  B.receive(a, nullptr);
  return B;
}

}  // end namespace rtbot

#endif  // OPERATOR_H
