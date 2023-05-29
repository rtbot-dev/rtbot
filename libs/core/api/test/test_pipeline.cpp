#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"
#include "rtbot/bindings.h"
#include "tools.h"

using namespace rtbot;
using namespace std;

TEST_CASE("read ppg pipeline") {
  nlohmann::json json;
  {
    ifstream in("examples/data/ppg.json");
    if (!in) throw runtime_error("file ppg.json not found");
    in >> json;
  }

  auto s = SamplePPG("examples/data/ppg.csv");

  SECTION("using the pipeline") {
    auto pipe = FactoryOp::createPipeline(json.dump().c_str());

    // process the data
    for (auto i = 0u; i < s.ti.size(); i++) {
      auto y = pipe.receive(Message<std::uint64_t, double>(s.ti[i], s.ppg[i]))[0];
      if (y) cout << y.value() << endl;
    }
  }

  SECTION("using the bindings") {
    createPipeline("pipe1", json.dump());
    // process the data
    for (auto i = 0u; i < s.ti.size(); i++) {
      auto y = receiveMessageInPipeline("pipe1", Message<std::uint64_t, double>(s.ti[i], s.ppg[i]))[0];
      if (y) cout << y.value() << endl;
    }
  }
}
