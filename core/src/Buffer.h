#include <stdexcept>
#include <vector>

namespace rtbot {

using std::vector;

template <class T> class Buffer {
public:
  int channelSize;
  int windowSize;

  Buffer(int channelSize_, int windowSize_)
      : channelSize(channelSize_), windowSize(windowSize_),
        data(channelSize_ * windowSize_, T(0)) {}

  /// @brief element at channel i and time stamp j
  T &operator()(int i, int j) {
    return data.at(i + to_matrix_column(j) * channelSize);
  }
  const T &operator()(int i, int j) const {
    return data.at(i + to_matrix_column(j) * channelSize);
  }

  /// @brief Add data to last time stamp of the buffer
  void add(vector<T> const &data) {
    if ( int(data.size()) != channelSize)
      throw std::invalid_argument(
          "Buffer::add() with wrong number of channels");
    for (int i = 0; i < channelSize; i++)
      (*this)(i, p0) = data[i];
    p0 = (p0 + 1) % windowSize;
  }

private:
  vector<T> data;
  int p0 = 0; ///< column position to put the new data arriving

  /// @brief  converts
  /// @param p time stamp position
  /// @return actual column number in the buffer matrix
  int to_matrix_column(int p) const {
    return (p + p0 + windowSize) % windowSize;
  }
};
} // namespace rtbot