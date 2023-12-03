#include <pico/stdlib.h>
#include <stdio.h>

#include "hardware/adc.h"
#include "iostream"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/std/PeakDetector.h"

using namespace rtbot;
using namespace std;

int main() {
  stdio_init_all();

  // define rtbot pipeline
  // operators
  auto i1 = Input("i1");
  auto peak = PeakDetector("b1", 3);
  auto join = Join<double>("j1", 2);

  // connections
  i1.connect(peak).connect(join, 0);
  i1.connect(join, 1);

  int t = 0;
  int sign = 1;
  double v = 0.0;

  // adc initialization
  adc_init();
  adc_gpio_init(26);
  adc_select_input(0);

  const float conversion_factor_mv = 3300.0f / (1 << 12);
  // main loop
  while (true) {
    // read voltage
    uint16_t mV = adc_read() * conversion_factor_mv;
    cout << "V=" << mV << "mV" << endl;

    Message msg(t, v);
    auto output = i1.receive(msg);
    t++;
    v += sign * 0.1;
    if (t % 10 == 0) sign = -sign;

    cout << "IN ( " << t << ", " << v << ") OUT ->" << endl;
    cout << "{";
    for (const auto& [k, msgs] : output) {
      cout << "  " << k << ": [";
      for (auto m : msgs) {
        cout << "{ t: " << m.time << ", v: " << m.value << " }, ";
      }
      cout << "\b\b], ";
    }
    cout << "\b\b  }" << endl;

    sleep_ms(1000);
  }
}
