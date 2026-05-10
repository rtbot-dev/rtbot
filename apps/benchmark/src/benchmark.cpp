#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/PerfCounters.h"
#include "rtbot/Program.h"
#include "rtbot/bindings.h"
#include "rtbot/compiled/ArithmeticStage.h"
#include "rtbot/compiled/CompiledProgram.h"
#include "rtbot/compiled/JoinStage.h"
#include "rtbot/compiled/MovingAverageStage.h"
#include "rtbot/compiled/PeakDetectorStage.h"
#include "rtbot/compiled/ResamplerHermiteStage.h"
#include "rtbot/compiled/ScaleStage.h"
#include "rtbot/compiled/StdDevStage.h"
#include "rtbot/compiled/jit/JitCompiler.h"
#include "libs/compiled/jit_spike/BollingerJit.h"
#include "libs/compiled/jit_spike/JitContext.h"
#include "rtbot/fuse/FusedExpression.h"
#include "rtbot/std/ArithmeticSync.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "tools.h"

using namespace rtbot;

std::string format_throughput(double messages_per_second) {
  std::ostringstream os;
  os.imbue(std::locale(""));
  os << std::fixed << std::setprecision(0) << messages_per_second << " msgs/s";
  return os.str();
}

// Common utility classes
class BenchmarkResults {
 public:
  // Store a vector of per-run times (ns). Median/min/max are derived on print.
  // Spread % = 100 * (max - min) / median — useful noise-floor indicator.
  void add_result(const std::string& name, size_t data_size, std::vector<double> times_ns) {
    std::sort(times_ns.begin(), times_ns.end());
    results.push_back({name, data_size, std::move(times_ns)});
  }

  void print_results() const {
    std::cout << "\nBenchmark Results:\n";
    std::cout << std::setw(20) << "Test" << std::setw(12) << "Size" << std::setw(14) << "Median (ms)" << std::setw(12)
              << "Min (ms)" << std::setw(12) << "Max (ms)" << std::setw(10) << "Spread%" << std::setw(20)
              << "Median msgs/s"
              << "\n";
    std::cout << std::string(100, '-') << "\n";

    for (const auto& r : results) {
      if (r.times_ns.empty()) continue;
      const double median_ns = r.times_ns[r.times_ns.size() / 2];
      const double min_ns = r.times_ns.front();
      const double max_ns = r.times_ns.back();
      const double spread_pct = 100.0 * (max_ns - min_ns) / median_ns;
      const double median_mps = (static_cast<double>(r.data_size) * 1e9) / median_ns;
      std::cout << std::setw(20) << r.name << std::setw(12) << r.data_size << std::setw(14) << std::fixed
                << std::setprecision(2) << median_ns / 1e6 << std::setw(12) << min_ns / 1e6 << std::setw(12)
                << max_ns / 1e6 << std::setw(9) << std::fixed << std::setprecision(2) << spread_pct << "%"
                << std::setw(20) << format_throughput(median_mps) << "\n";
    }
  }

 private:
  struct Result {
    std::string name;
    size_t data_size;
    std::vector<double> times_ns;  // sorted ascending on insert
  };
  std::vector<Result> results;
};

class SignalGenerator {
 public:
  static std::vector<double> generate_random_walk(size_t length, double start_price = 100.0, uint32_t seed = 42) {
    // Fixed seed by default: cross-run input data must be identical or
    // Bollinger variance between invocations will be input-driven, not timing-
    // driven. Callers can override if they need a different realization.
    std::mt19937 gen(seed);
    std::normal_distribution<> d(0, 1);

    std::vector<double> prices;
    prices.reserve(length);

    double current_price = start_price;
    prices.push_back(current_price);

    for (size_t i = 1; i < length; ++i) {
      current_price += d(gen);
      prices.push_back(current_price);
    }

    return prices;
  }
};

// PPG Pipeline Benchmark
class PPGPipelineBenchmark {
 public:
  PPGPipelineBenchmark(const std::vector<int>& times, const std::vector<double>& values) : values_(values) {
    if (times.empty() || values.empty() || times.size() != values.size()) {
      throw std::runtime_error("Invalid PPG dataset");
    }
    times_.reserve(times.size());
    for (const auto& t : times) {
      times_.push_back(static_cast<timestamp_t>(t));
    }
    dt_ = (times.back() - times.front()) / (times.size() - 1);
    short_window_ = static_cast<int>(std::round(50 / dt_));
    long_window_ = static_cast<int>(std::round(2000 / dt_));
  }

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto pipeline = create_pipeline();
    size_t data_index = 0;
    timestamp_t time_offset = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < data_size; ++i) {
      // When we reach the end of dataset, reset index and bump time offset.
      // NOTE: must be `+=` (accumulate) — prior `=` (reset-to-constant) caused
      // every pass after the second to collide with the previous pass's
      // timestamps, triggering Input's debug-mode ordering check. Each throw
      // is caught and swallowed (Input.h:53–55), and the message is dropped,
      // but exception dispatch costs ~3 µs/call. At 10M iterations that
      // equaled ~30 s of overhead and ~99% of the benchmark's wall time.
      if (data_index >= times_.size()) {
        data_index = 0;
        time_offset += times_[times_.size() - 1] - times_[0] + dt_;
      }

      timestamp_t current_time = times_[data_index] + time_offset;
      pipeline.input->receive_data(create_message<NumberData>(current_time, NumberData{values_[data_index]}), 0);
      pipeline.input->execute();
      pipeline.reset_sink();

      data_index++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  std::vector<timestamp_t> times_;
  const std::vector<double>& values_;
  double dt_;
  int short_window_;
  int long_window_;

  struct Pipeline {
    std::shared_ptr<Input> input;
    std::shared_ptr<MovingAverage> ma_short;
    std::shared_ptr<MovingAverage> ma_long;
    std::shared_ptr<Subtraction> minus;
    std::shared_ptr<PeakDetector> peak;
    std::shared_ptr<Join> join;
    std::shared_ptr<Output> output;
    std::shared_ptr<Collector> col;

    void reset_sink() { col->reset(); }
  };

  Pipeline create_pipeline() {
    Pipeline p;
    p.input = std::make_shared<Input>("i1", std::vector<std::string>{PortType::NUMBER});
    p.ma_short = std::make_shared<MovingAverage>("ma1", short_window_);
    p.ma_long = std::make_shared<MovingAverage>("ma2", long_window_);
    p.minus = std::make_shared<Subtraction>("diff");
    p.peak = std::make_shared<PeakDetector>("peak", 2 * short_window_ + 1);
    p.join = std::make_shared<Join>("join", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER});
    p.output = std::make_shared<Output>("o1", std::vector<std::string>{PortType::NUMBER});
    p.col = std::make_shared<Collector>("c", std::vector<std::string>{PortType::NUMBER});

    p.input->connect(p.ma_short)->connect(p.minus, 0, 0)->connect(p.peak)->connect(p.join, 0, 0)->connect(p.output);
    p.input->connect(p.ma_long)->connect(p.minus, 0, 1);
    p.input->connect(p.join, 0, 1);
    p.output->connect(p.col, 0, 0);

    return p;
  }
};

