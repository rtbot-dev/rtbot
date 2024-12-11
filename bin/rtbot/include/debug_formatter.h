#include <iomanip>
#include <sstream>

namespace rtbot_cli {

class DebugFormatter {
 public:
  static std::string format_debug_output(const nlohmann::json& j) {
    std::stringstream ss;
    for (auto it = j.begin(); it != j.end(); ++it) {
      const std::string& op_id = it.key();
      const auto& op_data = it.value();

      // Extract operator type from program structure or default to op_id
      std::string op_type = op_id;
      size_t underscore_pos = op_id.find('_');
      if (underscore_pos != std::string::npos) {
        op_type = op_id.substr(0, underscore_pos);
      }

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