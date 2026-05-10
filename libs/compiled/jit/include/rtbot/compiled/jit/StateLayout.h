#ifndef RTBOT_JIT_STATE_LAYOUT_H
#define RTBOT_JIT_STATE_LAYOUT_H

#include <cstddef>
#include <map>
#include <string>

#include "rtbot/compiled/jit/CompiledGraph.h"

namespace rtbot::jit {

struct StateLayout {
  // Per op id, the offset (in doubles, 0-based) into the segment's flat
  // state buffer where this op's state lives. Stateless ops are still
  // present in the map with the offset set to the value of `total_size`
  // at the time they were visited (a no-op slot).
  //
  // For an op with size 0, the offset is irrelevant — emitters know not
  // to use it. Including the entry simplifies downstream code that doesn't
  // need to special-case stateless lookups.
  std::map<std::string, std::size_t> offsets;

  // Total size of the state buffer in doubles.
  std::size_t total_size{0};

  // Per-slot init value overrides. Default is 0.0 (handled by
  // state_.resize). Map from slot offset (in doubles) to the override
  // value. Used to seed MaxAgg slots to -inf and MinAgg slots to +inf.
  std::map<std::size_t, double> state_init_overrides;
};

// Plan the state layout for a graph. Walks the nodes in `graph.nodes` order
// and assigns sequential offsets. Stateless ops contribute 0 size. Pipeline
// nodes are mutated to record their inner-program state size and init pattern,
// so the caller must supply a non-const graph.
StateLayout plan_state_layout(CompiledGraph& graph);

// Get the state size in doubles for one opcode instance, given its kind
// and parameters. Public so other parts of the compiler can use it.
std::size_t state_size_for(const OpNode& node);

// Maximum number of emissions per segment_fn call across the whole program.
// K = max(1, max Demultiplexer mux_num_ports). Multiplexer always emits at
// most 1, and current trivial/stateful ops all emit at most 1.
std::size_t compute_max_emits_per_call(const CompiledGraph& graph);

// Total scalar width emitted by the program's Output op. Equal to the sum of
// the Output op's per-port widths. For all current opcodes (which are
// width 1) this matches the number of collected output ports, so the JIT
// runtime sees the same num_outputs as before.
std::size_t compute_program_outputs(const CompiledGraph& graph);

}  // namespace rtbot::jit

#endif  // RTBOT_JIT_STATE_LAYOUT_H