// PPG pipeline as a composition of templated building blocks from libs/compiled.
// NOT a library type — PPG is a specific realization. The composition lives in
// the benchmark because the benchmark is one consumer of the building blocks;
// the parity test in libs/compiled/test/test_ppg_e2e.cpp has its own copy of
// the same composition. The library only contains the per-operator stages.
template <std::size_t WShort, std::size_t WLong, std::size_t WPeak>
struct PPGCompiled {
  static_assert(WPeak >= 3 && (WPeak % 2 == 1), "WPeak must be odd >= 3");

  rtbot::compiled::MovingAverageStage<WShort> ma_short;
  rtbot::compiled::MovingAverageStage<WLong> ma_long;
  rtbot::compiled::JoinStage<2> join_ma;
  rtbot::compiled::SubtractionStage minus;
  rtbot::compiled::PeakDetectorStage<WPeak> peak;
  rtbot::compiled::JoinStage<2> join_out;

  template <class F>
  inline void process(std::int64_t t, double v, F&& emit) noexcept {
    push_join_out_port_1(t, v, emit);

    std::int64_t ms_t = 0; double ms_v = 0.0;
    if (ma_short.process(t, v, ms_t, ms_v)) {
      push_join_ma(0, ms_t, ms_v, emit);
    }
    std::int64_t ml_t = 0; double ml_v = 0.0;
    if (ma_long.process(t, v, ml_t, ml_v)) {
      push_join_ma(1, ml_t, ml_v, emit);
    }
  }

 private:
  template <class F>
  inline void push_join_out_port_1(std::int64_t t, double v, F& emit) noexcept {
    std::int64_t out_t = 0;
    std::array<double, 2> out_vs{};
    if (join_out.push(1, t, v, out_t, out_vs)) {
      emit(out_t, out_vs[0], out_vs[1]);
    }
  }

  template <class F>
  inline void push_join_out_port_0(std::int64_t t, double v, F& emit) noexcept {
    std::int64_t out_t = 0;
    std::array<double, 2> out_vs{};
    if (join_out.push(0, t, v, out_t, out_vs)) {
      emit(out_t, out_vs[0], out_vs[1]);
    }
  }

  template <class F>
  inline void push_join_ma(std::size_t port, std::int64_t t, double v, F& emit) noexcept {
    std::int64_t join_t = 0;
    std::array<double, 2> join_vs{};
    if (join_ma.push(port, t, v, join_t, join_vs)) {
      const double diff = minus.process(join_vs[0], join_vs[1]);
      std::int64_t pk_t = 0;
      double pk_v = 0.0;
      if (peak.process(join_t, diff, pk_t, pk_v)) {
        push_join_out_port_0(pk_t, pk_v, emit);
      }
    }
  }
};

class PPGCompiledBenchmark {
 public:
  PPGCompiledBenchmark(const std::vector<int>& times, const std::vector<double>& values)
      : values_(values) {
    if (times.empty() || values.empty() || times.size() != values.size()) {
      throw std::runtime_error("Invalid PPG dataset");
    }
    times_.reserve(times.size());
    for (const auto& t : times) {
      times_.push_back(static_cast<rtbot::timestamp_t>(t));
    }
    dt_ = (times.back() - times.front()) / (times.size() - 1);
    short_window_ = static_cast<std::size_t>(std::round(50 / dt_));
    long_window_ = static_cast<std::size_t>(std::round(2000 / dt_));
  }

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    rtbot::compiled::CompiledProgram<PPGCompiled<50, 2000, 2 * 50 + 1>, 2> prog;

    size_t data_index = 0;
    rtbot::timestamp_t time_offset = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < data_size; ++i) {
      if (data_index >= times_.size()) {
        data_index = 0;
        time_offset += times_[times_.size() - 1] - times_[0] + dt_;
      }
      const rtbot::timestamp_t current_time = times_[data_index] + time_offset;
      prog.receive(current_time, values_[data_index]);
      ++data_index;
    }
    auto end = std::chrono::high_resolution_clock::now();
    volatile std::size_t sink = prog.collect_outputs().size();
    (void)sink;

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  std::vector<rtbot::timestamp_t> times_;
  const std::vector<double>& values_;
  double dt_;
  std::size_t short_window_;
  std::size_t long_window_;
};

// JIT-compiled PPG via JitCompiler::compile(ppg_json). The compile happens
// once at construction (excluded from per-run timing); each benchmark run
// drives 5M samples through the JIT'd function pointer.
class PPGJitCompiledBenchmark {
 public:
  PPGJitCompiledBenchmark(const std::vector<int>& times, const std::vector<double>& values,
                           std::string program_json)
      : values_(values), program_json_(std::move(program_json)) {
    if (times.empty() || values.empty() || times.size() != values.size()) {
      throw std::runtime_error("Invalid PPG dataset");
    }
    times_.reserve(times.size());
    for (const auto& t : times) {
      times_.push_back(static_cast<rtbot::timestamp_t>(t));
    }
    dt_ = (times.back() - times.front()) / (times.size() - 1);
  }

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    rtbot::jit::JitCompiler compiler;
    auto prog = compiler.compile(program_json_);  // pre-compile, exclude from timing

