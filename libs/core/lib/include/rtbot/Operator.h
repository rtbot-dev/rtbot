#ifndef OPERATOR_H
#define OPERATOR_H


#include <stdexcept>
#include <string>
#include <vector>
#include <map>

namespace rtbot {

using std::string;
using std::vector;


template<class T=double>
struct Message {
    int time;
    std::vector<T> value;

    Message()=default;
    Message(int time_, T value_): time(time_), value(1,value_) {}
    Message(int time_, vector<T> const& value_): time(time_), value(value_) {}
};

template<class T>
bool operator==(Message<T> const& a, Message<T> const& b) { return a.time==b.time && a.value==b.value; }




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
protected:
    std::map<const Operator* const, int> parents; //< an index for each parent

public:
  const string id;

  Operator(string const &id_) : id(id_) {}

  /**
   * Receives a message emitted from another operator. This method should be
   * implemented in concrete realizations of the `Operator` class. Here is where
   * the main logic of the operator is defined.
   *
   * @param msg {Buffer const &}  The message received by the operator in the
   * current processing cycle.
   * @param t {int} Timestamp of the message.
   */
  virtual void receive(Message<T> const& msg, const Operator<T> *sender=nullptr) = 0;

  void emit(Message<T> const& msg) const {
    for (auto x : children)
      x->receive(msg,this);
  }

  void addChildren(Operator<T> * child) { children.push_back(child); child->parents[this]=child->parents.size(); }
};


template<class T>
struct Input: public Operator<T>
{
    using Operator<T>::Operator;
    void receive(Message<T> const& msg, const Operator<T> *sender=nullptr) override { this->emit(msg); }
};


} // end namespace rtbot


#endif // OPERATOR_H
