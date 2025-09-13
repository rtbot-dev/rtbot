#ifndef IGNORE_H
#define IGNORE_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class Ignore : public Operator {
 public:
  Ignore(std::string id, size_t count = 0) : Operator(std::move(id)), count_(count), ignored_(0) {
    // Single input and output port
    add_data_port<NumberData>();
    add_control_port<NumberData>();
    add_output_port<NumberData>();
  }

  void reset() override {
    Operator::reset();
    ignored_ = 0;
  }

  std::string type_name() const override { return "Ignore"; }

  size_t get_count() const { return count_; }

  Bytes collect() override {
    Bytes bytes = Operator::collect();

    // Serialize count_
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&count_),
                 reinterpret_cast<const uint8_t*>(&count_) + sizeof(count_));

    // Serialize ignored_
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&ignored_),
                 reinterpret_cast<const uint8_t*>(&ignored_) + sizeof(ignored_));

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    // Restore count_
    count_ = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    // Restore ignored_
    ignored_ = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);
  }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in Ignore");
      }

      if (ignored_ >= count_) {
        // Forward message by cloning
        output_queue.push_back(input_queue.front()->clone());
      } else
        ignored_++;
      input_queue.pop_front();
    }
  }

  void process_control() override {
    auto& control_queue = get_control_queue(0);

    while (!control_queue.empty()) {
      const auto* query = dynamic_cast<const Message<NumberData>*>(control_queue.front().get());
      if (!query) {
        throw std::runtime_error("Invalid control message type in Ignore");
      }
      ignored_ = 0;
      control_queue.pop_front();
    }
  }

 private:
  size_t count_;
  size_t ignored_;
};

// Factory function
inline std::shared_ptr<Ignore> make_ignore(std::string id, size_t count = 0) {
  return std::make_shared<Ignore>(std::move(id), count);
}

}  // namespace rtbot

#endif  // IDENTITY_H