#ifndef FINITE_IMPULSE_RESPONSE_H
#define FINITE_IMPULSE_RESPONSE_H

#include <cmath>
#include <vector>

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"

namespace rtbot {

// Custom feature set - we only need sum tracking
struct FIRFeatures {
  static constexpr bool TRACK_SUM = true;
  static constexpr bool TRACK_VARIANCE = false;
};

class FiniteImpulseResponse : public Buffer<NumberData, FIRFeatures> {
 public:
  FiniteImpulseResponse(std::string id, const std::vector<double>& coeffs)
      : Buffer<NumberData, FIRFeatures>(std::move(id), coeffs.size()), coeffs_(coeffs) {
    if (coeffs.empty()) {
      throw std::runtime_error("FIR coefficients vector cannot be empty");
    }
  }

  std::string type_name() const override { return "FiniteImpulseResponse"; }

  const std::vector<double>& get_coefficients() const { return coeffs_; }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData>* msg) override {
    std::vector<std::unique_ptr<Message<NumberData>>> output;

    // Only emit when buffer is full to ensure proper FIR calculation
    if (!buffer_full()) {
      return output;
    }

    // Calculate FIR output
    double result = 0.0;
    const auto& buf = buffer();

    for (size_t i = 0; i < coeffs_.size(); ++i) {
      result += coeffs_[i] * buf[buf.size() - 1 - i]->data.value;
    }

    output.push_back(create_message<NumberData>(msg->time, NumberData{result}));
    return output;
  }

 private:
  std::vector<double> coeffs_;
};

// Factory function
inline std::shared_ptr<FiniteImpulseResponse> make_fir(std::string id, const std::vector<double>& coeffs) {
  return std::make_shared<FiniteImpulseResponse>(std::move(id), coeffs);
}

}  // namespace rtbot

#endif  // FINITE_IMPULSE_RESPONSE_H