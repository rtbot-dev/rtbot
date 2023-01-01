
#include "Buffer.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace rtbot {
using std::string;
using std::vector;

/**
 * Represents a genereric operator that can receive a message and forward its
 * computed value to its children. This is one of the main building blocks of
 * rtbot framework.
 *
 * @tparam T Numeric type used for floating computations, (`float`, `double`,
 * etc.).
 */
template <class T> class Op {
  vector<Op *> children;

public:
  const string id;

  Op(string const &id_) : id(id_) {}

  /**
   * Receives a message emitted from another operator. This method should be
   * implemented in concrete realizations of the `Operator` class. Here is where
   * the main logic of the operator is defined.
   *
   * @param msg {Buffer const &}  The message received by the operator in the
   * current processing cycle.
   * @param t {int} Timestamp of the message.
   */
  virtual void receive(int t, Buffer<T> const &msg) = 0;

  void emit(int t, Buffer<T> const &msg) const {
    for (auto x : children)
      x->receive(t, msg);
  }

  void addChildren(Op *const child) { children.push_back(child); }
};

template class Buffer<double>;
template class Op<double>;

} // end namespace rtbot