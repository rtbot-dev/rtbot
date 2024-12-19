#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "rtbot/extension/LuaOperator.h"
#include "rtbot/std/MovingAverage.h"

using namespace rtbot;
using namespace std::chrono;

// Helper function to measure peak memory usage in bytes
size_t get_peak_memory() {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS_EX pmc;
  GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
  return pmc.PeakWorkingSetSize;
#else
  // On Unix-like systems, read from /proc/self/status
  FILE* file = fopen("/proc/self/status", "r");
  if (!file) return 0;

  size_t peak_kb = 0;
  char line[128];

  while (fgets(line, sizeof(line), file)) {
    if (strncmp(line, "VmPeak:", 7) == 0) {
      sscanf(line, "VmPeak: %zu", &peak_kb);
      break;
    }
  }
  fclose(file);
  return peak_kb * 1024;  // Convert KB to bytes
#endif
}

// Helper to format memory sizes
std::string format_memory(size_t bytes) {
  std::stringstream ss;
  if (bytes < 1024) {
    ss << bytes << " B";
  } else if (bytes < 1024 * 1024) {
    ss << std::fixed << std::setprecision(2) << (bytes / 1024.0) << " KB";
  } else {
    ss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB";
  }
  return ss.str();
}

// Helper function to print benchmark results in a table format
void print_benchmark_results(const std::string& impl_name, double time_ms, size_t memory_bytes, double base_time = 0,
                             size_t base_memory = 0) {
  std::cout << std::left << std::setw(20) << impl_name;
  std::cout << std::right << std::setw(15) << std::fixed << std::setprecision(2) << time_ms << " ms";
  std::cout << std::right << std::setw(20) << format_memory(memory_bytes);

  if (base_time > 0) {
    double time_ratio = time_ms / base_time;
    // Only calculate memory ratio if both values are non-zero
    std::string memory_ratio;
    if (base_memory == 0 && memory_bytes == 0) {
      memory_ratio = "1.00";
    } else if (base_memory == 0) {
      memory_ratio = "∞";
    } else {
      memory_ratio = std::to_string(memory_bytes / static_cast<double>(base_memory));
      // Truncate to 2 decimal places
      size_t decimal_pos = memory_ratio.find('.');
      if (decimal_pos != std::string::npos && decimal_pos + 3 < memory_ratio.length()) {
        memory_ratio = memory_ratio.substr(0, decimal_pos + 3);
      }
    }
    std::cout << std::right << std::setw(15) << std::fixed << std::setprecision(2) << time_ratio << "x";
    std::cout << std::right << std::setw(15) << memory_ratio << "x";
  }
  std::cout << std::endl;
}

// Helper function to verify results match
bool verify_results(const std::vector<std::pair<timestamp_t, double>>& results1,
                    const std::vector<std::pair<timestamp_t, double>>& results2, double epsilon = 1e-10) {
  if (results1.size() != results2.size()) {
    std::cout << "Result size mismatch: " << results1.size() << " vs " << results2.size() << std::endl;
    return false;
  }

  for (size_t i = 0; i < results1.size(); i++) {
    if (results1[i].first != results2[i].first) {
      std::cout << "Timestamp mismatch at index " << i << ": " << results1[i].first << " vs " << results2[i].first
                << std::endl;
      return false;
    }
    if (std::abs(results1[i].second - results2[i].second) > epsilon) {
      std::cout << "Value mismatch at index " << i << ": " << results1[i].second << " vs " << results2[i].second
                << std::endl;
      return false;
    }
  }
  return true;
}

