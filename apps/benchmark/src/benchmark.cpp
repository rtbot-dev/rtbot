#include <benchmark/benchmark.h>

#include <cmath>
#include <iostream>

#include "rtbot/Collector.h"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/std/Minus.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "tools.h"

using namespace rtbot;

class RtBotFixture : public benchmark::Fixture {
  public:
    std::unique_ptr<Input<uint64_t, double>> i1;
    std::unique_ptr<SamplePPG> s;
    std::unique_ptr<MovingAverage<uint64_t, double>> ma1;
    std::unique_ptr<MovingAverage<uint64_t, double>> ma2;
    std::unique_ptr<Minus<uint64_t, double>> diff;
    std::unique_ptr<PeakDetector<uint64_t, double>> peak;
    std::unique_ptr<Join<uint64_t, double>> join;
    std::unique_ptr<Output<uint64_t, double>> o1;

  void SetUp(const ::benchmark::State& state) {
    if (i1.get() == nullptr) {
      s = std::make_unique<SamplePPG>("examples/data/ppg.csv");
      i1 = std::make_unique<Input<uint64_t, double>>("i1");
      ma1 = std::make_unique<MovingAverage<uint64_t, double>>("ma1", round(50 / s->dt()));
      ma2 = std::make_unique<MovingAverage<uint64_t, double>>("ma2", round(2000 / s->dt()));
      diff = std::make_unique<Minus<uint64_t, double>>("diff");
      peak = std::make_unique<PeakDetector<uint64_t, double>>("peak", 2 * ma1->getDataInputMaxSize() + 1);
      join = std::make_unique<Join<uint64_t, double>>("join", 2);
      o1 = std::make_unique<Output<uint64_t, double>>("o1");

      // draw the pipeline
      i1->connect(*ma1)
          ->connect(*diff, "o1", "i1")
          ->connect(*peak, "o1", "i1")
          ->connect(*join, "o1", "i1")
          ->connect(*o1, "o1", "i1");
      i1->connect(*ma2)->connect(*diff, "o1", "i2");
      i1->connect(*join, "o1", "i2");
    }
  }

  void TearDown(const ::benchmark::State& state) {}

  int Process() {
    uint passes = 100;
    // process the data
    for (auto j = 0u; j < passes; j++) {
      for (auto i = 0u; i < s->ti.size(); i++) {
        i1->receiveData(Message<uint64_t, double>(s->ti[i], s->ppg[i]));
        i1->executeData();
      }
    }

    return passes * s->ti.size();
  }
};


BENCHMARK_DEFINE_F(RtBotFixture, Process)(benchmark::State& state) {
  int rows = 0;
  for (auto _ : state) {
    rows = Process();
  }
  state.counters["rows"] += rows;
  state.counters["rowsRate"] = benchmark::Counter(rows, benchmark::Counter::kIsRate);
}

// Register the function as a benchmark
BENCHMARK_REGISTER_F(RtBotFixture, Process)
    ->Name("Process sample size 7195 * 100 rows")
    ->Unit(benchmark::kMillisecond)
    ->Range(1, 1000);

BENCHMARK_MAIN();
