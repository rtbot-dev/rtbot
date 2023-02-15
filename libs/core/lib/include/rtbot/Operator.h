#ifndef OPERATOR_H
#define OPERATOR_H

#include "rtbot/Message.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <limits>
#include <functional>

namespace rtbot {

using std::string;
using std::vector;
using std::function;


/**
 * Represents a genereric operator that can receive a message and forward its
 * computed value to its children. This is one of the main building blocks of
 * rtbot framework.
 *
 * @tparam T Numeric type used for floating computations, (`float`, `double`,
 * etc.).
 */
template <class T> class Operator {
    vector<Operator<T> *> children;

public:
  string id;
  function<T(T)> f;

  Operator()=default;
  explicit Operator(string const &id_) : id(id_) {}
  Operator(string const &id_, function<T(T)> f_) : id(id_), f(f_) {}
  virtual ~Operator()=default;

  /**
   * Receives a message emitted from another operator. This method should be
   * implemented in concrete realizations of the `Operator` class. Here is where
   * the main logic of the operator is defined.
   *
   * @param msg {Buffer const &}  The message received by the operator in the
   * current processing cycle.
   * @param t {int} Timestamp of the message.
   */
  virtual void receive(Message<T> const& msg, const Operator<T> *sender=nullptr)
  {
      auto out=msg;
      if (f)
          std::transform(msg.value.begin(), msg.value.end(),
                         out.value.begin(),f);
      emit(out);
  }

  void emit(Message<T> const& msg) const {
    for (auto x : children)
      x->receive(msg,this);
  }

  friend void connect(Operator<T>* from, Operator<T>* to) { from->addChildren(to); to->addSender(from); }

  protected:
  virtual void addChildren(Operator<T>* child) { children.push_back(child); }
  virtual void addSender(const Operator<T>*) { }
};

template<class T>
Operator<T>& operator|(Operator<T>& A, Operator<T>& B) { connect(&A,&B); return B; }

template<class T>
Operator<T>& operator|(Message<T> const& a, Operator<T>& B) { B.receive(a, nullptr); return B; }

template<class T>
struct Input: public Operator<T>
{
    using Operator<T>::Operator;
    void receive(Message<T> const& msg, const Operator<T> *sender=nullptr) override { if (int64_t(msg.time)<=t0) return; t0=msg.time; this->emit(msg); }

private:
    std::int64_t t0 = std::numeric_limits<int64_t>::lowest();
};


} // end namespace rtbot


#endif // OPERATOR_H