int main() {
  const size_t WINDOW_SIZE = 500;
  const size_t NUM_SAMPLES = 10000;
  const size_t NUM_RUNS = 5;  // Run multiple times for more stable results

  // Create test data
  std::vector<Message<NumberData>> test_data;
  test_data.reserve(NUM_SAMPLES);
  for (size_t i = 0; i < NUM_SAMPLES; i++) {
    test_data.emplace_back(i, NumberData{static_cast<double>(i % 100)});
  }

  // Create Lua implementation
  const std::string lua_code = R"(
        add_data_port()
        add_output_port()

        buffer = {}
        window_size = )" + std::to_string(WINDOW_SIZE) +
                               R"(

        function process_data(input_values, input_times)
            if #input_values[1] == 0 then
                return {[0] = {}}
            end

            local results = {}
            for i = 1, #input_values[1] do
                table.insert(buffer, input_values[1][i])
                if #buffer > window_size then
                    table.remove(buffer, 1)
                end
                
                if #buffer == window_size then
                    local sum = 0
                    for j = 1, #buffer do
                        sum = sum + buffer[j]
                    end
                    table.insert(results, {
                        time = input_times[1][i],
                        value = sum / window_size
                    })
                end
            end
            return {[0] = results}
        end
    )";

  // Print benchmark header
  std::cout << "\nBenchmarking Moving Average Implementation (Window Size: " << WINDOW_SIZE
            << ", Samples: " << NUM_SAMPLES << ")\n";
  std::cout << std::string(85, '-') << std::endl;
  std::cout << std::left << std::setw(20) << "Implementation" << std::right << std::setw(15) << "Time" << std::right
            << std::setw(20) << "Memory" << std::right << std::setw(15) << "Rel. Time" << std::right << std::setw(15)
            << "Rel. Memory" << std::endl;
  std::cout << std::string(85, '-') << std::endl;

  // Store averaged results
  double native_avg_time = 0;
  double lua_avg_time = 0;
  size_t native_avg_memory = 0;
  size_t lua_avg_memory = 0;

  // Store results for validation
  std::vector<std::pair<timestamp_t, double>> native_results;
  std::vector<std::pair<timestamp_t, double>> lua_results;

  // Run benchmarks multiple times
  for (size_t run = 0; run < NUM_RUNS; run++) {
    // Native Implementation
    auto native_op = std::make_unique<MovingAverage>("native_ma", WINDOW_SIZE);
    size_t initial_memory = get_peak_memory();
    auto start_time = high_resolution_clock::now();

    for (const auto& msg : test_data) {
      native_op->receive_data(create_message<NumberData>(msg.time, msg.data), 0);
      native_op->execute();

      // Only store results from first run
      if (run == 0) {
        const auto& output = native_op->get_output_queue(0);
        for (const auto& out_msg : output) {
          const auto* num_msg = dynamic_cast<const Message<NumberData>*>(out_msg.get());
          native_results.emplace_back(num_msg->time, num_msg->data.value);
        }
      }
      native_op->clear_all_output_ports();
    }

    auto end_time = high_resolution_clock::now();
    size_t peak_memory = get_peak_memory() - initial_memory;
    double duration = duration_cast<milliseconds>(end_time - start_time).count();

    native_avg_time += duration;
    native_avg_memory += peak_memory;

    // Lua Implementation
    auto lua_op = std::make_unique<LuaOperator>("lua_ma", lua_code);
    initial_memory = get_peak_memory();
    start_time = high_resolution_clock::now();

    for (const auto& msg : test_data) {
      lua_op->receive_data(create_message<NumberData>(msg.time, msg.data), 0);
      lua_op->execute();

      // Only store results from first run
      if (run == 0) {
        const auto& output = lua_op->get_output_queue(0);
        for (const auto& out_msg : output) {
          const auto* num_msg = dynamic_cast<const Message<NumberData>*>(out_msg.get());
          lua_results.emplace_back(num_msg->time, num_msg->data.value);
        }
      }
      lua_op->clear_all_output_ports();
    }

    end_time = high_resolution_clock::now();
    peak_memory = get_peak_memory() - initial_memory;
    duration = duration_cast<milliseconds>(end_time - start_time).count();

    lua_avg_time += duration;
    lua_avg_memory += peak_memory;
  }

  // Calculate averages
  native_avg_time /= NUM_RUNS;
  lua_avg_time /= NUM_RUNS;
  native_avg_memory /= NUM_RUNS;
  lua_avg_memory /= NUM_RUNS;

  // Print results
  print_benchmark_results("Native", native_avg_time, native_avg_memory);
  print_benchmark_results("Lua", lua_avg_time, lua_avg_memory, native_avg_time, native_avg_memory);

  // Verify results
  std::cout << std::endl << "Verifying results..." << std::endl;
  if (verify_results(native_results, lua_results)) {
    std::cout << "✓ Results match!" << std::endl;
  } else {
    std::cout << "✗ Results differ!" << std::endl;
    return 1;
  }

  return 0;
}