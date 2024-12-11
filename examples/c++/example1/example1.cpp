#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/bindings.h"
#include "rtbot/std/PeakDetector.h"

using namespace rtbot;
using namespace std;
std::string program_json = R"({
            "operators": [
                {"type": "Input", "id": "input1", "portTypes": ["number"]},
                {"type": "Output", "id": "output1", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "input1", "to": "output1"}
            ],
            "entryOperator": "input1",
            "outputs": [
                {"operatorId": "output1", "ports": ["o1"]}
            ]
        })";

int main(int argc, char** argv) {
  auto result = create_program("test_prog2", program_json);

  // process the data
  for (int i = 0; i < 10; i++) {
    add_to_message_buffer("test_prog2", "input1", i, i % 5);
    auto result = process_message_buffer("test_prog2");
    cout << "Result: " << result << endl;
  }
}
