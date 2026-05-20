#ifndef RTBOT_JIT_SEGMENT_PARTITIONER_H
#define RTBOT_JIT_SEGMENT_PARTITIONER_H

#include <string>
#include <vector>

#include "rtbot/compiled/jit/CompiledGraph.h"

namespace rtbot::jit {

struct Segment {
  // Op ids in this segment, in topological order from input toward output.
  std::vector<std::string> op_ids;
};

struct PartitionResult {
  std::vector<Segment> segments;
  // Op ids that are sync/routing ops crossing segment boundaries.
  // Empty for fully-linear graphs (the only case phase 7 supports in IR).
  std::vector<std::string> sync_ops;
};

// Partition the graph at sync boundaries. Sync op kinds: Join, Demux.
// (Mux/Pipeline are composite operators that the JSON parser would already
// have flattened by phase 7; not handled separately here.)
//
// For a fully-linear graph (no sync ops), returns one Segment containing
// every op in topological order, and an empty sync_ops vector.
//
// For a graph with sync ops, returns multiple Segments; sync_ops lists the
// boundary ops. Phase 7 callers reject this case.
PartitionResult partition_segments(const CompiledGraph& graph);

}  // namespace rtbot::jit

#endif  // RTBOT_JIT_SEGMENT_PARTITIONER_H
