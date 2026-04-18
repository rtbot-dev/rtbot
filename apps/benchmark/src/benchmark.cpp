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

  // Create benchmark instances
  double dt = 0.001;  // 1ms
  int short_window = static_cast<int>(std::round(50 / dt));
  int long_window = static_cast<int>(std::round(2000 / dt));

  PPGPipelineBenchmark ppg_benchmark(s.ti, s.ppg);
  BollingerBandsBenchmark bollinger_benchmark(bollinger_json);

  // Define test sizes. When RTBOT_PERF=1, skip the full sweep — we only
  // need the 10M measurement run below.
#ifdef RTBOT_PERF
  std::vector<size_t> test_sizes = {};
#else
  std::vector<size_t> test_sizes = {10000000};
#endif

  BenchmarkResults results2;
  constexpr size_t kSize = 5000000;
  constexpr size_t kRuns = 2;

#ifndef RTBOT_PERF
  // Warmup pass — one run per workload, discarded. Primes CPU boost clocks,
  // allocator arenas, and page cache so the first measured run isn't biased.
  std::cout << "Warmup pass...\n";
  bollinger_benchmark.benchmark_single_size(kSize);
  {
    BollingerBandsPureBenchmark pure;
    pure.benchmark_single_size(kSize);
  }
  ppg_benchmark.benchmark_single_size(kSize);

  std::vector<double> bollinger_times, pure_times, ppg_times;
  bollinger_times.reserve(kRuns);
  pure_times.reserve(kRuns);
  ppg_times.reserve(kRuns);

  // Interleave workloads across iterations: any slow drift in machine state
  // (thermal, scheduler, background load) hits all three equally per cycle
  // rather than biasing whichever workload runs last.
  std::cout << "Measuring (" << kRuns << " runs, interleaved)...\n";
  for (size_t i = 0; i < kRuns; ++i) {
    std::cout << "  iter " << (i + 1) << "/" << kRuns << "\n";
    bollinger_times.push_back(bollinger_benchmark.benchmark_single_size(kSize).first);
    {
      BollingerBandsPureBenchmark pure;
      pure_times.push_back(pure.benchmark_single_size(kSize).first);
    }
    ppg_times.push_back(ppg_benchmark.benchmark_single_size(kSize).first);
  }

  results2.add_result("Bollinger Program", kSize, std::move(bollinger_times));
  results2.add_result("Bollinger Pure", kSize, std::move(pure_times));
  results2.add_result("PPG Pipeline", kSize, std::move(ppg_times));
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