#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/std/PeakDetector.h"

using namespace rtbot;
using namespace std;

int main(int argc, char **argv) {
  auto i1 = Input("i1");
  auto peak = PeakDetector<double>("b1", 3);
  auto join = Join<double>("j1", 2);

  i1.connect(peak).connect(join, 0);
  i1.connect(join, 1);

  // process the data
  for (int i = 0; i < 26; i++) {
    auto output = i1.receive(Message<double>(i, i % 5));
    if (output.find("j1") != output.end()) {
      cout << "ok" << endl;
    }
  }
}
