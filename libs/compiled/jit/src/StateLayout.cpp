#include "rtbot/compiled/jit/StateLayout.h"

#include <limits>

#include "rtbot/compiled/JoinStage.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedStateLayout.h"

namespace rtbot::jit {

std::size_t state_size_for(const OpNode& node) {
  switch (node.kind) {
    // Stateless arithmetic
    case OpKind::Add:
    case OpKind::Sub:
    case OpKind::Mul:
    case OpKind::Div:
    case OpKind::Scale:
    case OpKind::AddScalar:
    case OpKind::PowerScalar:
    // Stateless transcendental
    case OpKind::Pow:
    case OpKind::Abs:
    case OpKind::Sqrt:
    case OpKind::Log:
    case OpKind::Log10:
    case OpKind::Exp:
    case OpKind::Sin:
    case OpKind::Cos:
    case OpKind::Tan:
    case OpKind::Sign:
    case OpKind::Floor:
    case OpKind::Ceil:
    case OpKind::Round:
    case OpKind::Neg:
    // Stateless comparison
    case OpKind::Gt:
    case OpKind::Gte:
    case OpKind::Lt:
    case OpKind::Lte:
    case OpKind::Eq:
    case OpKind::Neq:
    case OpKind::EqTol:
    case OpKind::NeqTol:
    case OpKind::GtScalar:
    case OpKind::LtScalar:
    case OpKind::GteScalar:
    case OpKind::LteScalar:
    case OpKind::EqScalar:
    case OpKind::NeqScalar:
    // Stateless boolean
    case OpKind::And:
    case OpKind::Or:
    case OpKind::Not:
    case OpKind::Xor:
    case OpKind::Nand:
    case OpKind::Nor:
    case OpKind::Xnor:
    case OpKind::Implication:
    // Predicate filters (stateless conditional emit)
    case OpKind::FiltGtScalar:
    case OpKind::FiltLtScalar:
    case OpKind::FiltEqScalar:
    case OpKind::FiltNeqScalar:
    case OpKind::FiltGtSync:
    case OpKind::FiltLtSync:
    case OpKind::FiltEqSync:
    case OpKind::FiltNeqSync:
    // Stateless trivial 1->1
    case OpKind::Identity:
    case OpKind::Constant:
    case OpKind::BooleanToNumber:
    case OpKind::TimeShift:
    case OpKind::TimestampExtract:
    case OpKind::LessThanOrEqualToReplace:
    case OpKind::Function:
    // Stateless vector ops
    case OpKind::VectorExtract:
    case OpKind::VectorProject:
    // I/O boundaries — not stateful in the JIT path
    case OpKind::Input:
    case OpKind::Output:
    // Gate has no per-instance state
    case OpKind::Gate:
    // StateLoad borrows another opcode's slot; no state of its own
    case OpKind::StateLoad:
      return 0;

    // Demultiplexer state layout: 1 data port + N control ports each holding
    // a Join-style PortBuffer<64> (130 doubles per port).
    //   ports [0]       : data port (queue of incoming data values)
    //   ports [1..N]    : control ports (queue of incoming control booleans)
    case OpKind::Demux: {
      std::size_t n = node.mux_num_ports;
      if (n == 0) n = 1;
      return (1 + n) * (2 * rtbot::compiled::kJoinPortCapacity + 2);
    }

    // Multiplexer state layout: N data ports + N control ports each holding
    // a Join-style PortBuffer<64> (130 doubles per port).
    //   ports [0..N)    : data ports
    //   ports [N..2N)   : control ports
    case OpKind::Mux: {
      std::size_t n = node.mux_num_ports;
      if (n == 0) n = 1;
      return 2 * n * (2 * rtbot::compiled::kJoinPortCapacity + 2);
    }

    // CumSum: sum + Kahan compensation = 2
    case OpKind::CumSum:
      return 2;

    // Count: running counter = 1
    case OpKind::Count:
      return 1;

    // MaxAgg: running maximum = 1 (init to -inf via state_init_overrides)
    case OpKind::MaxAgg:
      return 1;

    // MinAgg: running minimum = 1 (init to +inf via state_init_overrides)
    case OpKind::MinAgg:
      return 1;

    // MovingAverage: ring buffer (W) + sum + comp + count = W + 3
    case OpKind::MovingAverage:
      return node.window_size + 3;

    // MovingSum: same layout as MovingAverage = W + 3
    case OpKind::MovingSum:
      return node.window_size + 3;

    // StdDev: same layout as MovingAverage = W + 3
    case OpKind::StdDev:
      return node.window_size + 3;

    // Diff: prev_v, prev_t, curr_t, count = 4
    case OpKind::Diff:
      return 4;

    // FIR: ring buffer (W) + head + count = W + 2
    case OpKind::FIR:
      return node.window_size + 2;

    // IIR: x_head + x_count + y_head + y_count + x_ring(B) + y_ring(A) = 4 + B + A
    case OpKind::IIR:
      return 4 + node.b_len + node.a_len;

    // WinMin/WinMax: pos + size + W deque values + W deque positions = 2 + 2*W
    case OpKind::WinMin:
    case OpKind::WinMax:
      return 2 + 2 * node.window_size;

    // SignChange: prev_v + has_prev = 2
    case OpKind::SignChange:
      return 2;

    // PeakDetector: ring values (W) + ring timestamps (W) + count = 2*W + 1
    case OpKind::PeakDetector:
      return 2 * node.window_size + 1;

    // ResamplerHermite: 4 ring values + 4 ring timestamps + next_emit +
    // initialized + count = 11
    case OpKind::ResamplerHermite:
      return 11;

    // ResamplerConstant: last_value + next_emit + initialized = 3
    case OpKind::ResamplerConstant:
      return 3;

    // Join with N ports: N * (2 * kJoinPortCapacity + 2)
    // join_num_ports is set by the parser; fall back to port_types.size()
    // for legacy graphs and 2 if neither is provided.
    case OpKind::Join: {
      std::size_t n_ports = node.join_num_ports;
      if (n_ports == 0) {
        n_ports = node.port_types.empty() ? 2 : node.port_types.size();
      }
      return n_ports * (2 * rtbot::compiled::kJoinPortCapacity + 2);
    }

    // Linear and ReduceJoin: same per-port ring buffer machinery as Join.
    case OpKind::Linear:
    case OpKind::ReduceJoin: {
      std::size_t n_ports = node.join_num_ports;
      if (n_ports == 0 && node.kind == OpKind::Linear) {
        n_ports = node.coefficients.size();
      }
      if (n_ports < 2) n_ports = 2;
      return n_ports * (2 * rtbot::compiled::kJoinPortCapacity + 2);
    }

    // VectorCompose: same per-port ring buffer machinery as Join.
    case OpKind::VectorCompose: {
      std::size_t n_ports = node.join_num_ports;
      if (n_ports == 0) n_ports = 1;
      return n_ports * (2 * rtbot::compiled::kJoinPortCapacity + 2);
    }

    // FusedExpression: N port queues (Join-shape) + bytecode-derived state.
    // Total = N * port_capacity + bytecode_state_size.
    case OpKind::FusedExpression: {
      std::size_t n_ports = node.fe_num_ports;
      if (n_ports == 0) n_ports = 1;
      const std::size_t queue_size =
          n_ports * (2 * rtbot::compiled::kJoinPortCapacity + 2);
      auto pack = rtbot::fuse::pack_bytecode(node.fe_bytecode);
      const auto fl = rtbot::fuse::compute_state_layout(pack.packed,
                                                        pack.aux_args);
      return queue_size + fl.total_state_size;
    }

    // FusedExpressionVector: no port queues (single vector input wire);
    // state size is just the bytecode-derived layout.
    case OpKind::FusedExpressionVector: {
      auto pack = rtbot::fuse::pack_bytecode(node.fe_bytecode);
      const auto fl = rtbot::fuse::compute_state_layout(pack.packed,
                                                        pack.aux_args);
      return fl.total_state_size;
    }

    // BurstAggregate state layout:
    //   [agg_state ...]                 size from compute_state_layout
    //   has_seg_value                   1 (i1 stored as double 0/1)
    //   last_seg_value                  1
    //   last_key_values[K]              K (one slot per key column)
    //   has_valid_output                1
    //   last_valid_output[num_agg]      num_agg_outputs
    case OpKind::BurstAggregate: {
      auto pack = rtbot::fuse::pack_bytecode(node.ba_agg_bytecode);
      const auto fl = rtbot::fuse::compute_state_layout(pack.packed,
                                                        pack.aux_args);
      return fl.total_state_size + 1 + 1 + node.ba_key_columns.size() + 1 +
             node.ba_num_agg_outputs;
    }

    // TopK: 1 count slot followed by K rows of row_width doubles each.
    // count is stored as a double in [0, K]; rows[j*W + k] holds lane k of
    // the j-th sorted row (best at j=0, worst at j=count-1).
    case OpKind::TopK: {
      const std::size_t K = node.topk_k;
      const std::size_t W = (node.topk_row_width == 0) ? 1 : node.topk_row_width;
      return 1 + K * W;
    }

    // Pipeline state layout (planned by plan_state_layout, which fills
    // pipeline_inner_state_size before this is consulted):
    //   port queues:           N * (2 * kJoinPortCapacity + 2)
    //   last_segment_key:      1 (init NaN — D3 forces first-tick reset)
    //   buffered_msg_present:  1 (init 0.0 = false)
    //   buffered_t:            1
    //   buffered_v[numOutputs]:numOutputs
    //   inner_state:           pipeline_inner_state_size
    case OpKind::Pipeline: {
      // For control-port mode: n_ports is the data input port count and a
      // single control queue is appended.
      // For segment-bytecode mode: n_ports is the lane count of the upstream
      // vector wire feeding data port 0 (the parser sets join_num_ports to
      // pipeline_input_lane_width). One port queue per lane; no control.
      std::size_t n_ports = node.join_num_ports;
      if (n_ports == 0) n_ports = node.pipeline_input_port_types.size();
      if (n_ports == 0) n_ports = 1;
      const bool has_control = node.pipeline_segment_bytecode.empty();
      const std::size_t total_ports = n_ports + (has_control ? 1 : 0);
      const std::size_t queue_size =
          total_ports * (2 * rtbot::compiled::kJoinPortCapacity + 2);
      const std::size_t num_outputs = node.pipeline_output_port_types.size();
      return queue_size + 3 + num_outputs + node.pipeline_inner_state_size;
    }

    // MovingKeyCount state layout (ring + accounting + runtime ctx pointer):
    //   ring[W]:    last-seen keys
    //   ring_count: number of valid entries (max = W)
    //   ring_head:  next insert index (0..W-1)
    //   ctx_ptr:    bit-cast pointer to per-instance MovingKeyCountNodeCtx
    // Per-key counts live OUTSIDE the static state buffer — the ctx owns a
    // map<double, size_t> that the runtime helper updates.
    case OpKind::MovingKeyCount:
      return node.mkc_window_size + 3;

    // KeyedPipeline state layout:
    //   port queues: lane_width queues (one per lane, segment-bytecode shape)
    //   ctx_ptr_slot: 1 double holding the per-program KeyedPipelineNodeCtx
    //                 pointer (bit-cast to double; patched at construction).
    // Per-key state lives OUTSIDE the static state buffer — the ctx owns a
    // map<double, vector<double>> that the runtime helper grows lazily.
    case OpKind::KeyedPipeline: {
      std::size_t n_ports = node.join_num_ports;
      if (n_ports == 0) n_ports = node.keyed_pipeline_input_lane_width;
      if (n_ports == 0) n_ports = 1;
      const std::size_t queue_size =
          n_ports * (2 * rtbot::compiled::kJoinPortCapacity + 2);
      return queue_size + 1;
    }
  }

  // Unreachable — all enum values are handled above.
  return 0;
}

StateLayout plan_state_layout(CompiledGraph& graph) {
  StateLayout layout;
  std::size_t cursor = 0;

  for (auto& node : graph.nodes) {
    // Recursively plan the inner program for Pipeline nodes BEFORE sizing
    // the outer slot — state_size_for(Pipeline) reads pipeline_inner_state_size.
    if (node.kind == OpKind::Pipeline && node.pipeline_inner_graph) {
      StateLayout inner = plan_state_layout(*node.pipeline_inner_graph);
      node.pipeline_inner_state_size = inner.total_size;
      node.pipeline_inner_state_init.assign(inner.total_size, 0.0);
      for (const auto& [slot, value] : inner.state_init_overrides) {
        if (slot < node.pipeline_inner_state_init.size()) {
          node.pipeline_inner_state_init[slot] = value;
        }
      }
    }
    // KeyedPipeline mirrors Pipeline's inner-state-init pattern. The init
    // pattern lives ON the OpNode (not in the program's static state buffer);
    // the runtime helper consumes it when allocating a per-key buffer.
    if (node.kind == OpKind::KeyedPipeline && node.keyed_pipeline_inner_graph) {
      StateLayout inner = plan_state_layout(*node.keyed_pipeline_inner_graph);
      node.keyed_pipeline_inner_state_size = inner.total_size;
      node.keyed_pipeline_inner_state_init.assign(inner.total_size, 0.0);
      for (const auto& [slot, value] : inner.state_init_overrides) {
        if (slot < node.keyed_pipeline_inner_state_init.size()) {
          node.keyed_pipeline_inner_state_init[slot] = value;
        }
      }
    }
    const std::size_t off = cursor;
    layout.offsets[node.id] = off;
    cursor += state_size_for(node);

    // MaxAgg slot must start at -inf; MinAgg slot must start at +inf.
    if (node.kind == OpKind::MaxAgg) {
      layout.state_init_overrides[off] =
          -std::numeric_limits<double>::infinity();
    } else if (node.kind == OpKind::MinAgg) {
      layout.state_init_overrides[off] =
          std::numeric_limits<double>::infinity();
    } else if (node.kind == OpKind::FusedExpression) {
      // FE bytecode state lives after the port queues. Overlay non-default
      // (non-zero) initial values produced by FusedStateLayout (e.g. MAX_AGG
      // = -inf, MIN_AGG = +inf), then apply user-supplied stateInit on top.
      std::size_t n_ports = node.fe_num_ports;
      if (n_ports == 0) n_ports = 1;
      const std::size_t queue_size =
          n_ports * (2 * rtbot::compiled::kJoinPortCapacity + 2);
      const std::size_t bc_base = off + queue_size;
      auto pack = rtbot::fuse::pack_bytecode(node.fe_bytecode);
      const auto fl = rtbot::fuse::compute_state_layout(pack.packed,
                                                        pack.aux_args);
      for (std::size_t k = 0; k < fl.initial_values.size(); ++k) {
        const double v = fl.initial_values[k];
        if (v != 0.0) {
          layout.state_init_overrides[bc_base + k] = v;
        }
      }
      // pack_bytecode also produces a state_init shape that mirrors compute_
      // state_layout. User-supplied overrides ride on top.
      for (std::size_t k = 0; k < pack.state_init.size(); ++k) {
        const double v = pack.state_init[k];
        if (v != 0.0) {
          layout.state_init_overrides[bc_base + k] = v;
        }
      }
      for (std::size_t k = 0; k < node.fe_state_init.size(); ++k) {
        layout.state_init_overrides[bc_base + k] = node.fe_state_init[k];
      }
    } else if (node.kind == OpKind::BurstAggregate) {
      // Agg-state slots come first; seed any non-zero initial values from
      // FE-style state layout (e.g. MAX_AGG = -inf, MIN_AGG = +inf) and from
      // pack.state_init. The remaining housekeeping slots default to 0.
      const std::size_t bc_base = off;
      auto pack = rtbot::fuse::pack_bytecode(node.ba_agg_bytecode);
      const auto fl = rtbot::fuse::compute_state_layout(pack.packed,
                                                        pack.aux_args);
      for (std::size_t k = 0; k < fl.initial_values.size(); ++k) {
        const double v = fl.initial_values[k];
        if (v != 0.0) {
          layout.state_init_overrides[bc_base + k] = v;
        }
      }
      for (std::size_t k = 0; k < pack.state_init.size(); ++k) {
        const double v = pack.state_init[k];
        if (v != 0.0) {
          layout.state_init_overrides[bc_base + k] = v;
        }
      }
    } else if (node.kind == OpKind::FusedExpressionVector) {
      // FEV has no port queues — bytecode state begins at the node's offset.
      const std::size_t bc_base = off;
      auto pack = rtbot::fuse::pack_bytecode(node.fe_bytecode);
      const auto fl = rtbot::fuse::compute_state_layout(pack.packed,
                                                        pack.aux_args);
      for (std::size_t k = 0; k < fl.initial_values.size(); ++k) {
        const double v = fl.initial_values[k];
        if (v != 0.0) {
          layout.state_init_overrides[bc_base + k] = v;
        }
      }
      for (std::size_t k = 0; k < pack.state_init.size(); ++k) {
        const double v = pack.state_init[k];
        if (v != 0.0) {
          layout.state_init_overrides[bc_base + k] = v;
        }
      }
      for (std::size_t k = 0; k < node.fe_state_init.size(); ++k) {
        layout.state_init_overrides[bc_base + k] = node.fe_state_init[k];
      }
    } else if (node.kind == OpKind::Pipeline) {
      // Pipeline outer slot layout (matches state_size_for):
      //   [data port queues ...][control port queue?][last_segment_key]
      //   [buffered_msg_present][buffered_t][buffered_v[numOutputs]]
      //   [inner_state ...]
      //
      // last_segment_key is seeded with NaN so the first incoming tick is
      // always treated as a fresh segment by D3's reset logic. The inner
      // state slots inherit any overrides recorded by the recursive
      // plan_state_layout call above. n_ports is the lane count for
      // segment-bytecode mode (one queue per lane), or the data port count
      // for control-port mode (plus 1 trailing control queue).
      std::size_t n_ports = node.join_num_ports;
      if (n_ports == 0) n_ports = node.pipeline_input_port_types.size();
      if (n_ports == 0) n_ports = 1;
      const bool has_control = node.pipeline_segment_bytecode.empty();
      const std::size_t total_ports = n_ports + (has_control ? 1 : 0);
      const std::size_t queue_size =
          total_ports * (2 * rtbot::compiled::kJoinPortCapacity + 2);
      const std::size_t key_off = off + queue_size;
      layout.state_init_overrides[key_off] =
          std::numeric_limits<double>::quiet_NaN();
      const std::size_t num_outputs = node.pipeline_output_port_types.size();
      const std::size_t inner_base = key_off + 3 + num_outputs;
      for (std::size_t k = 0; k < node.pipeline_inner_state_init.size(); ++k) {
        const double v = node.pipeline_inner_state_init[k];
        if (v != 0.0) {
          layout.state_init_overrides[inner_base + k] = v;
        }
      }
    }
  }

  layout.total_size = cursor;
  return layout;
}

std::size_t compute_program_outputs(const CompiledGraph& graph) {
  for (const auto& node : graph.nodes) {
    if (node.kind != OpKind::Output) continue;

    std::size_t port_count = 0;
    auto it = graph.outputs.find(node.id);
    if (it != graph.outputs.end() && !it->second.empty()) {
      port_count = it->second.size();
    } else {
      port_count = node.port_types.size();
    }

    std::size_t total = 0;
    for (std::size_t p = 0; p < port_count; ++p) {
      total += node.output_port_width(p);
    }
    return total;
  }
  return 0;
}

std::size_t compute_max_emits_per_call(const CompiledGraph& graph) {
  std::size_t k = 1;
  for (const auto& node : graph.nodes) {
    if (node.kind == OpKind::Demux) {
      std::size_t n = node.mux_num_ports;
      if (n == 0) n = 1;
      if (n > k) k = n;
    }
    if (node.kind == OpKind::TopK) {
      const std::size_t n = node.topk_k;
      if (n > k) k = n;
    }
    if (node.kind == OpKind::Pipeline) {
      const std::size_t n = node.pipeline_output_port_types.size();
      if (n > k) k = n;
    }
    if (node.kind == OpKind::KeyedPipeline) {
      // KeyedPipeline emits one combined record per inner emission. The
      // inner sub-function's K bounds the per-tick emission count. We also
      // recurse to cover nested Pipelines / Demuxes inside the prototype.
      // In multi-view session programs the KP also fans out to downstream
      // FEV chains (one additional record per chain per inner emission), so
      // multiply inner_k by the number of downstream output connections.
      if (node.keyed_pipeline_inner_graph) {
        const std::size_t inner_k =
            compute_max_emits_per_call(*node.keyed_pipeline_inner_graph);
        // Count downstream output connections from this KP node.
        std::size_t fanout = 0;
        for (const auto& conn : graph.connections) {
          if (conn.from_id == node.id && conn.from_port == 0) ++fanout;
        }
        if (fanout == 0) fanout = 1;
        const std::size_t kp_max = inner_k * fanout;
        if (kp_max > k) k = kp_max;
      }
    }
  }
  return k;
}

}  // namespace rtbot::jit
