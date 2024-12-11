#include <catch2/catch.hpp>
#include <memory>
#include <vector>

#include "rtbot/Demultiplexer.h"
#include "rtbot/Input.h"
#include "rtbot/Output.h"
#include "rtbot/std/Constant.h"
#include "rtbot/std/Count.h"
#include "rtbot/std/CumulativeSum.h"
#include "rtbot/std/Difference.h"
#include "rtbot/std/FilterScalarOp.h"
#include "rtbot/std/Linear.h"
#include "rtbot/std/MathScalarOp.h"
#include "rtbot/std/MathSyncBinaryOp.h"
#include "rtbot/std/TimeShift.h"
#include "rtbot/std/Variable.h"

using namespace rtbot;

SCENARIO("Building RSI calculation from operators", "[rsi][integration]") {
  GIVEN("A collection of operators connected to calculate RSI with period 14") {
    const double n = 14.0;  // RSI period

    // Create input/output operators
    auto input = std::make_shared<Input>("input", std::vector<std::string>{PortType::NUMBER});
    auto output = std::make_shared<Output>("output", std::vector<std::string>{PortType::NUMBER});

    // Core operators for counting and switching
    auto count = std::make_shared<Count<NumberData>>("count");
    auto dm = std::make_shared<Demultiplexer<NumberData>>("dm", 2);

    // Control flow operators
    auto lt = std::make_shared<LessThan>("lt", n + 1.0);
    auto et = std::make_shared<EqualTo>("et", n + 1.0);
    auto gt = std::make_shared<GreaterThan>("gt", n + 1.0);
    auto etn2 = std::make_shared<EqualTo>("etn2", n + 2.0);

    // Constants for control flow with BooleanData
    auto cgtz = std::make_shared<Constant<NumberData, BooleanData>>("cgtz", BooleanData{false});
    auto cgto = std::make_shared<Constant<NumberData, BooleanData>>("cgto", BooleanData{true});
    auto cltz = std::make_shared<Constant<NumberData, BooleanData>>("cltz", BooleanData{false});
    auto clto = std::make_shared<Constant<NumberData, BooleanData>>("clto", BooleanData{true});
    auto ceto = std::make_shared<Constant<NumberData, BooleanData>>("ceto", BooleanData{true});

    auto diff1 = std::make_shared<Difference>("diff1");
    auto diff2 = std::make_shared<Difference>("diff2");
    auto gt0 = std::make_shared<GreaterThan>("gt0", 0.0);
    auto et0 = std::make_shared<EqualTo>("et0", 0.0);
    auto lt0 = std::make_shared<LessThan>("lt0", 0.0);
    auto sum0 = make_cumulative_sum("sum0");
    auto sc0 = std::make_shared<Scale>("sc0", 1.0 / n);
    auto coef = std::vector<double>{(n - 1) / n, 1.0 / n};
    auto l1 = std::make_shared<Linear>("l1", coef);
    auto neg0 = std::make_shared<Scale>("neg0", -1.0);
    auto sum1 = make_cumulative_sum("sum1");
    auto sc1 = std::make_shared<Scale>("sc1", 1.0 / n);
    auto l2 = std::make_shared<Linear>("l2", coef);

    auto gt1 = std::make_shared<GreaterThan>("gt1", 0.0);
    auto et1 = std::make_shared<EqualTo>("et1", 0.0);
    auto lt1 = std::make_shared<LessThan>("lt1", 0.0);

    auto const0 = make_constant_number("const0", 0.0);
    auto const1 = make_constant_number("const1", 0.0);
    auto neg1 = std::make_shared<Scale>("neg1", -1.0);

    auto varg = std::make_shared<Variable>("varg");
    auto varl = std::make_shared<Variable>("varl");
    auto ts1 = std::make_shared<TimeShift>("ts1", 1);
    auto ts2 = std::make_shared<TimeShift>("ts2", 1);

    auto divide = std::make_shared<Division>("divide");
    auto add1 = std::make_shared<Add>("add1", 1.0);
    auto power_1 = std::make_shared<Power>("power_1", -1.0);
    auto scale_100 = std::make_shared<Scale>("scale_100", -100.0);
    auto add100 = std::make_shared<Add>("add100", 100.0);

    // Connect input processing
    input->connect(dm, 0, 0);
    input->connect(count);
    count->connect(lt);
    count->connect(gt);
    count->connect(et);
    count->connect(etn2);

    // Connect demultiplexer control
    lt->connect(clto);
    clto->connect(dm, 0, 0, PortKind::CONTROL);
    lt->connect(cltz);
    cltz->connect(dm, 0, 1, PortKind::CONTROL);

    et->connect(ceto);
    ceto->connect(dm, 0, 0, PortKind::CONTROL);
    ceto->connect(dm, 0, 1, PortKind::CONTROL);

    gt->connect(cgto);
    gt->connect(cgtz);
    cgto->connect(dm, 0, 1, PortKind::CONTROL);
    cgtz->connect(dm, 0, 0, PortKind::CONTROL);

    // Connect price change paths
    dm->connect(diff1, 0);

    // First path - gains
    diff1->connect(gt0);
    gt0->connect(sum0);
    sum0->connect(sc0);
    sc0->connect(varg);
    varg->connect(ts1);
    ts1->connect(l1, 0, 0);
    l1->connect(ts1);

    // Second path - no change
    diff1->connect(et0);
    et0->connect(sum0);
    et0->connect(sum1);

    // Third path - losses
    diff1->connect(lt0);
    lt0->connect(neg0);
    neg0->connect(sum1);
    sum1->connect(sc1);
    sc1->connect(varl);
    varl->connect(ts2);
    ts2->connect(l2, 0, 0);
    l2->connect(ts2);

    // Connect diff2 paths
    dm->connect(diff2, 1);
    diff2->connect(gt1);
    gt1->connect(l1, 0, 1);
    gt1->connect(const0);
    const0->connect(l2, 0, 1);

    diff2->connect(lt1);
    lt1->connect(neg1);
    neg1->connect(l2, 0, 1);
    lt1->connect(const1);
    const1->connect(l1, 0, 1);

    diff2->connect(et1);
    et1->connect(l1, 0, 1);
    et1->connect(l2, 0, 1);

    etn2->connect(varg);
    et->connect(varg, 0, 0, PortKind::CONTROL);
    etn2->connect(varl);
    et->connect(varl, 0, 0, PortKind::CONTROL);

    // Connect RSI calculation chain
    varg->connect(divide, 0, 0);
    varl->connect(divide, 0, 1);
    l1->connect(divide, 0, 0);
    l2->connect(divide, 0, 1);

    divide->connect(add1);
    add1->connect(power_1);
    power_1->connect(scale_100);
    scale_100->connect(add100);
    add100->connect(output);

    WHEN("Processing a sequence of price data") {
      std::vector<std::pair<timestamp_t, double>> test_data = {
          {1, 54.8},  {2, 56.8},   {3, 57.85},  {4, 59.85},  {5, 60.57},  {6, 61.1},  {7, 62.17},
          {8, 60.6},  {9, 62.35},  {10, 62.15}, {11, 62.35}, {12, 61.45}, {13, 62.8}, {14, 61.37},
          {15, 62.5}, {16, 62.57}, {17, 60.8},  {18, 59.37}, {19, 60.35}, {20, 62.35}};

      std::vector<std::pair<timestamp_t, double>> outputs;

      for (const auto& [time, price] : test_data) {
        // Clear all operator outputs
        std::vector<std::shared_ptr<Operator>> ops = {
            input, dm,   count, lt,   et,     gt,     etn2, diff1,  diff2, gt0,     et0,       lt0,    gt1,    et1,
            lt1,   l1,   l2,    neg0, neg1,   varg,   varl, divide, add1,  power_1, scale_100, add100, output, cgtz,
            cgto,  cltz, clto,  ceto, const0, const1, ts1,  ts2,    sc0,   sc1,     sum0,      sum1};

        for (auto& op : ops) {
          op->clear_all_output_ports();
        }

        // Process new data point
        input->receive_data(create_message<NumberData>(time, NumberData{price}), 0);
        input->execute();

        const auto& output_queue = output->get_output_queue(0);

        for (const auto& msg : output_queue) {
          const auto* num_msg = dynamic_cast<const Message<NumberData>*>(msg.get());
          outputs.emplace_back(num_msg->time, num_msg->data.value);
        }
      }

      THEN("Output matches expected RSI behavior") {
        // Check specific expected values
        REQUIRE(outputs[0].second == Approx(74.21384).margin(0.00001));
        REQUIRE(outputs[0].first == 15);
        REQUIRE(outputs[1].second == Approx(74.33552).margin(0.00001));
        REQUIRE(outputs[1].first == 16);
      }
    }
  }
}