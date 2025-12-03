#include <catch2/catch.hpp>

#include "rtbot/std/CumulativeProduct.h"

using namespace rtbot;

SCENARIO("CumulativeProduct multiplies incoming samples", "[cumulative_product]") {
  GIVEN("A CumulativeProduct operator") {
    CumulativeProduct cp("cp");

    WHEN("messages arrive") {
      cp.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      cp.execute();
      cp.receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
      cp.execute();

      const auto& output = cp.get_output_queue(0);
      REQUIRE(output.size() == 2);
      const auto* first = dynamic_cast<const Message<NumberData>*>(output.front().get());
      REQUIRE(first != nullptr);
      REQUIRE(first->data.value == Approx(2.0));
      const auto* second = dynamic_cast<const Message<NumberData>*>(output.back().get());
      REQUIRE(second != nullptr);
      REQUIRE(second->data.value == Approx(6.0));
    }
  }
}
