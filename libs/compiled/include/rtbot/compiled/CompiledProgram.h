#ifndef RTBOT_COMPILED_PROGRAM_H
#define RTBOT_COMPILED_PROGRAM_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace rtbot::compiled {

// Per-emission output record: timestamp + N output doubles.
template <std::size_t N>
struct EmittedRecord {
  std::int64_t time;
  double values[N];
};

// CompiledProgram<Pipeline, NumOutputs> wraps a hand-composed pipeline
// struct (Pipeline) that exposes:
//
//     template <class F> void process(int64_t t, double v, F&& emit);
//
// where `emit` is invoked as emit(int64_t, double, ..., double) with
// NumOutputs trailing doubles. CompiledProgram captures emissions into an
// output buffer the caller drains via collect_outputs().
//
// Public surface mirrors Program's most-used methods so callers can swap
// backends with minimal code changes. (Unsupported features in this phase:
// JSON serialization, snapshot/restore, multi-input programs at the
// CompiledProgram boundary — only single-NumberData input supported here.)
template <class Pipeline, std::size_t NumOutputs>
class CompiledProgram {
 public:
  using Record = EmittedRecord<NumOutputs>;

  CompiledProgram() = default;

  template <class... Args>
  explicit CompiledProgram(Args&&... pipeline_args)
      : pipeline_(std::forward<Args>(pipeline_args)...) {}

  inline void receive(std::int64_t t, double v) noexcept {
    pipeline_.process(t, v, [this](auto... args) {
      this->capture(args...);
    });
  }

  // Pop all outputs accumulated since the last call, in emission order.
  std::vector<Record> collect_outputs() {
    std::vector<Record> result;
    result.swap(buffer_);
    return result;
  }

 private:
  // Variadic capture: time followed by exactly NumOutputs doubles.
  template <class... Doubles>
  inline void capture(std::int64_t t, Doubles... vs) noexcept {
    static_assert(sizeof...(Doubles) == NumOutputs,
                  "Pipeline emit arity does not match CompiledProgram NumOutputs");
    Record r{};
    r.time = t;
    fill_values(r, 0, vs...);
    buffer_.push_back(r);
  }

  static inline void fill_values(Record&, std::size_t) noexcept {}
  template <class... Tail>
  static inline void fill_values(Record& r, std::size_t i, double head, Tail... tail) noexcept {
    r.values[i] = head;
    fill_values(r, i + 1, tail...);
  }

  Pipeline pipeline_;
  std::vector<Record> buffer_;
};

}  // namespace rtbot::compiled

#endif  // RTBOT_COMPILED_PROGRAM_H
