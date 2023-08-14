#include <catch2/catch.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"
#include "rtbot/bindings.h"
#include "tools.h"

using namespace rtbot;
using namespace std;

TEST_CASE("read  pipeline test demultiplexer") {
  nlohmann::json json;
  {
    ifstream in("examples/data/program-test-4.json");
    if (!in) throw runtime_error("file not found");
    in >> json;
  }

  SECTION("using the pipeline") {
    auto pipe = FactoryOp::createProgram(json.dump().c_str());

    // process the data
    for (int i = 1; i <= 100; i++) {
      auto output = pipe.receiveDebug(Message<std::uint64_t, double>(i, (i < 20) ? 1 : 2));

      if (i < 21 && i > 1) {
        REQUIRE(output.find("dm")->second.find("o1")->second.size() == 1);
        REQUIRE(output.find("dm")->second.find("o1")->second.at(0).value == 1);
        REQUIRE(output.find("dm")->second.find("o1")->second.at(0).time == (i - 1));

        REQUIRE(output.find("dm")->second.count("o2") == 0);
      } else if (i >= 21) {
        REQUIRE(output.find("dm")->second.find("o2")->second.size() == 1);
        REQUIRE(output.find("dm")->second.find("o2")->second.at(0).value == 2);
        REQUIRE(output.find("dm")->second.find("o2")->second.at(0).time == (i - 1));

        REQUIRE(output.find("dm")->second.count("o1") == 0);
      }
    }
  }
}
