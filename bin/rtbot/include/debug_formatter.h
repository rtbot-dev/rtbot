#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace rtbot_cli {

class DebugFormatter {
 public:
  static std::string format_debug_output(const nlohmann::json& j, const nlohmann::json& program) {
    // Build operator type lookup map
    std::unordered_map<std::string, std::string> operator_types;
    if (program.contains("operators") && program["operators"].is_array()) {
      for (const auto& op : program["operators"]) {
        if (op.contains("id") && op.contains("type")) {
          operator_types[op["id"]] = op["type"];
        }
      }
    }

    std::stringstream ss;
    for (auto it = j.begin(); it != j.end(); ++it) {
      const std::string& op_id = it.key();
      const auto& op_data = it.value();

      // Look up operator type from program structure
      std::string op_type = operator_types.count(op_id) ? operator_types[op_id] : "Unknown";

      bool first_port = true;
      for (auto port_it = op_data.begin(); port_it != op_data.end(); ++port_it) {
        if (first_port) {
          ss << op_type << "(" << op_id << "):" << port_it.key() << " -> ";
          first_port = false;
        } else {
          ss << std::string(op_type.length() + op_id.length() + 3, ' ') << ":" << port_it.key() << " -> ";
        }

        // Format messages
        ss << format_messages(port_it.value());

        if (std::next(port_it) != op_data.end()) {
          ss << "\n";
        }
      }

      if (std::next(it) != j.end()) {
        ss << "\n";
      }
    }
    return ss.str();
  }

 private:
  static std::string format_messages(const nlohmann::json& messages) {
    std::stringstream ss;
    bool first = true;

    for (const auto& msg : messages) {
      if (!first) ss << ", ";
      ss << "(" << msg["time"].get<uint64_t>() << ", ";
      ss << std::fixed << std::setprecision(6) << msg["value"].get<double>() << ")";
      first = false;
    }

    return ss.str();
  }
};

}  // namespace rtbot_cli