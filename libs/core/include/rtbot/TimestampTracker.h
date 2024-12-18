#ifndef TIMESTAMP_TRACKER_H
#define TIMESTAMP_TRACKER_H

#include <limits>
#include <map>
#include <optional>
#include <set>
#include <type_traits>

namespace rtbot {

using timestamp_t = std::int64_t;

class TimestampTracker {
 public:
  template <typename TimeMap>
  static std::optional<timestamp_t> find_oldest_common_time(
      const TimeMap& tracker, timestamp_t min_time = std::numeric_limits<timestamp_t>::min()) {
    if (tracker.empty()) {
      return std::nullopt;
    }

    // Create set of timestamps >= min_time from first port
    std::set<timestamp_t> common_times = extract_timestamps(tracker.begin()->second, min_time);

    // Intersect with timestamps from other ports
    for (auto it = std::next(tracker.begin()); it != tracker.end(); ++it) {
      std::set<timestamp_t> port_times = extract_timestamps(it->second, min_time);

      std::set<timestamp_t> intersection;
      std::set_intersection(common_times.begin(), common_times.end(), port_times.begin(), port_times.end(),
                            std::inserter(intersection, intersection.begin()));
      common_times = std::move(intersection);

      if (common_times.empty()) {
        return std::nullopt;
      }
    }

    return common_times.empty() ? std::nullopt : std::optional{*common_times.begin()};
  }

 private:
  // Helper for std::set<timestamp_t>
  static std::set<timestamp_t> extract_timestamps(const std::set<timestamp_t>& times, timestamp_t min_time) {
    std::set<timestamp_t> result;
    for (const auto& time : times) {
      if (time >= min_time) {
        result.insert(time);
      }
    }
    return result;
  }

  // Helper for std::map<timestamp_t, T>
  template <typename T>
  static std::set<timestamp_t> extract_timestamps(const std::map<timestamp_t, T>& times, timestamp_t min_time) {
    std::set<timestamp_t> result;
    for (const auto& [time, _] : times) {
      if (time >= min_time) {
        result.insert(time);
      }
    }
    return result;
  }
};

}  // namespace rtbot

#endif  // TIMESTAMP_TRACKER_H