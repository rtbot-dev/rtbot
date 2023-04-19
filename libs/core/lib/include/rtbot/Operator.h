#ifndef OPERATOR_H
#define OPERATOR_H

#include "rtbot/Message.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <limits>
#include <functional>
#include <memory>

namespace rtbot {

using std::string;
using std::vector;
using std::function;
using std::map;
using std::unique_ptr;


/**
 * Represents a genereric operator that can receive a message and forward its
 * computed value to its children. This is one of the main building blocks of
 * rtbot framework.
 *
 * @tparam T Numeric type used for floating computations, (`float`, `double`,
 * etc.).
 */


template <class T> class Operator;
template <class T=double> using Op_ptr=unique_ptr<Operator<T>>;

template <class T=double> class Operator {
    vector<Operator<T> *> children;

public:

  string id;
  function<T(T)> f;

  Operator()=default;
  explicit Operator(string const &id_) : id(id_) {}
  Operator(string const &id_, function<T(T)> f_) : id(id_), f(f_) {}
  virtual ~Operator()=default;

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
  virtual map<string,Message<T>> receive(Message<T> const& msg, const Operator<T> *sender=nullptr)
  {
      auto out=msg;
      if (f)
          std::transform(msg.value.begin(), msg.value.end(),
                         out.value.begin(),f);
      std::vector<Message<>> msgs;
      msgs.push_back(out);                   
      return emit(msgs);
  }

  map<string,Message<T>> emit(std::vector<Message<T>> const& msgs) const {
     
      std::map<string,Message<T>> out;
      for(unsigned int i=0; i < msgs.size(); i++) {
        out.insert(std::pair<string,Message<T>>(id,msgs.at(i))); 
        for (auto x : children) {
            auto outi=x->receive(msgs.at(i),this);
            for(const auto& it : outi)
                out.emplace(it);
        }
      }
      return out;
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


} // end namespace rtbot


#endif // OPERATOR_H
