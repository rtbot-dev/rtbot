#ifndef TOPK_H
#define TOPK_H

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

// TopK: maintains the top-K entries by a score field, emitting a full snapshot
// of all K entries on each input. Uses a sorted vector (best first, worst last).
class TopK : public Operator {
 public:
  TopK(std::string id, int k, int score_index, bool descending)
      : Operator(std::move(id)),
        k_(k),
        score_index_(score_index),
        descending_(descending) {
    if (k_ <= 0) throw std::runtime_error("TopK: k must be positive");
    if (score_index_ < 0)
      throw std::runtime_error("TopK: score_index must be non-negative");
    add_data_port<VectorNumberData>();
    add_output_port<VectorNumberData>();
  }

  std::string type_name() const override { return "TopK"; }
  int k() const { return k_; }
  int score_index() const { return score_index_; }
  bool descending() const { return descending_; }

  Bytes collect_bytes() override {
    Bytes bytes = Operator::collect_bytes();
    size_t n = top_k_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&n),
                 reinterpret_cast<const uint8_t*>(&n) + sizeof(n));
    for (const auto& row : top_k_) {
      size_t m = row.size();
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&m),
                   reinterpret_cast<const uint8_t*>(&m) + sizeof(m));
      for (double v : row) {
        bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&v),
                     reinterpret_cast<const uint8_t*>(&v) + sizeof(v));
      }
    }
    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);
    size_t n;
    std::memcpy(&n, &(*it), sizeof(n));
    it += sizeof(n);
    top_k_.clear();
    top_k_.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      size_t m;
      std::memcpy(&m, &(*it), sizeof(m));
      it += sizeof(m);
      std::vector<double> row(m);
      for (size_t j = 0; j < m; ++j) {
        std::memcpy(&row[j], &(*it), sizeof(double));
        it += sizeof(double);
      }
      top_k_.push_back(std::move(row));
    }
  }

 protected:
  void process_data(bool /*debug*/ = false) override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = static_cast<const Message<VectorNumberData>*>(
          input_queue.front().get());
      if (!msg) throw std::runtime_error("TopK: invalid message type");

      const auto& values = *msg->data.values;
      if (score_index_ >= static_cast<int>(values.size())) {
        throw std::runtime_error("TopK: score_index out of bounds");
      }

      insert_into_top_k(values);

      for (const auto& row : top_k_) {
        output_queue.push_back(create_message<VectorNumberData>(
            msg->time, VectorNumberData{row}));
      }

      input_queue.pop_front();
    }
  }

 private:
  int k_;
  int score_index_;
  bool descending_;
  std::vector<std::vector<double>> top_k_;  // sorted: best first, worst last

  void insert_into_top_k(const std::vector<double>& entry) {
    // lower_bound finds first element where comp(elem, entry) is false.
    // descending: comp = existing_score > new_score → first where existing <= new
    // ascending:  comp = existing_score < new_score → first where existing >= new
    auto it = std::lower_bound(
        top_k_.begin(), top_k_.end(), entry,
        [this](const std::vector<double>& existing,
               const std::vector<double>& val) {
          double es = existing[score_index_];
          double vs = val[score_index_];
          return descending_ ? (es > vs) : (es < vs);
        });
    top_k_.insert(it, entry);
    if (static_cast<int>(top_k_.size()) > k_) {
      top_k_.pop_back();  // evict worst (always at back)
    }
  }
};

inline std::shared_ptr<TopK> make_topk(std::string id, int k, int score_index,
                                        bool descending = true) {
  return std::make_shared<TopK>(std::move(id), k, score_index, descending);
}

}  // namespace rtbot

#endif  // TOPK_H
