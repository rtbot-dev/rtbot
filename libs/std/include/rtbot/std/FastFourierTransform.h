#include <complex>
#include <valarray>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
class FastFourrierTransform : public Operator<T, V> {
 public:
  FastFourrierTransform(string const& id, size_t N = 7, size_t skip = 127, bool emitPower = true,
                        bool emitRePart = false, bool emitImPart = false)
      : Operator<T, V>(id) {
    this->N = N;
    this->M = pow(2, N);
    this->emitPower = emitPower;
    this->emitRePart = emitRePart;
    this->emitImPart = emitImPart;
    // recall that the N passed in the constructor is used to compute the size of the input buffer
    // the actual size of the FFT is 2^N
    this->addDataInput("i1", this->M);
    // allocate the vector that will contain the FFT
    this->a = vector<complex<V>>(this->M);
    // declare the output ports based on the parameters passed in the constructor
    for (int i = 0; i < this->M; i++) {
      this->addOutput("w" + to_string(i + 1));
      if (emitPower) this->addOutput("p" + to_string(i + 1));
      if (emitRePart) this->addOutput("re" + to_string(i + 1));
      if (emitImPart) this->addOutput("im" + to_string(i + 1));
    }
  }

  string typeName() const override { return "FastFourrierTransform"; }

  map<string, vector<Message<T, V>>> processData() override {
    this->skipCounter++;

    if (this->skipCounter < this->skip) return map<string, vector<Message<T, V>>>();

    this->skipCounter = 0;

    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    map<string, vector<Message<T, V>>> outputMsgs;

    auto input = this->dataInputs.find(inputPort)->second;
    for (int i = 0; i < this->M; i++) {
      this->a[i].real((input.at(i).value));
      this->a[i].imag(0);
    }

    // compute the FFT
    fft(this->a);

    auto time = this->getDataInputLastMessage(inputPort).time;
    for (size_t i = 0; i < this->M; i++) {
      if (this->emitRePart) {
        Message<T, V> re(time, this->a[i].real());
        vector<Message<T, V>> toEmit = {re};
        outputMsgs.emplace("re" + to_string(i + 1), toEmit);
      }
      if (this->emitImPart) {
        Message<T, V> im(time, this->a[i].imag());
        vector<Message<T, V>> toEmit = {im};
        outputMsgs.emplace("im" + to_string(i + 1), toEmit);
      }
      if (this->emitPower) {
        Message<T, V> p(time, pow(this->a[i].real(), 2) + pow(this->a[i].imag(), 2));
        vector<Message<T, V>> toEmit = {p};
        outputMsgs.emplace("p" + to_string(i + 1), toEmit);
      }

      Message<T, V> w(time, (i + 1.0) / this->M);
      vector<Message<T, V>> toEmit = {w};
      outputMsgs.emplace("w" + to_string(i + 1), toEmit);
    }

    return outputMsgs;
  }

  size_t getSize() { return this->M; }
  size_t getN() { return this->N; }
  bool getEmitPower() { return this->emitPower; }
  bool getEmitRePart() { return this->emitRePart; }
  bool getEmitImPart() { return this->emitImPart; }

 private:
  size_t skipCounter;
  size_t skip;
  size_t M;
  size_t N;
  bool emitPower;
  bool emitRePart;
  bool emitImPart;
  vector<complex<V>> a;

  // see https://rosettacode.org/wiki/Fast_Fourier_transform#C.2B.2B
  void fft(std::vector<std::complex<V>>& x) {
    // DFT
    unsigned int N = x.size(), k = N, n;
    double thetaT = 3.14159265358979323846264338328L / N;
    std::complex<V> phiT = std::complex<V>(cos(thetaT), -sin(thetaT)), Tt;
    while (k > 1) {
      n = k;
      k >>= 1;
      phiT = phiT * phiT;
      Tt = 1.0L;
      for (unsigned int l = 0; l < k; l++) {
        for (unsigned int a = l; a < N; a += n) {
          unsigned int b = a + k;
          std::complex<V> t = x[a] - x[b];
          x[a] += x[b];
          x[b] = t * Tt;
        }
        Tt *= phiT;
      }
    }
    // Decimate
    unsigned int m = (unsigned int)log2(N);
    for (unsigned int a = 0; a < N; a++) {
      unsigned int b = a;
      // Reverse bits
      b = (((b & 0xaaaaaaaa) >> 1) | ((b & 0x55555555) << 1));
      b = (((b & 0xcccccccc) >> 2) | ((b & 0x33333333) << 2));
      b = (((b & 0xf0f0f0f0) >> 4) | ((b & 0x0f0f0f0f) << 4));
      b = (((b & 0xff00ff00) >> 8) | ((b & 0x00ff00ff) << 8));
      b = ((b >> 16) | (b << 16)) >> (32 - m);
      if (b > a) {
        std::complex<V> t = x[a];
        x[a] = x[b];
        x[b] = t;
      }
    }
  }
};

}  // namespace rtbot