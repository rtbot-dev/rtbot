#include <ctime>
#include <string>
#include <iostream>
#include "yaml-cpp/yaml.h"

int main(int argc, char** argv) {
  YAML::Node primes = YAML::Load("[2, 3, 5, 7, 11]");
  for (std::size_t i=0;i<primes.size();i++) {
    std::cout << primes[i].as<int>() << "\n";
  }

  for(const auto& x:primes)
    std::cout << x << "\n";

  return 0;
}