#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/Program.h"
#include "rtbot/bindings.h"
#include "rtbot/std/MathSyncBinaryOp.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "tools.h"

using namespace rtbot;

std::string format_throughput(double messages_per_second) {
  const std::vector<std::pair<double, std::string>> units = {{1e9, "G"}, {1e6, "M"}, {1e3, "K"}};

  for (const auto& [threshold, prefix] : units) {
    if (messages_per_second >= threshold) {
      return std::to_string(static_cast<int>(messages_per_second * 1000 / threshold) / 1000) + " " + prefix + "/s";
    }
  }

  return std::to_string(static_cast<int>(messages_per_second)) + "/s";
}

// Common utility classes
class BenchmarkResults {
 public:
  void add_result(const std::string& name, size_t data_size, double total_time_ns, double messages_per_second) {
    results.push_back({name, data_size, total_time_ns, messages_per_second});
  }

  void print_results() const {
    std::cout << "\nBenchmark Results:\n";
    std::cout << std::setw(20) << "Test" << std::setw(15) << "Data Size" << std::setw(20) << "Total Time (ms)"
              << std::setw(25) << "Messages/Second\n";
    std::cout << std::string(80, '-') << "\n";

    for (const auto& result : results) {
      std::cout << std::setw(20) << result.name << std::setw(15) << result.data_size << std::setw(20) << std::fixed
                << std::setprecision(2) << result.total_time_ns / 1e6 << std::setw(25) << std::fixed
                << std::setprecision(2) << format_throughput(result.messages_per_second) << "\n";
    }
  }

 private:
  struct Result {
    std::string name;
    size_t data_size;
    double total_time_ns;
    double messages_per_second;
  };
  std::vector<Result> results;
};

class SignalGenerator {
 public:
  static std::vector<double> generate_random_walk(size_t length, double start_price = 100.0) {
    std::random_device rd;
    std::mt19937 gen(rd());
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
      // When we reach the end of dataset, reset index and increase time offset
      if (data_index >= times_.size()) {
        data_index = 0;
        time_offset = times_[times_.size() - 1] - times_[0] + dt_;
      }

      timestamp_t current_time = times_[data_index] + time_offset;
      pipeline.input->receive_data(create_message<NumberData>(current_time, NumberData{values_[data_index]}), 0);
      pipeline.input->execute();
      pipeline.clear_all_queues();

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

    void clear_all_queues() {
      input->clear_all_output_ports();
      ma_short->clear_all_output_ports();
      ma_long->clear_all_output_ports();
      minus->clear_all_output_ports();
      peak->clear_all_output_ports();
      join->clear_all_output_ports();
      output->clear_all_output_ports();
    }
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

    p.input->connect(p.ma_short)->connect(p.minus, 0, 0)->connect(p.peak)->connect(p.join, 0, 0)->connect(p.output);
    p.input->connect(p.ma_long)->connect(p.minus, 0, 1);
    p.input->connect(p.join, 0, 1);

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

    void clear_all_queues() {
      input->clear_all_output_ports();
      resampler->clear_all_output_ports();
      ma->clear_all_output_ports();
      sd->clear_all_output_ports();
      scale->clear_all_output_ports();
      upper->clear_all_output_ports();
      lower->clear_all_output_ports();
      output->clear_all_output_ports();
    }
  };

  std::pair<double, double> benchmark_single_size(size_t data_size) {
    auto prices = SignalGenerator::generate_random_walk(data_size);
    auto pipeline = create_pipeline();

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < data_size; ++i) {
      pipeline.input->receive_data(create_message<NumberData>(i, NumberData{prices[i]}), 0);
      pipeline.input->execute();
      pipeline.clear_all_queues();
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
    p.upper = std::make_shared<Addition>("upper");
    p.lower = std::make_shared<Subtraction>("lower");
    p.output = std::make_shared<Output>("output",
                                        std::vector<std::string>{PortType::NUMBER, PortType::NUMBER, PortType::NUMBER});

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

  // Define test sizes
  std::vector<size_t> test_sizes = {1000, 10000, 100000, 1000000, 10000000, 100000000};

  BenchmarkResults results;

  // Run benchmarks
  for (size_t size : test_sizes) {
    // Bollinger Bands
    auto bollinger_result = bollinger_benchmark.benchmark_single_size(size);
    results.add_result("Bollinger Bands", size, bollinger_result.first, bollinger_result.second);

    // Bollinger Bands Pure Operators
    BollingerBandsPureBenchmark bollinger_pure_benchmark;
    auto bollinger_pure_result = bollinger_pure_benchmark.benchmark_single_size(size);
    results.add_result("Bollinger Pure", size, bollinger_pure_result.first, bollinger_pure_result.second);
  }

  results.print_results();

  BenchmarkResults results2;

  // Run benchmarks
  for (size_t size : test_sizes) {
    // PPG Pipeline
    auto ppg_result = ppg_benchmark.benchmark_single_size(size);
    results2.add_result("PPG Pipeline", size, ppg_result.first, ppg_result.second);
  }

  results2.print_results();
  return 0;
}