#include <complex>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
class FastFourrierTransform : public Operator<T, V> {
 public:
  FastFourrierTransform(string const& id, size_t N = 7, size_t skip = 127, bool emitPower = true,
                        bool emitRePart = false, bool emitImPart = false)
      : Operator<T, V>(id) {
    this->n = pow(2, N);
    this->emitPower = emitPower;
    this->emitRePart = emitRePart;
    this->emitImPart = emitImPart;
    // recall that the N passed in the constructor is used to compute the size of the input buffer
    // the actual size of the FFT is 2^N
    this->addDataInput("i1", this->n);
    // allocate the vector that will contain the FFT
    this->a = vector<complex<V>>(this->n);
    // declare the output ports based on the parameters passed in the constructor
    for (int i = 0; i < this->n; i++) {
      this->addOutput("w" + to_string(i + 1));
      if (emitPower) this->addOutput("p" + to_string(i + 1));
      if (emitRePart) this->addOutput("re" + to_string(i + 1));
      if (emitImPart) this->addOutput("im" + to_string(i + 1));
    }
  }

  string typeName() const override { return "FastFourrierTransform"; }

  map<string, vector<Message<T, V>>> processData() override {
    this->skipCounter++;

    cout << "skipCounter = " << this->skipCounter << endl;
    cout << "skip = " << this->skip << endl;
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
    for (int i = 0; i < this->n; i++) {
      this->a[i].real((input.at((this->n - 1) - i).value));
      this->a[i].imag(0);
    }

    // compute the FFT
    fft(this->a, false);

    auto time = this->getDataInputLastMessage(inputPort).time;
    for (size_t i = 0; i < this->n; i++) {
      cout << "a[" << i << "] = " << this->a[i] << endl;
      if (this->emitRePart) {
        Message<T, V> re(time, this->a[i].real());
        vector<Message<T, V>> toEmit = {re};
        outputMsgs.emplace("re" + to_string(i + 1), toEmit);
        cout << "re" << i + 1 << " = " << re.value << endl;
      }
      if (this->emitImPart) {
        Message<T, V> im(time, this->a[i].imag());
        vector<Message<T, V>> toEmit = {im};
        outputMsgs.emplace("re" + to_string(i + 1), toEmit);
      }
      if (this->emitPower) {
        Message<T, V> p(time, pow(this->a[i].real(), 2) + pow(this->a[i].imag(), 2));
        vector<Message<T, V>> toEmit = {p};
        outputMsgs.emplace("p" + to_string(i + 1), toEmit);
      }

      Message<T, V> w(time, i * 1.0 / this->n);
      vector<Message<T, V>> toEmit = {w};
      outputMsgs.emplace("w" + to_string(i + 1), toEmit);
    }

    for (const auto& pair : outputMsgs) {
      std::cout << pair.first << ": ";
      for (const auto& msg : pair.second) {
        std::cout << "Time: " << msg.time << ", Value: " << msg.value << "; ";
      }
      std::cout << std::endl;
    }

    return outputMsgs;
  }

 private:
  size_t skipCounter;
  size_t skip;
  size_t n;
  bool emitPower;
  bool emitRePart;
  bool emitImPart;
  vector<complex<V>> a;

  void fft(vector<complex<V>>& a, bool invert) {
    size_t n = a.size();
    if (n == 1) return;

    vector<complex<V>> a0(n / 2), a1(n / 2);
    for (int i = 0; 2 * i < n; i++) {
      a0[i] = a[2 * i];
      a1[i] = a[2 * i + 1];
    }
    fft(a0, invert);
    fft(a1, invert);

    V ang = 2 * M_PI / n * (invert ? -1 : 1);
    complex<V> w(1), wn(cos(ang), sin(ang));
    for (int i = 0; 2 * i < n; i++) {
      a[i] = a0[i] + w * a1[i];
      if (invert) a[i] /= 2;
      w *= wn;
    }
  }
};

}  // namespace rtbot