#include <benchmark/benchmark.h>

#ifdef __APPLE__
#define BENCHMARK_OS_MAC
#endif

#include <cmath>
#include <iostream>

#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/std/MathSyncBinaryOp.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "tools.h"

using namespace rtbot;

class RtBotFixture : public benchmark::Fixture {
 public:
  std::shared_ptr<Input> i1;
  std::unique_ptr<SamplePPG> s;
  std::shared_ptr<MovingAverage> ma1;
  std::shared_ptr<MovingAverage> ma2;
  std::shared_ptr<Subtraction> diff;
  std::shared_ptr<PeakDetector> peak;
  std::shared_ptr<Join> join;
  std::shared_ptr<Output> o1;

  void SetUp(const ::benchmark::State& state) {
    if (!i1) {
      s = std::make_unique<SamplePPG>("examples/data/ppg.csv");
      i1 = std::make_shared<Input>("i1", std::vector<std::string>{PortType::NUMBER});
      ma1 = std::make_shared<MovingAverage>("ma1", round(50 / s->dt()));
      ma2 = std::make_shared<MovingAverage>("ma2", round(2000 / s->dt()));
      diff = std::make_shared<Subtraction>("diff");
      peak = std::make_shared<PeakDetector>("peak", 2 * round(50 / s->dt()) + 1);
      join = std::make_shared<Join>("join", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER});
      o1 = std::make_shared<Output>("o1", std::vector<std::string>{PortType::NUMBER});

      // Build pipeline
      i1->connect(ma1)->connect(diff, 0, 0)->connect(peak)->connect(join, 0, 0)->connect(o1);
      i1->connect(ma2)->connect(diff, 0, 1);
      i1->connect(join, 0, 1);
      i1->connect(o1);
    }
  }

  void TearDown(const ::benchmark::State& state) {}

  int Process() {
    uint passes = 10000;
    // process the data
    for (auto j = 0u; j < passes; j++) {
      for (auto i = 0u; i < s->ti.size(); i++) {
        i1->receive_data(create_message<NumberData>(s->ti[i], NumberData{s->ppg[i]}), 0);
        i1->execute();
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
    ->Name("Process sample size 7195 * 10000 rows")
    ->Unit(benchmark::kMillisecond)
    ->Range(1, 10000);

BENCHMARK_MAIN();