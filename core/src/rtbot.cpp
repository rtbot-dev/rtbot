#include "Operator.h"
#include "yaml-cpp/yaml.h"
#include <ctime>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  YAML::Node primes = YAML::Load("[2, 3, 5, 7, 11]");
  for (std::size_t i = 0; i < primes.size(); i++) {
    std::cout << primes[i].as<int>() << "\n";
  }

  for (const auto &x : primes)
    std::cout << x << "\n";

  return 0;
}