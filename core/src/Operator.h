
#include <string>
#include <vector>

namespace rtbot {
using std::string;
using std::vector;

/// @brief Base class
/// @tparam T
template <class T> class Buffer {
  vector<T> data;
  int pos = 0;

public:
  int channelSize;
  int windowSize;

  Buffer(int channelSize_, int windowSize_)
      : data(channelSize_ * windowSize_, T(0)), channelSize(channelSize_),
        windowSize(windowSize_) {}

  /// @brief element at channel i and time stamp j
  T &operator(int i, int j) { return data.at(i + j * channelSize); }
  const T &operator(int i, int j) const { return data.at(i + j * channelSize); }

  /// @brief The message msg at the last
  /// @param values
  void add(const vector<T> &msg) {
    if (msg.size() != channelSize)
      throw std::__throw_invalid_argument(
          "Buffer::add() with wrong number of channels");
    for (int i = 0; i < channelSize; i++)
      (*this)(i, pos) = msg[i];
    pos = (pos + 1) % windowSize;
  }

private:
};

template <class T> class Op {
  vector<Op> children;

public:
  const string id;

  Op(string const &id_) : id(id_) {}

  virtual void receive(int t, Buffer const &msg) = 0;

  void emit(int t, Buffer const &msg) const {
    for (const Op &x : children)
      x.receive(t, msg);
  }

  void addChildren(Op const &child) { children.push_back(child); }
};

class MA : public Op<double> {

public:
  struct Param {
    int backward = 0;
    int forward = 0;
  };

  MA(string id_, Param const &pars_ = {}) : Op<double>(id_), pars(pars_) {}

private:
  Param pars;
};

template class Buffer<double>;
template class Op<double>;

} // end namespace rtbot