    size_t data_index = 0;
    rtbot::timestamp_t time_offset = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < data_size; ++i) {
      if (data_index >= times_.size()) {
        data_index = 0;
        time_offset += times_[times_.size() - 1] - times_[0] + dt_;
      }
      const rtbot::timestamp_t current_time = times_[data_index] + time_offset;
      prog->receive(current_time, values_[data_index]);
      ++data_index;
    }
    auto end = std::chrono::high_resolution_clock::now();
    volatile std::size_t sink = prog->collect_outputs().size();
    (void)sink;

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  std::vector<rtbot::timestamp_t> times_;
  const std::vector<double>& values_;
  double dt_;
  std::string program_json_;
};

// Bollinger Bands Program Benchmark
class BollingerBandsBenchmark {
 public:
  BollingerBandsBenchmark(const std::string& program_json) : program_json_(program_json) {}

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    rtbot::Program program(program_json_);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < data_size; ++i) {
      program.receive(Message<NumberData>(i, NumberData{prices[i]}));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double total_time_ns = duration.count();
    double messages_per_second = (data_size * 1e9) / total_time_ns;

    return {total_time_ns, messages_per_second};
  }

 private:
  std::string program_json_;
};

// Add to the existing benchmark.cpp file

class BollingerBandsPureBenchmark {
 public:
  struct Pipeline {
    std::shared_ptr<Input> input;
    std::shared_ptr<ResamplerHermite> resampler;
    std::shared_ptr<MovingAverage> ma;
    std::shared_ptr<StandardDeviation> sd;
    std::shared_ptr<Scale> scale;
    std::shared_ptr<Addition> upper;
    std::shared_ptr<Subtraction> lower;
    std::shared_ptr<Output> output;
    std::shared_ptr<Collector> col;

    void reset_sink() { col->reset(); }
  };

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    auto pipeline = create_pipeline();

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < data_size; ++i) {
      pipeline.input->receive_data(create_message<NumberData>(i, NumberData{prices[i]}), 0);
      pipeline.input->execute();
      pipeline.reset_sink();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  Pipeline create_pipeline() {
    Pipeline p;

    // Create operators
    p.input = std::make_shared<Input>("input", std::vector<std::string>{PortType::NUMBER});
    p.resampler = std::make_shared<ResamplerHermite>("resamp", 1);
    p.ma = std::make_shared<MovingAverage>("ma", 14);
    p.sd = std::make_shared<StandardDeviation>("sd", 14);
    p.scale = std::make_shared<Scale>("scale", 2.0);
    p.upper = std::make_shared<Addition>("upper", 2);
    p.lower = std::make_shared<Subtraction>("lower", 2);
    p.output = std::make_shared<Output>("output",
                                        std::vector<std::string>{PortType::NUMBER, PortType::NUMBER, PortType::NUMBER});
    p.col = std::make_shared<Collector>("c", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER, PortType::NUMBER});
    p.output->connect(p.col, 0, 0);
    p.output->connect(p.col, 1, 1);
    p.output->connect(p.col, 2, 2);

    // Connect pipeline:
    // input -> resampler -> ma -> output(middle band)
    //                   \-> sd -> scale ---> upper(add) -> output(upper band)
    //                                    \-> lower(sub) -> output(lower band)
    //                            ma --------> upper
    //                            ma --------> lower
    p.input->connect(p.resampler);
    p.resampler->connect(p.ma);
    p.resampler->connect(p.sd);
    p.sd->connect(p.scale);
    p.ma->connect(p.upper);
    p.ma->connect(p.lower);
    p.scale->connect(p.upper, 0, 1);
    p.scale->connect(p.lower, 0, 1);
    p.upper->connect(p.output, 0, 1);
    p.lower->connect(p.output, 0, 0);
    p.ma->connect(p.output, 0, 2);

    return p;
  }
};

// Templated Bollinger stage — the architectural ceiling estimate, with NO
// Operator framework on the hot path. No port queues, no Message<T>, no
// virtual dispatch, no emit_output, no Collector receive. Just a struct with
// inline state and a process() method composed at compile time.
//
// The benchmark drives this directly in a tight loop, writing outputs into a
// caller-provided sink. That eliminates from the measurement everything that
// BollingerSpecializedOp inherits from Operator: the ~50 ns/event of around-
// FE framework cost we identified in phase-0 analysis.
//
// Numerical contract: identical algorithm to BollingerSpecializedOp (same
// Kahan-compensated running sum, same O(W) M2 recompute). Only the framework
// around the math differs.
template <std::size_t W, int K_int>
struct BollingerTemplatedStage {
  static constexpr double K = static_cast<double>(K_int);

  std::array<double, W> ring{};
  double sum{0.0};
  double comp{0.0};
  std::size_t count{0};

  // Returns true and writes lower/upper/middle if the window is full.
  // Returns false (warmup) for the first W-1 messages.
  inline bool process(double v, double& lower, double& upper, double& middle) {
    const std::size_t idx = count % W;
    if (count >= W) {
      const double leaving = ring[idx];
      const double ys = (-leaving) - comp;
      const double ts = sum + ys;
      comp = (ts - sum) - ys;
      sum = ts;
    }
    ring[idx] = v;
    const double ya = v - comp;
    const double ta = sum + ya;
    comp = (ta - sum) - ya;
    sum = ta;
    ++count;

    if (count < W) return false;

    const double mean = sum / static_cast<double>(W);
    double m2 = 0.0;
    for (std::size_t k = 0; k < W; ++k) {
      std::size_t ring_idx;
      if (count == W) {
        ring_idx = k;
      } else {
        ring_idx = (idx + 1 + k) % W;
      }
      const double d = ring[ring_idx] - mean;
      m2 += d * d;
    }
    const double sd = std::sqrt(m2 / static_cast<double>(W - 1));

    lower = mean - K * sd;
    upper = mean + K * sd;
    middle = mean;
    return true;
  }
};

