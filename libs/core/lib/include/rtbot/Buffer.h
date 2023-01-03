#include <stdexcept>
#include <vector>

namespace rtbot {

using std::vector;
using std::string;

template <class T> class Buffer {
public:
  int channelSize;
  int windowSize;

  Buffer(int channelSize_, int windowSize_)
      : channelSize(channelSize_), windowSize(windowSize_),
        data(channelSize_ * windowSize_, T(0)) {}

  /// @brief element at channel i and time stamp j
  T &operator()(int i, int j) {
      int col=to_matrix_column(j);
      if (col>sz)
          throw std::invalid_argument("Buffer(i,j) with index j out of bound");
      return data.at(i + col * channelSize);
  }
  const T &operator()(int i, int j) const {
      int col=to_matrix_column(j);
      if (col>=sz)
          throw std::invalid_argument("Buffer(i,j) with index j out of bound");
      return data.at(i + col * channelSize);
  }

  /// returns a printable list of list
  vector<vector<T>> getData() const
  {
     vector<vector<T>> out(sz, vector<T>(channelSize));
     for(auto j=0; j<sz; j++)
          for(auto i=0; i<channelSize; i++)
              out[j][i]=(*this)(i,j);
      return out;
  }

  /// Add data to last time stamp of the buffer
  void add(vector<T> const &data) {
    if ( int(data.size()) != channelSize)
      throw std::invalid_argument(
          "Buffer::add() with wrong number of channels");
    for (int i = 0; i < channelSize; i++)
      (*this)(i, p0) = data[i];
    p0 = (p0 + 1) % windowSize;
    if (sz<windowSize) sz++;
  }

private:
  vector<T> data;
  int p0 = 0; ///< column position to put the new data arriving
  int sz=0;

  /// @brief  converts
  /// @param p time stamp position
  /// @return actual column number in the buffer matrix
  int to_matrix_column(int p) const {
      if (sz<windowSize)
          return (p+windowSize)%windowSize;
      return (p + p0 +  windowSize) % windowSize;
  }
};
} // namespace rtbot
