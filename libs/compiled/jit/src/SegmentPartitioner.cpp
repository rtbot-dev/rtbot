#include "rtbot/compiled/jit/SegmentPartitioner.h"

#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rtbot::jit {

namespace {

bool is_sync_op(OpKind kind) {
  return kind == OpKind::Join || kind == OpKind::Demux ||
         kind == OpKind::Mux  ||
         kind == OpKind::Linear || kind == OpKind::ReduceJoin ||
         kind == OpKind::VectorCompose ||
         kind == OpKind::FusedExpression ||
         kind == OpKind::Pipeline ||
         kind == OpKind::KeyedPipeline;
}

}  // namespace

PartitionResult partition_segments(const CompiledGraph& graph) {
  // Build id → node index map for quick lookup.
  std::unordered_map<std::string, std::size_t> id_to_idx;
  id_to_idx.reserve(graph.nodes.size());
  for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
    id_to_idx[graph.nodes[i].id] = i;
  }

  // Build forward adjacency list and in-degree map (Kahn's algorithm).
  std::unordered_map<std::string, std::vector<std::string>> forward;
  std::unordered_map<std::string, int> in_degree;

  for (const auto& node : graph.nodes) {
    forward[node.id];   // ensure every node has an entry even with no edges
    in_degree[node.id]; // same for in-degree
  }

  for (const auto& conn : graph.connections) {
    // Guard against duplicate edges contributing to in-degree more than once.
    // Use a set per target to deduplicate same from→to pairs at in-degree level.
    forward[conn.from_id].push_back(conn.to_id);
    in_degree[conn.to_id]++;
  }

  // StateLoad nodes reference a source op via state_source_id but have no
  // explicit data connection from that op. Add an implicit ordering edge so
  // the topological sort places the source op before the StateLoad node.
  for (const auto& node : graph.nodes) {
    if (node.kind == OpKind::StateLoad && !node.state_source_id.empty()) {
      forward[node.state_source_id].push_back(node.id);
      in_degree[node.id]++;
    }
  }

  // Kahn's topological sort.
  std::queue<std::string> ready;
  for (const auto& node : graph.nodes) {
    if (in_degree[node.id] == 0) {
      ready.push(node.id);
    }
  }

  std::vector<std::string> topo_order;
  topo_order.reserve(graph.nodes.size());

  while (!ready.empty()) {
    std::string cur = ready.front();
    ready.pop();
    topo_order.push_back(cur);

    for (const auto& succ : forward[cur]) {
      if (--in_degree[succ] == 0) {
        ready.push(succ);
      }
    }
  }

  // Walk topological order and split at sync ops.
  PartitionResult result;
  Segment current_segment;

  for (const auto& id : topo_order) {
    const OpNode& node = graph.nodes[id_to_idx[id]];
    if (is_sync_op(node.kind)) {
      // Flush the current segment if it has any ops.
      if (!current_segment.op_ids.empty()) {
        result.segments.push_back(std::move(current_segment));
        current_segment = Segment{};
      }
      result.sync_ops.push_back(id);
      // The sync op itself is not placed in any segment; start a fresh one
      // for the downstream ops.
    } else {
      current_segment.op_ids.push_back(id);
    }
  }

  // Flush the trailing segment.
  if (!current_segment.op_ids.empty()) {
    result.segments.push_back(std::move(current_segment));
  }

  return result;
}

}  // namespace rtbot::jit