// Hand-codegen Bollinger operator: what an LLVM-JIT'd FusedExpression would
// emit for the Bollinger bytecode after constant folding and cross-output CSE.
// The point is NOT to ship a Bollinger-specific operator — it's to measure the
// throughput ceiling a JIT could deliver for an arbitrary FE program of this
// shape (single windowed input, multiple outputs sharing one MA + STD), so we
// can decide whether building a JIT for the FE is worth the engineering lift.
//
// Differences from the naive Bollinger Fused bytecode that a JIT would produce
// automatically through CSE and dead-store elimination:
//   - One MA state instead of three (the bytecode declares MA_UPDATE 3 times)
//   - One STD state instead of two
//   - W=14 and k=2.0 are compile-time constants instead of state/aux reads
//   - Stack machine collapsed into registers
//   - No dispatch loop; each opcode body inlined into a straight-line function
// Numerical contract: same Kahan-compensated running sum + O(W) M2 recompute
// as the FE's STD_UPDATE, so output should match Bollinger Fused bit-for-bit.
class BollingerSpecializedOp : public Operator {
 public:
  static constexpr std::size_t W = 14;
  static constexpr double K = 2.0;

  explicit BollingerSpecializedOp(std::string id)
      : Operator(std::move(id)) {
    add_data_port<NumberData>();
    add_output_port<VectorNumberData>();
  }

  std::string type_name() const override { return "BollingerSpecialized"; }

 protected:
  void process_data(bool debug = false) override {
    auto& q = get_data_queue(0);
    while (!q.empty()) {
      const auto* msg = static_cast<const Message<NumberData>*>(q.front().get());
      const double v = msg->data.value;
      const timestamp_t t = msg->time;

      const std::size_t idx = count_ % W;
      if (count_ >= W) {
        const double leaving = ring_[idx];
        const double ys = (-leaving) - comp_;
        const double ts = sum_ + ys;
        comp_ = (ts - sum_) - ys;
        sum_ = ts;
      }
      ring_[idx] = v;
      const double ya = v - comp_;
      const double ta = sum_ + ya;
      comp_ = (ta - sum_) - ya;
      sum_ = ta;
      ++count_;

      if (count_ < W) {
        q.pop_front();
        continue;
      }

      const double mean = sum_ / static_cast<double>(W);
      double m2 = 0.0;
      for (std::size_t k = 0; k < W; ++k) {
        std::size_t ring_idx;
        if (count_ == W) {
          ring_idx = k;
        } else {
          ring_idx = (idx + 1 + k) % W;
        }
        const double d = ring_[ring_idx] - mean;
        m2 += d * d;
      }
      const double sd = std::sqrt(m2 / static_cast<double>(W - 1));

      auto out_vec = make_pooled_vector_double(3);
      (*out_vec)[0] = mean - K * sd;  // lower
      (*out_vec)[1] = mean + K * sd;  // upper
      (*out_vec)[2] = mean;            // middle

      emit_output(
          0,
          create_message<VectorNumberData>(t, VectorNumberData(std::move(out_vec))),
          debug);
      q.pop_front();
    }
  }

 private:
  std::array<double, W> ring_{};
  double sum_{0.0};
  double comp_{0.0};
  std::size_t count_{0};
};

// Bollinger Bands as a single FusedExpression operator. Same input prefix as
// BollingerBandsPureBenchmark (Input -> ResamplerHermite) so the comparison
// isolates "scattered ops graph" vs "single fused operator" for the math part.
//
// Bytecode produces three outputs as a VectorNumberData per tick:
//   out[0] = lower  = MA(14) - 2*STD(14)
//   out[1] = upper  = MA(14) + 2*STD(14)
//   out[2] = middle = MA(14)
//
// This is the naive 1:1 translation an SQL-to-fuse compiler would emit before
// any cross-output deduplication: MA_UPDATE appears 3x and STD_UPDATE 2x, each
// occurrence with its own state slots. A future variant can use STATE_LOAD to
// share the single MA/STD result across the three output expressions.
class BollingerBandsFusedBenchmark {
 public:
  struct Pipeline {
    std::shared_ptr<Input> input;
    std::shared_ptr<ResamplerHermite> resampler;
    std::shared_ptr<FusedExpression> fe;
    std::shared_ptr<Collector> col;

    void reset_sink() { col->reset(); }
  };

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    auto pipeline = create_pipeline();

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < data_size; ++i) {
      pipeline.input->receive_data(create_message<NumberData>(i, NumberData{prices[i]}), 0);
      pipeline.input->execute();
      pipeline.reset_sink();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  Pipeline create_pipeline() {
    using namespace rtbot::fused_op;

    Pipeline p;
    p.input = std::make_shared<Input>("input", std::vector<std::string>{PortType::NUMBER});
    p.resampler = std::make_shared<ResamplerHermite>("resamp", 1);

    std::vector<double> bytecode = {
      // out[0] = MA(14) - 2*STD(14)
      INPUT, 0, MA_UPDATE, 14, INPUT, 0, STD_UPDATE, 14, CONST, 0, MUL, SUB, END,
      // out[1] = MA(14) + 2*STD(14)
      INPUT, 0, MA_UPDATE, 14, INPUT, 0, STD_UPDATE, 14, CONST, 0, MUL, ADD, END,
      // out[2] = MA(14)
      INPUT, 0, MA_UPDATE, 14, END,
    };
    std::vector<double> constants = {2.0};
    p.fe = make_fused_expression("bb_fe", 1, 3, bytecode, constants);
    p.col = make_vector_number_collector("bb_col");

    p.input->connect(p.resampler);
    p.resampler->connect(p.fe);
    p.fe->connect(p.col, 0, 0);

    return p;
  }
};

// Same input prefix as BollingerBandsPureBenchmark and BollingerBandsFusedBenchmark,
// but the math is done by BollingerSpecializedOp — the hand-codegen estimate
// of what a JIT'd FE would produce after CSE.
class BollingerBandsSpecializedBenchmark {
 public:
  struct Pipeline {
    std::shared_ptr<Input> input;
    std::shared_ptr<ResamplerHermite> resampler;
    std::shared_ptr<BollingerSpecializedOp> bb;
    std::shared_ptr<Collector> col;

