#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "rtbot/bindings.h"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: diagnose_file <path_to_json>" << std::endl;
    return 1;
  }

  std::ifstream file(argv[1]);
  if (!file.is_open()) {
    std::cerr << "Could not open file: " << argv[1] << std::endl;
    return 1;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string json_str = buffer.str();

  std::string result = rtbot::diagnose_program(json_str);

  // Pretty print the JSON result
  auto j = nlohmann::json::parse(result);
  std::cout << j.dump(2) << std::endl;

  return 0;
}
