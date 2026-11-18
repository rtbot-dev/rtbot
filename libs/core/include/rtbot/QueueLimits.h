#ifndef RTBOT_QUEUE_LIMITS_H
#define RTBOT_QUEUE_LIMITS_H

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace rtbot {

class QueueLimitExceeded : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

enum class QueueOverflowPolicy { Error, DropOldest };

struct QueueLimits {
  // The legacy engine's per-port bound. Zero explicitly opts out.
  size_t max_messages = 17280;
  QueueOverflowPolicy overflow = QueueOverflowPolicy::Error;

  QueueOverflowPolicy restore_overflow = QueueOverflowPolicy::Error;

  static QueueLimits from_environment() {
    return parse(std::getenv("RTBOT_MAX_QUEUE_MESSAGES"), std::getenv("RTBOT_QUEUE_OVERFLOW"),
                 std::getenv("RTBOT_QUEUE_RESTORE_OVERFLOW"));
  }

  static QueueLimits parse(const char* maximum, const char* policy, const char* restore_policy = nullptr) {
    QueueLimits limits;
    if (maximum) {
      const std::string text(maximum);
      if (text.empty()) throw std::invalid_argument("RTBOT_MAX_QUEUE_MESSAGES must be an unsigned integer");
      size_t value = 0;
      for (const char c : text) {
        if (c < '0' || c > '9' || value > (std::numeric_limits<size_t>::max() - (c - '0')) / 10)
          throw std::invalid_argument("RTBOT_MAX_QUEUE_MESSAGES must be an unsigned integer within size_t range");
        value = value * 10 + (c - '0');
      }
      limits.max_messages = value;
    }
    if (policy) {
      const std::string text(policy);
      if (text == "error") limits.overflow = QueueOverflowPolicy::Error;
      else if (text == "drop_oldest") limits.overflow = QueueOverflowPolicy::DropOldest;
      else throw std::invalid_argument("RTBOT_QUEUE_OVERFLOW must be error or drop_oldest");
    }
    if (restore_policy) {
      const std::string text(restore_policy);
      if (text == "drop_oldest") limits.restore_overflow = QueueOverflowPolicy::DropOldest;
      else if (text != "error") throw std::invalid_argument("RTBOT_QUEUE_RESTORE_OVERFLOW must be error or drop_oldest");
    }
    return limits;
  }
};

struct QueueStatistics {
  size_t peak_messages = 0;
  size_t dropped_messages = 0;
  size_t rejected_messages = 0;
};

}  // namespace rtbot
#endif