    void reset_sink() { col->reset(); }
  };

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    auto pipeline = create_pipeline();

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < data_size; ++i) {
      pipeline.input->receive_data(create_message<NumberData>(i, NumberData{prices[i]}), 0);
      pipeline.input->execute();
      pipeline.reset_sink();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  Pipeline create_pipeline() {
    Pipeline p;
    p.input = std::make_shared<Input>("input", std::vector<std::string>{PortType::NUMBER});
    p.resampler = std::make_shared<ResamplerHermite>("resamp", 1);
    p.bb = std::make_shared<BollingerSpecializedOp>("bb_spec");
    p.col = make_vector_number_collector("bb_col");

    p.input->connect(p.resampler);
    p.resampler->connect(p.bb);
    p.bb->connect(p.col, 0, 0);

    return p;
  }
};

// Bollinger Bands as a composition of templated building blocks from
// libs/compiled. NOT a library type — Bollinger is a specific realization.
// The composition lives here because the benchmark is one consumer of the
// building blocks; the parity test in libs/compiled/test/test_bollinger_e2e.cpp
// has its own copy of the same composition. The library only contains the
// per-operator stages.
template <std::int64_t Interval, std::size_t W, int K_int>
struct BollingerCompiled {
  rtbot::compiled::ResamplerHermiteStage resampler{Interval};
  rtbot::compiled::MovingAverageStage<W> ma;
  rtbot::compiled::StdDevStage<W> sd;
  rtbot::compiled::ScaleStage scale{static_cast<double>(K_int)};
  rtbot::compiled::AdditionStage add;
  rtbot::compiled::SubtractionStage sub;

  template <class F>
  inline void process(std::int64_t t, double v, F&& emit) noexcept {
    resampler.process(t, v, [&](std::int64_t rt, double rv) {
      std::int64_t ma_t = 0, sd_t = 0;
      double ma_v = 0.0, sd_v = 0.0;
      const bool ma_ok = ma.process(rt, rv, ma_t, ma_v);
      const bool sd_ok = sd.process(rt, rv, sd_t, sd_v);
      if (!(ma_ok && sd_ok)) return;
      const double scaled = scale.process(sd_v);
      const double lower = sub.process(ma_v, scaled);
      const double upper = add.process(ma_v, scaled);
      emit(rt, lower, upper, ma_v);
    });
  }
};

