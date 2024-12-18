#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <stack>

namespace rtbot_cli {

class ProgramViewer {
 private:
  struct Node {
    std::string id;
    std::string type;
    std::string params;
    std::vector<std::string> targets;
    std::vector<std::string> sources;
    bool visited = false;
    size_t depth = 0;
    size_t position = 0;
  };

  static constexpr const char* COLOR_RESET = "\033[0m";
  static constexpr const char* COLOR_OPERATOR = "\033[1;36m";
  static constexpr const char* COLOR_ID = "\033[1;33m";
  static constexpr const char* COLOR_ARROW = "\033[1;35m";
  static constexpr const char* COLOR_PARAM = "\033[1;32m";

  static std::string format_node(const Node& node) {
    std::stringstream ss;
    ss << COLOR_OPERATOR << node.type << COLOR_RESET << "(" << COLOR_ID << node.id << COLOR_RESET;
    if (!node.params.empty()) {
      ss << ", " << COLOR_PARAM << node.params << COLOR_RESET;
    }
    ss << ")";
    return ss.str();
  }

  static std::string get_connection_prefix(size_t source_pos, size_t target_pos) {
    std::string prefix = "\n";
    for (size_t i = 0; i < source_pos; i++) {
      prefix += "    ";
    }
    prefix += "\\";
    return prefix;
  }

  static void calculate_positions(std::map<std::string, Node>& nodes, const std::string& start) {
    std::queue<std::string> q;
    q.push(start);
    nodes[start].depth = 0;
    nodes[start].position = 0;

    size_t current_depth = 0;
    size_t current_pos = 0;

    while (!q.empty()) {
      std::string current = q.front();
      q.pop();

      Node& node = nodes[current];
      if (node.depth > current_depth) {
        current_depth = node.depth;
        current_pos = 0;
      }
      node.position = current_pos++;

      for (const auto& target : node.targets) {
        Node& target_node = nodes[target];
        if (target_node.depth <= node.depth) {
          target_node.depth = node.depth + 1;
          q.push(target);
        }
      }
    }
  }

 public:
  static std::string visualize_program(const nlohmann::json& program) {
    try {
      if (!program.contains("operators") || !program.contains("connections") || !program.contains("entryOperator")) {
        return "Error: Invalid program structure\n";
      }

      std::map<std::string, Node> nodes;
      std::string entry_id = program["entryOperator"];
      std::stringstream result;

      // Build nodes
      for (const auto& op : program["operators"]) {
        Node node;
        node.id = op["id"].get<std::string>();
        node.type = op["type"].get<std::string>();

        if (op["type"] == "Scale" && op.contains("value")) {
          node.params = std::to_string(op["value"].get<double>());
        } else if ((op["type"] == "MovingAverage" || op["type"] == "StandardDeviation") && op.contains("window_size")) {
          node.params = std::to_string(op["window_size"].get<int>());
        } else if (op["type"] == "ResamplerHermite" && op.contains("interval")) {
          node.params = std::to_string(op["interval"].get<int>());
        }

        nodes[node.id] = node;
      }

      // Build connections
      for (const auto& conn : program["connections"]) {
        std::string from = conn["from"].get<std::string>();
        std::string to = conn["to"].get<std::string>();
        nodes[from].targets.push_back(to);
        nodes[to].sources.push_back(from);
      }

      // Calculate node positions
      calculate_positions(nodes, entry_id);

      // Find main path (longest path from entry)
      std::vector<std::string> main_path;
      std::string current = entry_id;
      std::set<std::string> visited;

      while (!visited.count(current)) {
        main_path.push_back(current);
        visited.insert(current);

        if (nodes[current].targets.empty()) break;

        // Choose the next node with lowest depth
        std::string next;
        size_t min_depth = SIZE_MAX;
        for (const auto& target : nodes[current].targets) {
          if (!visited.count(target) && nodes[target].depth < min_depth) {
            min_depth = nodes[target].depth;
            next = target;
          }
        }

        if (next.empty()) break;
        current = next;
      }

      // Print main path
      if (!main_path.empty()) {
        result << format_node(nodes[main_path[0]]);
        for (size_t i = 1; i < main_path.size(); i++) {
          result << COLOR_ARROW << " -> " << COLOR_RESET << format_node(nodes[main_path[i]]);
        }

        // Print branch connections
        for (size_t i = 0; i < main_path.size(); i++) {
          const auto& node = nodes[main_path[i]];
          for (const auto& target : node.targets) {
            if (std::find(main_path.begin(), main_path.end(), target) == main_path.end()) {
              result << get_connection_prefix(i, nodes[target].position) << COLOR_ARROW << "-> " << COLOR_RESET
                     << format_node(nodes[target]);
            }
          }
        }
      }

      result << "\n";
      return result.str();

    } catch (const std::exception& e) {
      return "Error visualizing program: " + std::string(e.what()) + "\n";
    }
  }
};

}  // namespace rtbot_cli