// Drives the locally-defined BollingerCompiled composition in a tight loop
// via CompiledProgram.
class BollingerBandsComposedBenchmark {
 public:
  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    rtbot::compiled::CompiledProgram<BollingerCompiled<1, 14, 2>, 3> prog;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < data_size; ++i) {
      prog.receive(static_cast<std::int64_t>(i), prices[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    // Drain to ensure the optimizer can't elide the work.
    volatile std::size_t sink = prog.collect_outputs().size();
    (void)sink;

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return {duration.count(), (data_size * 1e9) / duration.count()};
  }
};

// JIT-compiled Bollinger via JitCompiler::compile(bollinger_json). The
// compile happens once at construction (excluded from per-run timing); each
// benchmark run drives 5M samples through the JIT'd function pointer.
class BollingerJitCompiledBenchmark {
 public:
  explicit BollingerJitCompiledBenchmark(const std::string& program_json)
      : program_json_(program_json) {}

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);

    rtbot::jit::JitCompiler compiler;
    auto prog = compiler.compile(program_json_);  // pre-compile, exclude from timing

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < data_size; ++i) {
      prog->receive(static_cast<std::int64_t>(i + 1), prices[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    volatile std::size_t sink = prog->collect_outputs().size();
    (void)sink;

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  std::string program_json_;
};

// JIT-compiled Bollinger via JitCompiler::compile, but calling the JIT'd function
// pointer directly (bypassing JitCompiledProgram::receive wrapper) to isolate
// wrapper overhead from JIT code quality.
class BollingerJitDirectBenchmark {
 public:
  explicit BollingerJitDirectBenchmark(const std::string& program_json) {
    rtbot::jit::JitCompiler compiler;
    prog_ = compiler.compile(program_json);
    state_.resize(prog_->state_size(), 0.0);
    fn_ = prog_->raw_fn();
    out_v_.resize(prog_->num_outputs(), 0.0);
  }

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    std::fill(state_.begin(), state_.end(), 0.0);

    int64_t out_t = 0;
    int32_t out_pid = 0;
    size_t emitted = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < data_size; ++i) {
      int32_t cnt = fn_(state_.data(), static_cast<int64_t>(i + 1), prices[i],
                        &out_t, out_v_.data(), &out_pid);
      if (cnt > 0) ++emitted;
    }
    auto end = std::chrono::high_resolution_clock::now();
    volatile size_t sink = emitted;
    (void)sink;

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  std::unique_ptr<rtbot::jit::JitCompiledProgram> prog_;
  rtbot::jit::JitCompiledProgram::SegmentFnT fn_{nullptr};
  std::vector<double> state_;
  std::vector<double> out_v_;
};

// Bollinger Bands via Program::send + drain_outputs (streaming API).
// Sends all N samples first, then drains once. This amortizes the per-call
// batch translation over the entire run — for JIT-active programs, each
// send() is just one JitCompiledProgram::receive() call with no heap work.
// Expected throughput: close to direct JIT (~67M msgs/s) vs ~4M for receive().
class BollingerProgramStreamBenchmark {
 public:
  explicit BollingerProgramStreamBenchmark(const std::string& program_json)
      : program_json_(program_json) {}

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    rtbot::Program prog(program_json_);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < data_size; ++i) {
      prog.send(static_cast<std::int64_t>(i + 1), prices[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    // Drain once; volatile prevents the optimizer from eliding the work.
    volatile std::size_t sink = prog.drain_outputs().size();
    (void)sink;

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  std::string program_json_;
};

// Bollinger Bands via Program::send + drain_into(callback).
// Identical send loop to BollingerProgramStreamBenchmark, but the drain
// bypasses ProgramMsgBatch construction entirely: the callback is invoked once
// per emission directly on the JIT's flat emit buffer. Expected throughput:
// close to direct JIT (65-72M msgs/s).
class BollingerProgramJitDrainIntoBenchmark {
 public:
  explicit BollingerProgramJitDrainIntoBenchmark(const std::string& program_json)
      : program_json_(program_json) {}

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    rtbot::Program prog(program_json_);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < data_size; ++i) {
      prog.send(static_cast<std::int64_t>(i + 1), prices[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();

    // Drain via callback; volatile prevents dead-store elimination.
    std::size_t emit_count = 0;
    prog.drain_into([&emit_count](std::int64_t, const double*, std::size_t) {
      ++emit_count;
    });
    volatile std::size_t sink = emit_count;
    (void)sink;

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  std::string program_json_;
};

// Bollinger via Program's public construction path but using the escape-hatch
// accessor to call JitCompiledProgram::receive directly on the hot path.
// Expected throughput: matches Bollinger JIT (~65M msgs/s), proving the
// accessor reaches the same speed as direct JitCompiler::compile usage.
class BollingerProgramJitEscapeHatchBenchmark {
 public:
  explicit BollingerProgramJitEscapeHatchBenchmark(const std::string& json) : json_(json) {}

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    rtbot::Program prog(json_);
    auto* jit = prog.jit_program();
    if (!jit) throw std::runtime_error("JIT not available");

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < data_size; ++i) {
      jit->receive(static_cast<int64_t>(i + 1), prices[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    volatile std::size_t sink = jit->collect_outputs().size();
    (void)sink;

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  std::string json_;
};

// JIT-compiled Bollinger via the Phase-5 spike's hand-coded emit_bollinger_process.
// Calls the JIT'd function pointer directly (same calling convention as JitCompiledProgram)
// to isolate JIT code quality from wrapper overhead.
class BollingerJitSpikeBenchmark {
 public:
  using SpikeFnT = bool (*)(double* state, int64_t t, double v,
                             int64_t* out_t,
                             double* out_lower, double* out_upper, double* out_middle);

  BollingerJitSpikeBenchmark() {
    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto mod = std::make_unique<llvm::Module>("bollinger_spike", *ctx);
    rtbot::emit_bollinger_process(*mod, *ctx);
    jit_ctx_ = std::make_unique<rtbot::JitContext>();
    jit_ctx_->compile_module(std::move(mod), std::move(ctx));
    fn_ = jit_ctx_->lookup<SpikeFnT>("bollinger_process");
    state_.resize(rtbot::kBollingerStateSize, 0.0);
  }

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    std::fill(state_.begin(), state_.end(), 0.0);

    int64_t out_t = 0;
    double out_lower = 0.0, out_upper = 0.0, out_middle = 0.0;
    size_t emitted = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < data_size; ++i) {
      if (fn_(state_.data(), static_cast<int64_t>(i + 1), prices[i],
              &out_t, &out_lower, &out_upper, &out_middle)) {
        ++emitted;
      }
    }
    auto end = std::chrono::high_resolution_clock::now();
    volatile size_t sink = emitted;
    (void)sink;

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return {duration.count(), (data_size * 1e9) / duration.count()};
  }

 private:
  std::unique_ptr<rtbot::JitContext> jit_ctx_;
  SpikeFnT fn_{nullptr};
  std::vector<double> state_;
};

// Drives BollingerTemplatedStage directly in a tight loop, with no Operator
// framework. Caveat for cross-comparison: this skips the ResamplerHermite
// stage that the other Bollinger variants include. The random-walk input
// already has integer-spaced timestamps and the resampler runs at interval=1,
// so in steady state the resampler is approximately identity for this input
// — but the variant is included as a "no-framework, no-resampler" floor, not
// as drop-in apples-to-apples replacement for Bollinger Spec.
class BollingerBandsTemplatedBenchmark {
 public:
  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    BollingerTemplatedStage<14, 2> stage;

    // Sink: accumulate a checksum of every emitted output so the compiler
    // can't eliminate the math as dead code. The checksum value itself
    // doesn't matter — what matters is that every output is read.
    double checksum = 0.0;
    std::size_t emitted = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < data_size; ++i) {
      double lower = 0.0, upper = 0.0, middle = 0.0;
      if (stage.process(prices[i], lower, upper, middle)) {
        checksum += lower + upper + middle;
        ++emitted;
      }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Volatile sink so the optimizer can't observe the checksum is unused.
    volatile double sink = checksum;
    (void)sink;
    (void)emitted;

    return {duration.count(), (data_size * 1e9) / duration.count()};
  }
};

std::string make_ppg_json(std::size_t WShort, std::size_t WLong, std::size_t WPeak) {
  std::ostringstream o;
  o << R"({
  "title": "PPG Peak Detection",
  "apiVersion": "v1",
  "entryOperator": "input",
  "output": { "out": ["o1", "o2"] },
  "operators": [
    { "id": "input",    "type": "Input",         "portTypes": ["number"] },
    { "id": "ma_short", "type": "MovingAverage", "window_size": )" << WShort << R"( },
    { "id": "ma_long",  "type": "MovingAverage", "window_size": )" << WLong << R"( },
    { "id": "join_ma",  "type": "Join",          "portTypes": ["number", "number"] },
    { "id": "minus",    "type": "Subtraction" },
    { "id": "peak",     "type": "PeakDetector",  "window_size": )" << WPeak << R"( },
    { "id": "join_out", "type": "Join",          "portTypes": ["number", "number"] },
    { "id": "out",      "type": "Output",        "portTypes": ["number", "number"] }
  ],
  "connections": [
    { "from": "input",    "to": "ma_short",  "fromPort": "o1", "toPort": "i1" },
    { "from": "input",    "to": "ma_long",   "fromPort": "o1", "toPort": "i1" },
    { "from": "ma_short", "to": "join_ma",   "fromPort": "o1", "toPort": "i1" },
    { "from": "ma_long",  "to": "join_ma",   "fromPort": "o1", "toPort": "i2" },
    { "from": "join_ma",  "to": "minus",     "fromPort": "o1", "toPort": "i1" },
    { "from": "join_ma",  "to": "minus",     "fromPort": "o2", "toPort": "i2" },
    { "from": "minus",    "to": "peak",      "fromPort": "o1", "toPort": "i1" },
    { "from": "peak",     "to": "join_out",  "fromPort": "o1", "toPort": "i1" },
    { "from": "input",    "to": "join_out",  "fromPort": "o1", "toPort": "i2" },
    { "from": "join_out", "to": "out",       "fromPort": "o1", "toPort": "i1" },
    { "from": "join_out", "to": "out",       "fromPort": "o2", "toPort": "i2" }
  ]
})";
  return o.str();
}

int main() {
  // Bollinger Bands program JSON
  std::string bollinger_json = R"({
    "title": "Bollinger Bands",
    "description": "This program is generated by the compiler, it represents the bollinger bands operator",
    "apiVersion": "v1",
    "author": "Eduardo",
    "entryOperator": "754",
    "output": { "37": ["o2", "o1", "o3"] },
    "operators": [
        { "id": "37", "type": "Output", "portTypes": ["number", "number", "number"] },
        { "id": "495", "type": "Subtraction" },
        { "id": "861", "type": "Addition" },
        { "id": "996", "type": "Scale", "value": 2 },
        { "id": "865", "type": "StandardDeviation", "window_size": 14 },
        { "id": "510", "type": "MovingAverage", "window_size": 14 },
        { "id": "262", "type": "ResamplerHermite", "interval": 1 },
        { "id": "754", "type": "Input", "portTypes": ["number"] }
    ],
    "connections": [
        { "from": "510", "to": "37", "fromPort": "o1", "toPort": "i3" },
        { "from": "495", "to": "37", "fromPort": "o1", "toPort": "i1" },
        { "from": "861", "to": "37", "fromPort": "o1", "toPort": "i2" },
        { "from": "996", "to": "495", "fromPort": "o1", "toPort": "i2" },
        { "from": "510", "to": "495", "fromPort": "o1", "toPort": "i1" },
        { "from": "996", "to": "861", "fromPort": "o1", "toPort": "i2" },
        { "from": "510", "to": "861", "fromPort": "o1", "toPort": "i1" },
        { "from": "865", "to": "996", "fromPort": "o1", "toPort": "i1" },
        { "from": "262", "to": "865", "fromPort": "o1", "toPort": "i1" },
        { "from": "262", "to": "510", "fromPort": "o1", "toPort": "i1" },
        { "from": "754", "to": "262", "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  // Load PPG data
  auto s = SamplePPG("examples/data/ppg.csv");

  std::cout << "Starting RTBot Combined Benchmark\n";
  std::cout << "================================\n";

  // Compute PPG window sizes from actual dataset dt — same formula as
  // PPGPipelineBenchmark and PPGCompiledBenchmark, so all three paths
  // exercise the same workload.
  double dt = (s.ti.back() - s.ti.front()) / static_cast<double>(s.ti.size() - 1);
  std::size_t ppg_w_short = static_cast<std::size_t>(std::round(50.0 / dt));
  std::size_t ppg_w_long  = static_cast<std::size_t>(std::round(2000.0 / dt));
  std::size_t ppg_w_peak  = 2 * ppg_w_short + 1;
  std::string ppg_json = make_ppg_json(ppg_w_short, ppg_w_long, ppg_w_peak);

  PPGPipelineBenchmark ppg_benchmark(s.ti, s.ppg);
  PPGCompiledBenchmark ppg_compiled_benchmark(s.ti, s.ppg);
  PPGJitCompiledBenchmark ppg_jit_benchmark(s.ti, s.ppg, std::move(ppg_json));
  BollingerBandsBenchmark bollinger_benchmark(bollinger_json);
  BollingerProgramStreamBenchmark bollinger_stream_benchmark(bollinger_json);
  BollingerProgramJitDrainIntoBenchmark bollinger_stream_cb_benchmark(bollinger_json);
  BollingerJitCompiledBenchmark bollinger_jit_benchmark(bollinger_json);
  BollingerJitDirectBenchmark bollinger_jit_direct_benchmark(bollinger_json);
  BollingerProgramJitEscapeHatchBenchmark bollinger_escape_benchmark(bollinger_json);

  // Define test sizes. When RTBOT_PERF=1, skip the full sweep — we only
  // need the 10M measurement run below.
#ifdef RTBOT_PERF
  std::vector<size_t> test_sizes = {};
#else
  std::vector<size_t> test_sizes = {10000000};
#endif

  BenchmarkResults results2;
  constexpr size_t kSize = 5000000;
  constexpr size_t kRuns = 5;

#ifndef RTBOT_PERF
  // Warmup pass — one run per workload, discarded. Primes CPU boost clocks,
  // allocator arenas, and page cache so the first measured run isn't biased.
  std::cout << "Warmup pass...\n";
  bollinger_benchmark.benchmark_single_size(kSize);
  bollinger_stream_benchmark.benchmark_single_size(kSize);
  bollinger_stream_cb_benchmark.benchmark_single_size(kSize);
  bollinger_jit_benchmark.benchmark_single_size(kSize);
  {
    BollingerBandsPureBenchmark pure;
    pure.benchmark_single_size(kSize);
  }
  ppg_benchmark.benchmark_single_size(kSize);
  ppg_compiled_benchmark.benchmark_single_size(kSize);
  ppg_jit_benchmark.benchmark_single_size(kSize);

  std::vector<double> bollinger_times, bollinger_stream_times, bollinger_stream_cb_times, pure_times, ppg_times;
  bollinger_times.reserve(kRuns);
  bollinger_stream_times.reserve(kRuns);
  bollinger_stream_cb_times.reserve(kRuns);
  pure_times.reserve(kRuns);
  ppg_times.reserve(kRuns);
  std::vector<double> ppg_compiled_times;
  ppg_compiled_times.reserve(kRuns);
  std::vector<double> ppg_jit_times;
  ppg_jit_times.reserve(kRuns);

  std::vector<double> fused_times;
  std::vector<double> spec_times;
  std::vector<double> templ_times;
  std::vector<double> comp_times;
  std::vector<double> bollinger_jit_times;
  std::vector<double> bollinger_jit_spike_times;
  std::vector<double> bollinger_jit_direct_times;
  std::vector<double> bollinger_escape_times;
  fused_times.reserve(kRuns);
  spec_times.reserve(kRuns);
  templ_times.reserve(kRuns);
  comp_times.reserve(kRuns);
  bollinger_jit_times.reserve(kRuns);
  bollinger_jit_spike_times.reserve(kRuns);
  bollinger_jit_direct_times.reserve(kRuns);
  bollinger_escape_times.reserve(kRuns);
  // Warm up all variants so first measured run isn't an outlier.
  {
    BollingerBandsFusedBenchmark fused;
    fused.benchmark_single_size(kSize);
  }
  {
    BollingerBandsSpecializedBenchmark spec;
    spec.benchmark_single_size(kSize);
  }
  {
    BollingerBandsTemplatedBenchmark templ;
    templ.benchmark_single_size(kSize);
  }
  {
    BollingerBandsComposedBenchmark comp;
    comp.benchmark_single_size(kSize);
  }
  BollingerJitSpikeBenchmark jit_spike_bench;
  jit_spike_bench.benchmark_single_size(kSize);
  bollinger_jit_direct_benchmark.benchmark_single_size(kSize);
  bollinger_escape_benchmark.benchmark_single_size(kSize);

  // Interleave workloads across iterations: any slow drift in machine state
  // (thermal, scheduler, background load) hits all three equally per cycle
  // rather than biasing whichever workload runs last.
  std::cout << "Measuring (" << kRuns << " runs, interleaved)...\n";
  for (size_t i = 0; i < kRuns; ++i) {
    std::cout << "  iter " << (i + 1) << "/" << kRuns << "\n";
    bollinger_times.push_back(bollinger_benchmark.benchmark_single_size(kSize).first);
    bollinger_stream_times.push_back(bollinger_stream_benchmark.benchmark_single_size(kSize).first);
    bollinger_stream_cb_times.push_back(bollinger_stream_cb_benchmark.benchmark_single_size(kSize).first);
    bollinger_jit_times.push_back(bollinger_jit_benchmark.benchmark_single_size(kSize).first);
    bollinger_jit_direct_times.push_back(bollinger_jit_direct_benchmark.benchmark_single_size(kSize).first);
    bollinger_escape_times.push_back(bollinger_escape_benchmark.benchmark_single_size(kSize).first);
    bollinger_jit_spike_times.push_back(jit_spike_bench.benchmark_single_size(kSize).first);
    {
      BollingerBandsPureBenchmark pure;
      pure_times.push_back(pure.benchmark_single_size(kSize).first);
    }
    {
      BollingerBandsFusedBenchmark fused;
      fused_times.push_back(fused.benchmark_single_size(kSize).first);
    }
    {
      BollingerBandsSpecializedBenchmark spec;
      spec_times.push_back(spec.benchmark_single_size(kSize).first);
    }
    {
      BollingerBandsTemplatedBenchmark templ;
      templ_times.push_back(templ.benchmark_single_size(kSize).first);
    }
    {
      BollingerBandsComposedBenchmark comp;
      comp_times.push_back(comp.benchmark_single_size(kSize).first);
    }
    ppg_times.push_back(ppg_benchmark.benchmark_single_size(kSize).first);
    ppg_compiled_times.push_back(ppg_compiled_benchmark.benchmark_single_size(kSize).first);
    ppg_jit_times.push_back(ppg_jit_benchmark.benchmark_single_size(kSize).first);
  }

  results2.add_result("Bollinger Program", kSize, std::move(bollinger_times));
  results2.add_result("Bollinger Stream", kSize, std::move(bollinger_stream_times));
  results2.add_result("Bollinger Stream Cb", kSize, std::move(bollinger_stream_cb_times));
  results2.add_result("Bollinger Pure", kSize, std::move(pure_times));
  results2.add_result("Bollinger Fused", kSize, std::move(fused_times));
  results2.add_result("Bollinger Spec", kSize, std::move(spec_times));
  results2.add_result("Bollinger Templ", kSize, std::move(templ_times));
  results2.add_result("Bollinger Comp", kSize, std::move(comp_times));
  results2.add_result("Bollinger JIT", kSize, std::move(bollinger_jit_times));
  results2.add_result("Bollinger JIT Direct", kSize, std::move(bollinger_jit_direct_times));
  results2.add_result("Bollinger Escape", kSize, std::move(bollinger_escape_times));
  results2.add_result("Bollinger JIT Spike", kSize, std::move(bollinger_jit_spike_times));
  results2.add_result("PPG Pipeline", kSize, std::move(ppg_times));
  results2.add_result("PPG Compiled", kSize, std::move(ppg_compiled_times));
  results2.add_result("PPG JIT", kSize, std::move(ppg_jit_times));
  results2.print_results();
#endif

#ifdef RTBOT_PERF
  // Per-workload per-phase breakdown at a single stable size.
  constexpr size_t kPerfSize = 10000000;

  std::cout << "\n=== PerfCounters @ " << kPerfSize << " ticks ===\n";

  std::cout << "\n--- Bollinger Bands (Program) ---\n";
  PerfCounters::reset();
  bollinger_benchmark.benchmark_single_size(kPerfSize);
  PerfCounters::dump(std::cout);

  std::cout << "\n--- Bollinger Pure ---\n";
  {
    BollingerBandsPureBenchmark pure;
    PerfCounters::reset();
    pure.benchmark_single_size(kPerfSize);
    PerfCounters::dump(std::cout);
  }

  std::cout << "\n--- PPG Pipeline ---\n";
  PerfCounters::reset();
  ppg_benchmark.benchmark_single_size(kPerfSize);
  PerfCounters::dump(std::cout);
#endif

  return 0;
}