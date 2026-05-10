// SegmentEmitter.cpp
//
// Orchestrates JIT IR generation for one linear segment defined in a
// CompiledGraph. Generalises the spike's hand-rolled emit_bollinger_process
// to work for any sequence of ops.
//
// Function signature (extern "C"):
//   bool segment_process(double* state, int64_t t, double v,
//                        int64_t* out_t, double* out_v_array);
//
// Returns true when a tick is emitted (out_t and out_v_array[0..num_outputs-1]
// are written); false otherwise.
//
// Multi-emit (Resampler) handling:
//   The Resampler is detected as a first-class case. When present, ops
//   downstream of it are emitted inside the Resampler's callback body,
//   running once per interpolated sample. The last downstream emission's
//   outputs are retained via allocas for the function's return value.
//
//   The callback always exits with the IRBuilder positioned in a dedicated
//   `chain_exit` block. Warmup-skip paths branch directly to `chain_exit`
//   (bypassing Output writes). After the Resampler loop exits, the function
//   checks al_did_emit and branches to ret_true or ret_false.
//
// Stateful guard handling (linear path):
//   Each stateful op (MA, StdDev) returns an emit_flag. If false (warmup),
//   control flow branches directly to ret_false.
//
// Multi-segment (Join) handling (emit_program):
//   For graphs containing Join sync ops. Each op runs in topological order.
//   Stateful ops run unconditionally (state must update); their downstream
//   sub-paths are gated on emit_flag. Join try_sync runs unconditionally;
//   downstream of each Join is gated on sync_flag. The function returns true
//   only when the final Join (connected to Output) syncs and outputs are written.

#include "rtbot/compiled/jit/SegmentEmitter.h"

#include <algorithm>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include "rtbot/Pipeline.h"
#include "rtbot/compiled/JoinStage.h"
#include "rtbot/compiled/jit/JitCompiledProgram.h"
#include <cstdint>
#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/SegmentPartitioner.h"
#include "rtbot/compiled/jit/emit/Aggregate.h"
#include "rtbot/compiled/jit/emit/Arithmetic.h"
#include "rtbot/compiled/jit/emit/Boolean.h"
#include "rtbot/compiled/jit/emit/BooleanToNumber.h"
#include "rtbot/compiled/jit/emit/Comparison.h"
#include "rtbot/compiled/jit/emit/Constant.h"
#include "rtbot/compiled/jit/emit/Diff.h"
#include "rtbot/compiled/jit/emit/FIR.h"
#include "rtbot/compiled/jit/emit/Filter.h"
#include "rtbot/compiled/jit/emit/Function.h"
#include "rtbot/compiled/jit/emit/FusedExpression.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"
#include "rtbot/fuse/FusedStateLayout.h"
#include "rtbot/compiled/jit/emit/Gate.h"
#include "rtbot/compiled/jit/emit/IIR.h"
#include "rtbot/compiled/jit/emit/Demux.h"
#include "rtbot/compiled/jit/emit/Identity.h"
#include "rtbot/compiled/jit/emit/Join.h"
#include "rtbot/compiled/jit/emit/Linear.h"
#include "rtbot/compiled/jit/emit/Mux.h"
#include "rtbot/compiled/jit/emit/ReduceJoin.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"
#include "rtbot/compiled/jit/emit/MovingKeyCount.h"
#include "rtbot/compiled/jit/emit/MovingSum.h"
#include "rtbot/compiled/jit/emit/PeakDetector.h"
#include "rtbot/compiled/jit/emit/Replace.h"
#include "rtbot/compiled/jit/emit/Resampler.h"
#include "rtbot/compiled/jit/emit/ResamplerConstant.h"
#include "rtbot/compiled/jit/emit/SignChange.h"
#include "rtbot/compiled/jit/emit/StdDev.h"
#include "rtbot/compiled/jit/emit/StateLoad.h"
#include "rtbot/compiled/jit/emit/TimeShift.h"
#include "rtbot/compiled/jit/emit/TimestampExtract.h"
#include "rtbot/compiled/jit/emit/TopK.h"
#include "rtbot/compiled/jit/emit/Transcendental.h"
#include "rtbot/compiled/jit/emit/WindowMinMax.h"

namespace rtbot::jit {

namespace {

// Value map key: (op_id, output_port_index).
struct PortKey {
  std::string op_id;
  std::size_t port;
  bool operator<(const PortKey& o) const {
    if (op_id != o.op_id) return op_id < o.op_id;
    return port < o.port;
  }
};

// Each port's value: (t: i64, v).
// When `width == 1`, `v` is a scalar `double` SSA value (the historical
// shape). When `width > 1`, `v` is an `[width x double]` SSA vector
// produced by VectorCompose / VectorProject and consumed by VectorExtract /
// VectorProject / Output.
struct PortValue {
  llvm::Value* t{nullptr};
  llvm::Value* v{nullptr};
  std::size_t  width{1};
};

using ValueMap = std::map<PortKey, PortValue>;

// Collect connections into op `to_id` matching the requested kind, sorted
// ascending by to_port.
std::vector<const Connection*> inputs_for_kind(const CompiledGraph& graph,
                                               const std::string& to_id,
                                               PortKind kind) {
  std::vector<const Connection*> result;
  for (const auto& c : graph.connections) {
    if (c.to_id == to_id && c.to_kind == kind) result.push_back(&c);
  }
  std::sort(result.begin(), result.end(),
            [](const Connection* a, const Connection* b) {
              return a->to_port < b->to_port;
            });
  return result;
}

// Data-port inputs only. Existing emitter logic relies on this default.
std::vector<const Connection*> inputs_for(const CompiledGraph& graph,
                                           const std::string& to_id) {
  return inputs_for_kind(graph, to_id, PortKind::Data);
}

// Convenience aliases for explicit data-or-control intent at call sites.
std::vector<const Connection*> data_inputs_for(const CompiledGraph& graph,
                                                const std::string& to_id) {
  return inputs_for_kind(graph, to_id, PortKind::Data);
}

std::vector<const Connection*> control_inputs_for(const CompiledGraph& graph,
                                                   const std::string& to_id) {
  return inputs_for_kind(graph, to_id, PortKind::Control);
}

// Look up an op node by id. Throws if not found.
const OpNode& find_node(const CompiledGraph& graph, const std::string& id) {
  for (const auto& n : graph.nodes) {
    if (n.id == id) return n;
  }
  throw std::runtime_error("SegmentEmitter: node not found: " + id);
}

// Determine num_outputs for the Output op. Equal to the sum of its per-port
// widths; for the all-width-1 baseline this matches the collected port count.
std::size_t compute_num_outputs(const CompiledGraph& graph,
                                 const std::string& /*output_op_id*/) {
  return compute_program_outputs(graph);
}

// Emit IR for a single stateless op. Dispatches on node.kind and stores the
// result in value_map at (node.id, 0). Input connections are resolved from
// the value_map. Throws for stateful/special ops (handled by callers).
void emit_stateless_op(IrEmissionContext& ec,
                       const CompiledGraph& graph,
                       const OpNode& node,
                       ValueMap& value_map) {
  auto get = [&](const std::string& from_id, std::size_t from_port) -> PortValue {
    auto it = value_map.find({from_id, from_port});
    if (it == value_map.end()) {
      throw std::runtime_error("SegmentEmitter: missing value for " + from_id +
                               " port " + std::to_string(from_port));
    }
    return it->second;
  };

  auto in_conns = inputs_for(graph, node.id);

  auto store1 = [&](llvm::Value* out_t, llvm::Value* out_v) {
    value_map[{node.id, 0}] = {out_t, out_v};
  };

  auto get1 = [&]() -> PortValue {
    if (in_conns.empty()) {
      throw std::runtime_error("SegmentEmitter: op " + node.id + " needs 1 input");
    }
    return get(in_conns[0]->from_id, in_conns[0]->from_port);
  };

  auto get2 = [&]() -> std::pair<PortValue, PortValue> {
    if (in_conns.size() < 2) {
      throw std::runtime_error("SegmentEmitter: op " + node.id + " needs 2 inputs");
    }
    return {get(in_conns[0]->from_id, in_conns[0]->from_port),
            get(in_conns[1]->from_id, in_conns[1]->from_port)};
  };

  switch (node.kind) {
    case OpKind::Add: { auto [a, b] = get2(); store1(a.t, emit::emit_add(ec, a.v, b.v)); break; }
    case OpKind::Sub: { auto [a, b] = get2(); store1(a.t, emit::emit_sub(ec, a.v, b.v)); break; }
    case OpKind::Mul: { auto [a, b] = get2(); store1(a.t, emit::emit_mul(ec, a.v, b.v)); break; }
    case OpKind::Div: { auto [a, b] = get2(); store1(a.t, emit::emit_div(ec, a.v, b.v)); break; }
    case OpKind::Scale: { auto a = get1(); store1(a.t, emit::emit_scale(ec, a.v, node.scale_constant)); break; }
    case OpKind::AddScalar:   { auto a = get1(); store1(a.t, emit::emit_add_scalar(ec, a.v, node.scalar_value)); break; }
    case OpKind::PowerScalar: { auto a = get1(); store1(a.t, emit::emit_power_scalar(ec, a.v, node.scalar_value)); break; }
    case OpKind::Pow:   { auto [a, b] = get2(); store1(a.t, emit::emit_pow(ec, a.v, b.v)); break; }
    case OpKind::Abs:   { auto a = get1(); store1(a.t, emit::emit_abs(ec, a.v));   break; }
    case OpKind::Sqrt:  { auto a = get1(); store1(a.t, emit::emit_sqrt(ec, a.v));  break; }
    case OpKind::Log:   { auto a = get1(); store1(a.t, emit::emit_log(ec, a.v));   break; }
    case OpKind::Log10: { auto a = get1(); store1(a.t, emit::emit_log10(ec, a.v)); break; }
    case OpKind::Exp:   { auto a = get1(); store1(a.t, emit::emit_exp(ec, a.v));   break; }
    case OpKind::Sin:   { auto a = get1(); store1(a.t, emit::emit_sin(ec, a.v));   break; }
    case OpKind::Cos:   { auto a = get1(); store1(a.t, emit::emit_cos(ec, a.v));   break; }
    case OpKind::Tan:   { auto a = get1(); store1(a.t, emit::emit_tan(ec, a.v));   break; }
    case OpKind::Sign:  { auto a = get1(); store1(a.t, emit::emit_sign(ec, a.v));  break; }
    case OpKind::Floor: { auto a = get1(); store1(a.t, emit::emit_floor(ec, a.v)); break; }
    case OpKind::Ceil:  { auto a = get1(); store1(a.t, emit::emit_ceil(ec, a.v));  break; }
    case OpKind::Round: { auto a = get1(); store1(a.t, emit::emit_round(ec, a.v)); break; }
    case OpKind::Neg:   { auto a = get1(); store1(a.t, emit::emit_neg(ec, a.v));   break; }
    case OpKind::Gt:    { auto [a, b] = get2(); store1(a.t, emit::emit_gt(ec, a.v, b.v));  break; }
    case OpKind::Gte:   { auto [a, b] = get2(); store1(a.t, emit::emit_gte(ec, a.v, b.v)); break; }
    case OpKind::Lt:    { auto [a, b] = get2(); store1(a.t, emit::emit_lt(ec, a.v, b.v));  break; }
    case OpKind::Lte:   { auto [a, b] = get2(); store1(a.t, emit::emit_lte(ec, a.v, b.v)); break; }
    case OpKind::Eq:    { auto [a, b] = get2(); store1(a.t, emit::emit_eq(ec, a.v, b.v));  break; }
    case OpKind::Neq:   { auto [a, b] = get2(); store1(a.t, emit::emit_neq(ec, a.v, b.v)); break; }
    case OpKind::EqTol: { auto [a, b] = get2(); store1(a.t, emit::emit_eq_tol(ec, a.v, b.v, node.tolerance));  break; }
    case OpKind::NeqTol:{ auto [a, b] = get2(); store1(a.t, emit::emit_neq_tol(ec, a.v, b.v, node.tolerance)); break; }
    case OpKind::GtScalar:  { auto a = get1(); store1(a.t, emit::emit_gt(ec, a.v, llvm::ConstantFP::get(ec.b().getDoubleTy(), node.scalar_value)));  break; }
    case OpKind::LtScalar:  { auto a = get1(); store1(a.t, emit::emit_lt(ec, a.v, llvm::ConstantFP::get(ec.b().getDoubleTy(), node.scalar_value)));  break; }
    case OpKind::GteScalar: { auto a = get1(); store1(a.t, emit::emit_gte(ec, a.v, llvm::ConstantFP::get(ec.b().getDoubleTy(), node.scalar_value))); break; }
    case OpKind::LteScalar: { auto a = get1(); store1(a.t, emit::emit_lte(ec, a.v, llvm::ConstantFP::get(ec.b().getDoubleTy(), node.scalar_value))); break; }
    case OpKind::EqScalar:  { auto a = get1(); store1(a.t, emit::emit_eq_tol(ec, a.v, llvm::ConstantFP::get(ec.b().getDoubleTy(), node.scalar_value), node.tolerance));  break; }
    case OpKind::NeqScalar: { auto a = get1(); store1(a.t, emit::emit_neq_tol(ec, a.v, llvm::ConstantFP::get(ec.b().getDoubleTy(), node.scalar_value), node.tolerance)); break; }
    case OpKind::And:   { auto [a, b] = get2(); store1(a.t, emit::emit_and(ec, a.v, b.v)); break; }
    case OpKind::Or:    { auto [a, b] = get2(); store1(a.t, emit::emit_or(ec, a.v, b.v));  break; }
    case OpKind::Not:   { auto a = get1(); store1(a.t, emit::emit_not(ec, a.v)); break; }
    case OpKind::Xor:        { auto [a, b] = get2(); store1(a.t, emit::emit_xor(ec, a.v, b.v));         break; }
    case OpKind::Nand:       { auto [a, b] = get2(); store1(a.t, emit::emit_nand(ec, a.v, b.v));        break; }
    case OpKind::Nor:        { auto [a, b] = get2(); store1(a.t, emit::emit_nor(ec, a.v, b.v));         break; }
    case OpKind::Xnor:       { auto [a, b] = get2(); store1(a.t, emit::emit_xnor(ec, a.v, b.v));        break; }
    case OpKind::Implication:{ auto [a, b] = get2(); store1(a.t, emit::emit_implication(ec, a.v, b.v)); break; }
    case OpKind::Identity:
    case OpKind::BooleanToNumber: { auto a = get1(); store1(a.t, a.v); break; }
    case OpKind::Constant: {
      auto a = get1();
      store1(a.t, emit::emit_constant(ec, node.constant_value));
      break;
    }
    case OpKind::TimestampExtract: {
      auto a = get1();
      store1(a.t, emit::emit_timestamp_extract(ec, a.t));
      break;
    }
    case OpKind::Function: {
      auto a = get1();
      store1(a.t, emit::emit_function(ec, node.function_points,
                                       node.function_use_hermite,
                                       node.function_tangents, a.v));
      break;
    }
    case OpKind::VectorExtract: {
      auto a = get1();
      if (a.width <= 1 || !a.v->getType()->isVectorTy()) {
        throw std::runtime_error(
            "SegmentEmitter: VectorExtract '" + node.id +
            "' input is not a vector wire");
      }
      if (node.vector_index >= a.width) {
        throw std::runtime_error(
            "SegmentEmitter: VectorExtract '" + node.id + "' index " +
            std::to_string(node.vector_index) +
            " out of bounds for vector of width " + std::to_string(a.width));
      }
      llvm::Value* idx = llvm::ConstantInt::get(
          llvm::Type::getInt32Ty(ec.ctx()),
          static_cast<std::uint32_t>(node.vector_index));
      llvm::Value* scalar = ec.b().CreateExtractElement(a.v, idx, "vex");
      value_map[{node.id, 0}] = {a.t, scalar, /*width=*/1};
      break;
    }
    case OpKind::VectorProject: {
      auto a = get1();
      if (a.width <= 1 || !a.v->getType()->isVectorTy()) {
        throw std::runtime_error(
            "SegmentEmitter: VectorProject '" + node.id +
            "' input is not a vector wire");
      }
      const std::size_t M = node.vector_indices.size();
      if (M == 0) {
        throw std::runtime_error(
            "SegmentEmitter: VectorProject '" + node.id +
            "' has no indices");
      }
      llvm::Type* dty = llvm::Type::getDoubleTy(ec.ctx());
      llvm::Type* vty = llvm::VectorType::get(
          dty, llvm::ElementCount::getFixed(static_cast<unsigned>(M)));
      llvm::Value* out_vec = llvm::UndefValue::get(vty);
      auto& bldr = ec.b();
      llvm::Type* i32 = llvm::Type::getInt32Ty(ec.ctx());
      for (std::size_t k = 0; k < M; ++k) {
        const std::size_t idx = node.vector_indices[k];
        if (idx >= a.width) {
          throw std::runtime_error(
              "SegmentEmitter: VectorProject '" + node.id + "' index " +
              std::to_string(idx) +
              " out of bounds for vector of width " + std::to_string(a.width));
        }
        llvm::Value* el = bldr.CreateExtractElement(
            a.v,
            llvm::ConstantInt::get(i32, static_cast<std::uint32_t>(idx)),
            "vp_el");
        out_vec = bldr.CreateInsertElement(
            out_vec, el,
            llvm::ConstantInt::get(i32, static_cast<std::uint32_t>(k)),
            "vp_ins");
      }
      value_map[{node.id, 0}] = {a.t, out_vec, /*width=*/M};
      break;
    }
    default:
      throw std::runtime_error(
          "SegmentEmitter: emit_stateless_op called on non-stateless op kind for " +
          node.id);
  }
}

// Compute the flat slot base in out_v for a given Output input port. Sums
// the widths of all preceding Output ports.
std::size_t output_flat_slot_base(const OpNode& output_node,
                                   std::size_t to_port) {
  std::size_t base = 0;
  for (std::size_t p = 0; p < to_port; ++p) {
    base += output_node.output_port_width(p);
  }
  return base;
}

// Write a single connection's PortValue into one or more consecutive slots
// of `slot_writer`. `slot_writer(k, v)` writes scalar `v` (a double SSA) to
// flat slot k. Handles both scalar and vector PortValues.
template <typename SlotWriter>
void write_port_value_to_slots(IrEmissionContext& ec,
                                const OpNode& output_node,
                                std::size_t to_port,
                                const PortValue& pv,
                                SlotWriter slot_writer) {
  const std::size_t slot_base = output_flat_slot_base(output_node, to_port);
  const std::size_t out_w     = output_node.output_port_width(to_port);
  if (pv.width <= 1) {
    if (out_w >= 1) slot_writer(slot_base, pv.v);
    return;
  }
  // Vector wire: the upstream produces an `[N x double]`. Extract each lane
  // and write to consecutive output slots. The Output's port width must
  // match the upstream vector width.
  if (pv.width != out_w) {
    throw std::runtime_error(
        "SegmentEmitter: vector wire width " + std::to_string(pv.width) +
        " does not match Output port width " + std::to_string(out_w));
  }
  llvm::Type* i32 = llvm::Type::getInt32Ty(ec.ctx());
  for (std::size_t k = 0; k < pv.width; ++k) {
    llvm::Value* el = ec.b().CreateExtractElement(
        pv.v, llvm::ConstantInt::get(i32, static_cast<std::uint32_t>(k)),
        "out_lane");
    slot_writer(slot_base + k, el);
  }
}

// Write the Output op's inputs to out_t_alloca and out_v_allocas, then
// set al_did_emit = 1 and branch to chain_exit_bb.
void emit_output_writes(IrEmissionContext& ec,
                         const CompiledGraph& graph,
                         const std::string& output_op_id,
                         std::size_t num_outputs,
                         const ValueMap& value_map,
                         llvm::Value* al_out_t,
                         const std::vector<llvm::Value*>& al_out_v,
                         llvm::Value* al_did_emit,
                         llvm::BasicBlock* chain_exit_bb) {
  auto& b  = ec.b();
  auto& ctx = ec.ctx();
  llvm::Type* i8 = llvm::Type::getInt8Ty(ctx);

  auto in_conns = inputs_for(graph, output_op_id);
  const OpNode& output_node = find_node(graph, output_op_id);

  // Write out_t from the timestamp of the first connected input.
  if (!in_conns.empty()) {
    auto it = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
    if (it != value_map.end()) {
      b.CreateStore(it->second.t, al_out_t);
    }
  }

  // Write each output value to its port slot(s). Vector wires fan out across
  // consecutive slots starting at the port's flat-slot base.
  for (const auto* conn : in_conns) {
    auto it = value_map.find({conn->from_id, conn->from_port});
    if (it == value_map.end()) continue;
    write_port_value_to_slots(
        ec, output_node, conn->to_port, it->second,
        [&](std::size_t slot, llvm::Value* val) {
          if (slot < num_outputs) {
            b.CreateStore(val, al_out_v[slot]);
          }
        });
  }

  b.CreateStore(llvm::ConstantInt::get(i8, 1), al_did_emit);
  b.CreateBr(chain_exit_bb);
}

// Emit IR for the post-Resampler chain (ops after the Resampler through Output).
// Called as the Resampler's emit callback. The IRBuilder is positioned in the
// Resampler's loop body when this is called.
//
// Optimised single-guard strategy matching the spike's control-flow shape:
//
//   Update phase (straight-line for each stateful op):
//     For MA/StdDev: call emit_*_update() which runs the Kahan ring update and
//     returns an i1 emit_flag WITHOUT creating a PHI merge block.  Stateless
//     ops and Output are skipped here.
//
//   Combined warmup guard:
//     combined_flag = AND of all per-op emit_flags.  A single CondBr jumps to
//     chain_emit (all warmed) or chain_exit_bb (any still warming).  No PHI
//     nodes are created for the output values.
//
//   Emit block (chain_emit):
//     Compute each stateful op's output value directly (emit_moving_average_output,
//     emit_stddev_output) and store in value_map.  Then emit stateless ops and
//     Output writes.  All control flow leads to chain_exit_bb.
//
// After the callback returns, the IRBuilder is positioned in chain_exit_bb.
// The Resampler emitter adds `br(bb_loop_next)` to chain_exit_bb.
//
// should_emit_alloca: i1* alloca owned by the SegmentEmitter. Initialized to
// true before each Resampler callback invocation. Gate ops AND this with the
// predicate; Output checks it before writing.
void emit_post_resampler_chain(IrEmissionContext& ec,
                                const CompiledGraph& graph,
                                const StateLayout& layout,
                                const std::vector<std::string>& post_rs_ids,
                                const std::string& resampler_op_id,
                                const std::string& output_op_id,
                                std::size_t num_outputs,
                                llvm::Value* al_out_t,
                                const std::vector<llvm::Value*>& al_out_v,
                                llvm::Value* al_did_emit,
                                llvm::Value* al_should_emit,
                                llvm::Value* rs_rt,
                                llvm::Value* rs_rv) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Function* fn = b.GetInsertBlock()->getParent();

  // Re-initialize should_emit to true at the start of each callback invocation
  // (each interpolated Resampler sample is an independent filtering decision).
  b.CreateStore(llvm::ConstantInt::getTrue(ctx), al_should_emit);

  // chain_exit_bb: all paths lead here. The Resampler emitter adds
  // `br(bb_loop_next)` to it after this function returns.
  llvm::BasicBlock* chain_exit_bb =
      llvm::BasicBlock::Create(ctx, "chain_exit", fn);

  // Value map seeded with the Resampler's output.
  ValueMap value_map;
  value_map[{resampler_op_id, 0}] = {rs_rt, rs_rv};

  // -----------------------------------------------------------------------
  // Update phase: run all stateful ops. Each emitter returns an i1 emit_flag
  // with NO PHI merge block. combined_flag = AND of all flags.
  //
  // MA/StdDev use split _update / _output forms.
  // All other stateful ops use the full emit_* form: both update and output
  // are computed here and stored for reuse in the emit block.
  // -----------------------------------------------------------------------

  // Per-op update results stored for Phase 2 output computation.
  struct MAEntry { std::string op_id; emit::MAUpdateResult upd; };
  struct SDEntry { std::string op_id; emit::SDUpdateResult upd; std::size_t W; };
  // Full-form stateful outputs (MovingSum, Diff, SignChange, WinMin/Max, FIR, IIR).
  struct SOEntry { std::string op_id; emit::StatefulOutput so; };

  std::vector<MAEntry> ma_results;
  std::vector<SDEntry> sd_results;
  std::vector<SOEntry> so_results;

  llvm::Value* combined_flag = llvm::ConstantInt::getTrue(ctx);

  for (const auto& op_id : post_rs_ids) {
    const OpNode& node = find_node(graph, op_id);

    // Stateless, Gate, CumSum/Count/MaxAgg/MinAgg (always-emit), and Output
    // are handled in the emit block.
    bool is_windowed_stateful =
        (node.kind == OpKind::MovingAverage || node.kind == OpKind::StdDev  ||
         node.kind == OpKind::MovingSum      || node.kind == OpKind::Diff    ||
         node.kind == OpKind::SignChange     || node.kind == OpKind::WinMin  ||
         node.kind == OpKind::WinMax         || node.kind == OpKind::FIR     ||
         node.kind == OpKind::IIR            ||
         node.kind == OpKind::TimeShift      ||
         node.kind == OpKind::LessThanOrEqualToReplace ||
         node.kind == OpKind::MovingKeyCount ||
         node.kind == OpKind::FiltGtScalar   || node.kind == OpKind::FiltLtScalar ||
         node.kind == OpKind::FiltEqScalar   || node.kind == OpKind::FiltNeqScalar ||
         node.kind == OpKind::FiltGtSync     || node.kind == OpKind::FiltLtSync ||
         node.kind == OpKind::FiltEqSync     || node.kind == OpKind::FiltNeqSync);
    if (!is_windowed_stateful) continue;

    auto in_conns = inputs_for(graph, op_id);
    if (in_conns.empty()) {
      throw std::runtime_error("SegmentEmitter: stateful op " + op_id + " has no input");
    }
    auto inp = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
    if (inp == value_map.end()) {
      throw std::runtime_error("SegmentEmitter: missing input for " + op_id);
    }

    std::size_t offset = layout.offsets.at(op_id);

    if (node.kind == OpKind::MovingAverage) {
      auto upd = emit::emit_moving_average_update(ec, offset, node.window_size,
                                                  inp->second.v);
      combined_flag = b.CreateAnd(combined_flag, upd.emit_flag, "comb_flag");
      ma_results.push_back({op_id, upd});
    } else if (node.kind == OpKind::StdDev) {
      auto upd = emit::emit_stddev_update(ec, offset, node.window_size,
                                          inp->second.v);
      combined_flag = b.CreateAnd(combined_flag, upd.emit_flag, "comb_flag");
      sd_results.push_back({op_id, upd, node.window_size});
    } else {
      // Full-form emitters: update + output together.
      emit::StatefulOutput so{};
      switch (node.kind) {
        case OpKind::MovingSum:
          so = emit::emit_moving_sum(ec, offset, node.window_size,
                                     inp->second.t, inp->second.v);
          break;
        case OpKind::Diff:
          so = emit::emit_diff(ec, offset, /*use_oldest_time=*/true,
                               inp->second.t, inp->second.v);
          break;
        case OpKind::SignChange:
          so = emit::emit_sign_change(ec, offset, inp->second.t, inp->second.v);
          break;
        case OpKind::WinMin:
          so = emit::emit_win_min(ec, offset, node.window_size,
                                  inp->second.t, inp->second.v);
          break;
        case OpKind::WinMax:
          so = emit::emit_win_max(ec, offset, node.window_size,
                                  inp->second.t, inp->second.v);
          break;
        case OpKind::FIR:
          so = emit::emit_fir(ec, offset, node.window_size,
                              node.coefficients,
                              inp->second.t, inp->second.v);
          break;
        case OpKind::IIR:
          so = emit::emit_iir(ec, offset, node.b_len, node.a_len,
                              node.coefficients,
                              inp->second.t, inp->second.v);
          break;
        case OpKind::TimeShift:
          so = emit::emit_time_shift(ec, node.time_shift,
                                     inp->second.t, inp->second.v);
          break;
        case OpKind::LessThanOrEqualToReplace:
          so = emit::emit_lte_replace(ec, node.replace_threshold,
                                      node.replace_by,
                                      inp->second.t, inp->second.v);
          break;
        case OpKind::MovingKeyCount:
          so = emit::emit_moving_key_count(ec, offset, node.mkc_window_size,
                                           inp->second.t, inp->second.v);
          break;
        case OpKind::FiltGtScalar:
          so = emit::emit_filt_gt_scalar(ec, node.scalar_value,
                                         inp->second.t, inp->second.v);
          break;
        case OpKind::FiltLtScalar:
          so = emit::emit_filt_lt_scalar(ec, node.scalar_value,
                                         inp->second.t, inp->second.v);
          break;
        case OpKind::FiltEqScalar:
          so = emit::emit_filt_eq_scalar(ec, node.scalar_value, node.tolerance,
                                          inp->second.t, inp->second.v);
          break;
        case OpKind::FiltNeqScalar:
          so = emit::emit_filt_neq_scalar(ec, node.scalar_value, node.tolerance,
                                           inp->second.t, inp->second.v);
          break;
        case OpKind::FiltGtSync:
        case OpKind::FiltLtSync:
        case OpKind::FiltEqSync:
        case OpKind::FiltNeqSync: {
          if (in_conns.size() < 2) {
            throw std::runtime_error("SegmentEmitter: filter sync op " + op_id +
                                     " needs 2 inputs");
          }
          auto inp2 = value_map.find({in_conns[1]->from_id, in_conns[1]->from_port});
          if (inp2 == value_map.end()) {
            throw std::runtime_error("SegmentEmitter: missing 2nd input for " + op_id);
          }
          if (node.kind == OpKind::FiltGtSync) {
            so = emit::emit_filt_gt_sync(ec, inp->second.t, inp->second.v, inp2->second.v);
          } else if (node.kind == OpKind::FiltLtSync) {
            so = emit::emit_filt_lt_sync(ec, inp->second.t, inp->second.v, inp2->second.v);
          } else if (node.kind == OpKind::FiltEqSync) {
            so = emit::emit_filt_eq_sync(ec, node.tolerance, inp->second.t,
                                          inp->second.v, inp2->second.v);
          } else {
            so = emit::emit_filt_neq_sync(ec, node.tolerance, inp->second.t,
                                           inp->second.v, inp2->second.v);
          }
          break;
        }
        default:
          break;
      }
      combined_flag = b.CreateAnd(combined_flag, so.emit_flag, "comb_flag");
      so_results.push_back({op_id, so});
    }
  }

  // -----------------------------------------------------------------------
  // Single combined warmup guard.
  // -----------------------------------------------------------------------
  llvm::BasicBlock* emit_bb =
      llvm::BasicBlock::Create(ctx, "chain_emit", fn);
  b.CreateCondBr(combined_flag, emit_bb, chain_exit_bb);

  // -----------------------------------------------------------------------
  // Emit block: register stateful outputs, then stateless ops, always-emit
  // aggregates, Gate, and Output.
  // -----------------------------------------------------------------------
  b.SetInsertPoint(emit_bb);

  // MA/StdDev: compute output values from the split update results.
  for (auto& e : ma_results) {
    llvm::Value* out_v = emit::emit_moving_average_output(ec, e.upd);
    value_map[{e.op_id, 0}] = {rs_rt, out_v};
  }
  for (auto& e : sd_results) {
    llvm::Value* out_v = emit::emit_stddev_output(ec, e.upd, e.W);
    value_map[{e.op_id, 0}] = {rs_rt, out_v};
  }
  // Full-form stateful ops: output values already computed in the update phase.
  for (auto& e : so_results) {
    value_map[{e.op_id, 0}] = {e.so.out_t, e.so.out_v};
  }

  for (const auto& op_id : post_rs_ids) {
    const OpNode& node = find_node(graph, op_id);

    // Windowed stateful ops: already populated in value_map above — skip.
    bool is_windowed_stateful =
        (node.kind == OpKind::MovingAverage || node.kind == OpKind::StdDev  ||
         node.kind == OpKind::MovingSum      || node.kind == OpKind::Diff    ||
         node.kind == OpKind::SignChange     || node.kind == OpKind::WinMin  ||
         node.kind == OpKind::WinMax         || node.kind == OpKind::FIR     ||
         node.kind == OpKind::IIR            ||
         node.kind == OpKind::TimeShift      ||
         node.kind == OpKind::LessThanOrEqualToReplace ||
         node.kind == OpKind::MovingKeyCount ||
         node.kind == OpKind::FiltGtScalar   || node.kind == OpKind::FiltLtScalar ||
         node.kind == OpKind::FiltEqScalar   || node.kind == OpKind::FiltNeqScalar ||
         node.kind == OpKind::FiltGtSync     || node.kind == OpKind::FiltLtSync ||
         node.kind == OpKind::FiltEqSync     || node.kind == OpKind::FiltNeqSync);
    if (is_windowed_stateful) continue;

    if (node.kind == OpKind::Output) {
      // Check should_emit before writing: Gate may have suppressed this tick.
      llvm::BasicBlock* bb_write =
          llvm::BasicBlock::Create(ctx, "rs_out_write", fn);
      llvm::Value* se = b.CreateLoad(b.getInt1Ty(), al_should_emit);
      b.CreateCondBr(se, bb_write, chain_exit_bb);
      b.SetInsertPoint(bb_write);
      emit_output_writes(ec, graph, output_op_id, num_outputs,
                         value_map, al_out_t, al_out_v, al_did_emit,
                         chain_exit_bb);
      b.SetInsertPoint(chain_exit_bb);
      return;
    }

    if (node.kind == OpKind::Gate) {
      auto in_conns = inputs_for(graph, op_id);
      if (in_conns.empty()) {
        throw std::runtime_error("SegmentEmitter: Gate op " + op_id + " has no input");
      }
      auto pred_it = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
      if (pred_it == value_map.end()) {
        throw std::runtime_error("SegmentEmitter: missing predicate for Gate op " + op_id);
      }
      emit::emit_gate(ec, al_should_emit, pred_it->second.v);
      continue;
    }

    // Always-emit aggregates: CumSum, Count, MaxAgg, MinAgg.
    {
      bool is_aggregate = (node.kind == OpKind::CumSum || node.kind == OpKind::Count ||
                           node.kind == OpKind::MaxAgg || node.kind == OpKind::MinAgg);
      if (is_aggregate) {
        auto in_conns = inputs_for(graph, op_id);
        std::size_t offset = layout.offsets.at(op_id);
        llvm::Value* inp_v = nullptr;
        if (!in_conns.empty()) {
          auto it = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
          if (it != value_map.end()) inp_v = it->second.v;
        }
        llvm::Value* out_v = nullptr;
        switch (node.kind) {
          case OpKind::CumSum:  out_v = emit::emit_cumsum(ec, offset, inp_v); break;
          case OpKind::Count:   out_v = emit::emit_count(ec, offset);         break;
          case OpKind::MaxAgg:  out_v = emit::emit_max_agg(ec, offset, inp_v); break;
          case OpKind::MinAgg:  out_v = emit::emit_min_agg(ec, offset, inp_v); break;
          default:              break;
        }
        value_map[{op_id, 0}] = {rs_rt, out_v};
        continue;
      }
    }

    // StateLoad: read source op's first state slot.
    if (node.kind == OpKind::StateLoad) {
      auto src_it = layout.offsets.find(node.state_source_id);
      if (src_it == layout.offsets.end()) {
        throw std::runtime_error(
            "SegmentEmitter(resampler): StateLoad op " + op_id +
            " references unknown source op " + node.state_source_id);
      }
      llvm::Value* loaded = emit::emit_state_load(ec, src_it->second);
      value_map[{op_id, 0}] = {rs_rt, loaded};
      continue;
    }

    // Stateless op.
    emit_stateless_op(ec, graph, node, value_map);
  }

  // Fell off the end without reaching Output (should not happen in a valid graph).
  b.CreateBr(chain_exit_bb);
  b.SetInsertPoint(chain_exit_bb);
}

// Walk Pipeline's segment_bytecode at JIT-compile time and produce an SSA
// value (double) for the segment key. Reads vector lanes from `lanes` (one
// SSA double per lane). The opcode subset matches Pipeline::evaluate_segment_
// bytecode (0..20 + 26..34); stateful aggregate opcodes (21..25) are not
// supported and stateful windowed opcodes (35..43) are likewise rejected by
// Pipeline::validate_segment_bytecode_stack_, so this walker only handles
// stateless cases.
//
// Termination is the first END (opcode 20). All FE END opcodes after the
// first one are not produced by Pipeline's segment bytecode (single scalar
// key). Stack underflow / overflow / unknown opcode cases throw at JIT-
// compile time so malformed bytecode never produces invalid IR.
llvm::Value* walk_segment_bytecode(IrEmissionContext& ec,
                                    const std::vector<double>& bytecode,
                                    const std::vector<double>& constants,
                                    const std::vector<llvm::Value*>& lanes) {
  auto& ctx = ec.ctx();
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);

  std::vector<llvm::Value*> stack;
  stack.reserve(rtbot::Pipeline::kSegmentStackSize);

  auto pop = [&]() -> llvm::Value* {
    llvm::Value* v = stack.back();
    stack.pop_back();
    return v;
  };

  std::size_t pc = 0;
  while (pc < bytecode.size()) {
    const int opcode = static_cast<int>(bytecode[pc++]);
    switch (opcode) {
      case 0 /* INPUT */: {
        const std::size_t lane = static_cast<std::size_t>(bytecode[pc++]);
        if (lane >= lanes.size()) {
          throw std::runtime_error(
              "Pipeline segment bytecode: INPUT lane " + std::to_string(lane) +
              " out of range (vector width " + std::to_string(lanes.size()) + ")");
        }
        stack.push_back(lanes[lane]);
        break;
      }
      case 1 /* CONST */: {
        const std::size_t idx = static_cast<std::size_t>(bytecode[pc++]);
        if (idx >= constants.size()) {
          throw std::runtime_error(
              "Pipeline segment bytecode: CONST idx " + std::to_string(idx) +
              " out of range (constants size " +
              std::to_string(constants.size()) + ")");
        }
        stack.push_back(llvm::ConstantFP::get(f64, constants[idx]));
        break;
      }
      case 2 /* ADD */:  { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_add(ec, x, y)); break; }
      case 3 /* SUB */:  { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_sub(ec, x, y)); break; }
      case 4 /* MUL */:  { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_mul(ec, x, y)); break; }
      case 5 /* DIV */:  { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_div(ec, x, y)); break; }
      case 6 /* POW */:  { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_pow(ec, x, y)); break; }
      case 7 /* ABS */:   { llvm::Value* x=pop(); stack.push_back(emit::emit_abs  (ec, x)); break; }
      case 8 /* SQRT */:  { llvm::Value* x=pop(); stack.push_back(emit::emit_sqrt (ec, x)); break; }
      case 9 /* LOG */:   { llvm::Value* x=pop(); stack.push_back(emit::emit_log  (ec, x)); break; }
      case 10 /* LOG10 */:{ llvm::Value* x=pop(); stack.push_back(emit::emit_log10(ec, x)); break; }
      case 11 /* EXP */:  { llvm::Value* x=pop(); stack.push_back(emit::emit_exp  (ec, x)); break; }
      case 12 /* SIN */:  { llvm::Value* x=pop(); stack.push_back(emit::emit_sin  (ec, x)); break; }
      case 13 /* COS */:  { llvm::Value* x=pop(); stack.push_back(emit::emit_cos  (ec, x)); break; }
      case 14 /* TAN */:  { llvm::Value* x=pop(); stack.push_back(emit::emit_tan  (ec, x)); break; }
      case 15 /* SIGN */: { llvm::Value* x=pop(); stack.push_back(emit::emit_sign (ec, x)); break; }
      case 16 /* FLOOR */:{ llvm::Value* x=pop(); stack.push_back(emit::emit_floor(ec, x)); break; }
      case 17 /* CEIL */: { llvm::Value* x=pop(); stack.push_back(emit::emit_ceil (ec, x)); break; }
      case 18 /* ROUND */:{ llvm::Value* x=pop(); stack.push_back(emit::emit_round(ec, x)); break; }
      case 19 /* NEG */:  { llvm::Value* x=pop(); stack.push_back(emit::emit_neg  (ec, x)); break; }
      case 20 /* END */: {
        if (stack.empty()) {
          throw std::runtime_error("Pipeline segment bytecode: END with empty stack");
        }
        return stack.back();
      }
      case 26 /* GT */:  { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_gt (ec, x, y)); break; }
      case 27 /* GTE */: { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_gte(ec, x, y)); break; }
      case 28 /* LT */:  { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_lt (ec, x, y)); break; }
      case 29 /* LTE */: { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_lte(ec, x, y)); break; }
      case 30 /* EQ */:  { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_eq (ec, x, y)); break; }
      case 31 /* NEQ */: { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_neq(ec, x, y)); break; }
      case 32 /* AND */: { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_and(ec, x, y)); break; }
      case 33 /* OR */:  { llvm::Value* y=pop(); llvm::Value* x=pop(); stack.push_back(emit::emit_or (ec, x, y)); break; }
      case 34 /* NOT */: { llvm::Value* x=pop(); stack.push_back(emit::emit_not(ec, x)); break; }
      default:
        throw std::runtime_error(
            "Pipeline segment bytecode: unsupported opcode " +
            std::to_string(opcode) +
            " (only 0..20 and 26..34 are allowed in segment bytecode)");
    }
  }
  throw std::runtime_error("Pipeline segment bytecode: missing END opcode");
}

}  // namespace

EmittedSegment emit_segment(llvm::LLVMContext& ctx, llvm::Module& mod,
                            const CompiledGraph& graph,
                            const Segment& segment,
                            const StateLayout& layout) {
  // -------------------------------------------------------------------------
  // Identify Input, Output, and Resampler ops.
  // -------------------------------------------------------------------------
  std::string input_op_id;
  std::string output_op_id;
  std::string resampler_op_id;

  for (const auto& id : segment.op_ids) {
    const OpNode& node = find_node(graph, id);
    if (node.kind == OpKind::Pipeline) {
      // Pipeline is a sync op (see SegmentPartitioner::is_sync_op), so it
      // cannot appear inside a single-segment graph; emit_program is the
      // entry path. Reaching here means the graph was misconfigured.
      throw std::runtime_error(
          "emit_segment: Pipeline op '" + id +
          "' must be partitioned at a sync boundary; use emit_program instead");
    }
    if (node.kind == OpKind::Input)             input_op_id      = id;
    if (node.kind == OpKind::Output)            output_op_id     = id;
    if (node.kind == OpKind::ResamplerHermite ||
        node.kind == OpKind::ResamplerConstant) resampler_op_id  = id;
  }

  if (input_op_id.empty()) {
    throw std::runtime_error("SegmentEmitter: segment has no Input op");
  }
  if (output_op_id.empty()) {
    throw std::runtime_error("SegmentEmitter: segment has no Output op");
  }

  const std::size_t num_outputs = compute_num_outputs(graph, output_op_id);

  // -------------------------------------------------------------------------
  // Choose a unique function name.
  // -------------------------------------------------------------------------
  static int fn_counter = 0;
  std::string fn_name = "segment_process_" + std::to_string(fn_counter++);

  // -------------------------------------------------------------------------
  // Build the function:
  //   i8 segment_process(double* state, i64 t, double v, i64* out_t,
  //                      double* out_v_array, i32* out_port_id)
  // -------------------------------------------------------------------------
  llvm::Type* f64  = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64  = llvm::Type::getInt64Ty(ctx);
  llvm::Type* i32  = llvm::Type::getInt32Ty(ctx);
  llvm::Type* i8   = llvm::Type::getInt8Ty(ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);
  llvm::Type* i64p = llvm::PointerType::getUnqual(i64);
  llvm::Type* i32p = llvm::PointerType::getUnqual(i32);

  llvm::FunctionType* fn_ty = llvm::FunctionType::get(
      i32, {f64p, i64, f64, i64p, f64p, i32p}, false);

  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, fn_name, mod);

  auto ai = fn->arg_begin();
  llvm::Argument* arg_state       = &*ai++;  arg_state->setName("state");
  llvm::Argument* arg_t           = &*ai++;  arg_t->setName("t");
  llvm::Argument* arg_v           = &*ai++;  arg_v->setName("v");
  llvm::Argument* arg_out_t       = &*ai++;  arg_out_t->setName("out_t_arr");
  llvm::Argument* arg_out_v       = &*ai++;  arg_out_v->setName("out_v_arr");
  llvm::Argument* arg_out_port_id = &*ai++;  arg_out_port_id->setName("out_port_id_arr");

  llvm::IRBuilder<> b(ctx);
  b.setFastMathFlags(llvm::FastMathFlags{});

  // -------------------------------------------------------------------------
  // Basic blocks.
  // -------------------------------------------------------------------------
  llvm::BasicBlock* bb_entry     = llvm::BasicBlock::Create(ctx, "entry",     fn);
  llvm::BasicBlock* bb_ret_false = llvm::BasicBlock::Create(ctx, "ret_false", fn);
  llvm::BasicBlock* bb_ret_true  = llvm::BasicBlock::Create(ctx, "ret_true",  fn);

  b.SetInsertPoint(bb_ret_false);
  b.CreateRet(llvm::ConstantInt::get(i32, 0));

  b.SetInsertPoint(bb_entry);

  // Default slot 0's port id to 0. Single-emit emitters use slot 0; multi-
  // emit Demux writes per-record port ids and a count > 1.
  b.CreateStore(llvm::ConstantInt::get(i32, 0), arg_out_port_id);

  IrEmissionContext ec(ctx, mod, b, arg_state);
  (void)i32p;

  // -------------------------------------------------------------------------
  // Allocas (in entry block, before any control flow).
  // -------------------------------------------------------------------------
  llvm::Value* al_out_t_slot = b.CreateAlloca(i64, nullptr, "al_out_t");
  std::vector<llvm::Value*> al_out_v;
  al_out_v.reserve(num_outputs);
  for (std::size_t i = 0; i < num_outputs; ++i) {
    al_out_v.push_back(b.CreateAlloca(f64, nullptr, "al_out_v" + std::to_string(i)));
  }
  llvm::Value* al_did_emit = b.CreateAlloca(i8, nullptr, "al_did_emit");
  b.CreateStore(llvm::ConstantInt::get(i8, 0), al_did_emit);

  // should_emit: program-level Gate suppression flag (i1, starts true).
  // Gate ops AND this with (predicate != 0.0). Output checks it before
  // writing results. Always allocated so LLVM can fold it for Gate-free programs.
  llvm::Value* al_should_emit = b.CreateAlloca(b.getInt1Ty(), nullptr, "al_should_emit");
  b.CreateStore(llvm::ConstantInt::getTrue(ctx), al_should_emit);

  // -------------------------------------------------------------------------
  // Partition: ops before Resampler vs ops after Resampler.
  // -------------------------------------------------------------------------
  const bool has_resampler = !resampler_op_id.empty();

  std::vector<std::string> pre_rs_ids;
  std::vector<std::string> post_rs_ids;

  if (has_resampler) {
    bool past_rs = false;
    for (const auto& id : segment.op_ids) {
      if (id == resampler_op_id) { past_rs = true; continue; }
      if (!past_rs) pre_rs_ids.push_back(id);
      else          post_rs_ids.push_back(id);
    }
  }

  // =========================================================================
  // Case A: Linear (no Resampler).
  // =========================================================================
  if (!has_resampler) {
    ValueMap value_map;

    // Two-phase emission: every stateful op runs its full state-update IR
    // unconditionally (so parallel branches advance in lockstep during warmup).
    // Each op's emit_flag is ANDed into combined_flag; the single warmup branch
    // is taken at Output.
    llvm::Value* combined_flag = llvm::ConstantInt::getTrue(ctx);

    for (const auto& op_id : segment.op_ids) {
      const OpNode& node = find_node(graph, op_id);

      // --- Input ---
      if (node.kind == OpKind::Input) {
        value_map[{op_id, 0}] = {arg_t, arg_v};
        continue;
      }

      // --- Output: check combined warmup flag AND Gate suppression ---
      if (node.kind == OpKind::Output) {
        llvm::Value* se = b.CreateLoad(b.getInt1Ty(), al_should_emit);
        llvm::Value* gate_flag = b.CreateAnd(combined_flag, se, "out_gate");
        llvm::BasicBlock* bb_out_write =
            llvm::BasicBlock::Create(ctx, "out_write", fn);
        b.CreateCondBr(gate_flag, bb_out_write, bb_ret_false);
        b.SetInsertPoint(bb_out_write);

        auto in_conns = inputs_for(graph, op_id);
        const OpNode& output_node = node;
        if (!in_conns.empty()) {
          auto it = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
          if (it != value_map.end()) {
            b.CreateStore(it->second.t, arg_out_t);
          }
        }
        for (const auto* conn : in_conns) {
          auto it = value_map.find({conn->from_id, conn->from_port});
          if (it == value_map.end()) continue;
          write_port_value_to_slots(
              ec, output_node, conn->to_port, it->second,
              [&](std::size_t slot, llvm::Value* val) {
                if (slot < num_outputs) {
                  llvm::Value* g = b.CreateGEP(
                      f64, arg_out_v,
                      llvm::ConstantInt::get(i64, static_cast<int64_t>(slot)));
                  b.CreateStore(val, g);
                }
              });
        }
        b.CreateBr(bb_ret_true);
        break;
      }

      // --- Gate: AND should_emit with (predicate != 0.0) --------------------
      if (node.kind == OpKind::Gate) {
        auto in_conns = inputs_for(graph, op_id);
        if (in_conns.empty()) {
          throw std::runtime_error("SegmentEmitter: Gate op " + op_id + " has no input");
        }
        auto pred_it = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
        if (pred_it == value_map.end()) {
          throw std::runtime_error("SegmentEmitter: missing predicate for Gate op " + op_id);
        }
        emit::emit_gate(ec, al_should_emit, pred_it->second.v);
        continue;
      }

      // --- Stateful: windowed ops that return StatefulOutput ---
      // (and stateless conditional-skip ops TimeShift/Replace, which share the
      //  same StatefulOutput shape and benefit from the same skip plumbing.)
      {
        bool is_stateful_so = (node.kind == OpKind::MovingAverage ||
                               node.kind == OpKind::StdDev        ||
                               node.kind == OpKind::MovingSum      ||
                               node.kind == OpKind::PeakDetector   ||
                               node.kind == OpKind::Diff           ||
                               node.kind == OpKind::SignChange      ||
                               node.kind == OpKind::WinMin         ||
                               node.kind == OpKind::WinMax         ||
                               node.kind == OpKind::FIR            ||
                               node.kind == OpKind::IIR            ||
                               node.kind == OpKind::TimeShift      ||
                               node.kind == OpKind::LessThanOrEqualToReplace ||
                               node.kind == OpKind::MovingKeyCount ||
                               node.kind == OpKind::FiltGtScalar   ||
                               node.kind == OpKind::FiltLtScalar   ||
                               node.kind == OpKind::FiltEqScalar   ||
                               node.kind == OpKind::FiltNeqScalar  ||
                               node.kind == OpKind::FiltGtSync     ||
                               node.kind == OpKind::FiltLtSync     ||
                               node.kind == OpKind::FiltEqSync     ||
                               node.kind == OpKind::FiltNeqSync);
        if (is_stateful_so) {
          auto in_conns = inputs_for(graph, op_id);
          if (in_conns.empty()) {
            throw std::runtime_error("SegmentEmitter: stateful op " + op_id + " has no input");
          }
          auto inp = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
          if (inp == value_map.end()) {
            throw std::runtime_error("SegmentEmitter: missing input for " + op_id);
          }
          std::size_t offset = layout.offsets.at(op_id);

          emit::StatefulOutput out{};
          switch (node.kind) {
            case OpKind::MovingAverage:
              out = emit::emit_moving_average(ec, offset, node.window_size,
                                              inp->second.t, inp->second.v);
              break;
            case OpKind::StdDev:
              out = emit::emit_stddev(ec, offset, node.window_size,
                                      inp->second.t, inp->second.v);
              break;
            case OpKind::MovingSum:
              out = emit::emit_moving_sum(ec, offset, node.window_size,
                                          inp->second.t, inp->second.v);
              break;
            case OpKind::PeakDetector:
              out = emit::emit_peak_detector(ec, offset, node.window_size,
                                             inp->second.t, inp->second.v);
              break;
            case OpKind::Diff:
              out = emit::emit_diff(ec, offset, /*use_oldest_time=*/true,
                                    inp->second.t, inp->second.v);
              break;
            case OpKind::SignChange:
              out = emit::emit_sign_change(ec, offset, inp->second.t, inp->second.v);
              break;
            case OpKind::WinMin:
              out = emit::emit_win_min(ec, offset, node.window_size,
                                       inp->second.t, inp->second.v);
              break;
            case OpKind::WinMax:
              out = emit::emit_win_max(ec, offset, node.window_size,
                                       inp->second.t, inp->second.v);
              break;
            case OpKind::FIR:
              out = emit::emit_fir(ec, offset, node.window_size,
                                   node.coefficients,
                                   inp->second.t, inp->second.v);
              break;
            case OpKind::IIR:
              out = emit::emit_iir(ec, offset, node.b_len, node.a_len,
                                   node.coefficients,
                                   inp->second.t, inp->second.v);
              break;
            case OpKind::TimeShift:
              out = emit::emit_time_shift(ec, node.time_shift,
                                          inp->second.t, inp->second.v);
              break;
            case OpKind::LessThanOrEqualToReplace:
              out = emit::emit_lte_replace(ec, node.replace_threshold,
                                           node.replace_by,
                                           inp->second.t, inp->second.v);
              break;
            case OpKind::MovingKeyCount:
              out = emit::emit_moving_key_count(ec, offset, node.mkc_window_size,
                                                inp->second.t, inp->second.v);
              break;
            case OpKind::FiltGtScalar:
              out = emit::emit_filt_gt_scalar(ec, node.scalar_value,
                                              inp->second.t, inp->second.v);
              break;
            case OpKind::FiltLtScalar:
              out = emit::emit_filt_lt_scalar(ec, node.scalar_value,
                                              inp->second.t, inp->second.v);
              break;
            case OpKind::FiltEqScalar:
              out = emit::emit_filt_eq_scalar(ec, node.scalar_value, node.tolerance,
                                               inp->second.t, inp->second.v);
              break;
            case OpKind::FiltNeqScalar:
              out = emit::emit_filt_neq_scalar(ec, node.scalar_value, node.tolerance,
                                                inp->second.t, inp->second.v);
              break;
            case OpKind::FiltGtSync:
            case OpKind::FiltLtSync:
            case OpKind::FiltEqSync:
            case OpKind::FiltNeqSync: {
              if (in_conns.size() < 2) {
                throw std::runtime_error("SegmentEmitter: filter sync op " + op_id +
                                         " needs 2 inputs");
              }
              auto inp2 = value_map.find({in_conns[1]->from_id, in_conns[1]->from_port});
              if (inp2 == value_map.end()) {
                throw std::runtime_error("SegmentEmitter: missing 2nd input for " + op_id);
              }
              if (node.kind == OpKind::FiltGtSync) {
                out = emit::emit_filt_gt_sync(ec, inp->second.t, inp->second.v, inp2->second.v);
              } else if (node.kind == OpKind::FiltLtSync) {
                out = emit::emit_filt_lt_sync(ec, inp->second.t, inp->second.v, inp2->second.v);
              } else if (node.kind == OpKind::FiltEqSync) {
                out = emit::emit_filt_eq_sync(ec, node.tolerance, inp->second.t,
                                               inp->second.v, inp2->second.v);
              } else {
                out = emit::emit_filt_neq_sync(ec, node.tolerance, inp->second.t,
                                                inp->second.v, inp2->second.v);
              }
              break;
            }
            default:
              break;
          }

          // Combine into the segment-wide warmup flag. The per-op branch is
          // deferred to Output so all parallel stateful branches advance
          // state in lockstep during warmup.
          combined_flag = b.CreateAnd(combined_flag, out.emit_flag, "comb_flag");
          value_map[{op_id, 0}] = {out.out_t, out.out_v};
          continue;
        }
      }

      // --- Always-emit aggregates: CumSum, Count, MaxAgg, MinAgg ---
      {
        bool is_aggregate = (node.kind == OpKind::CumSum  ||
                             node.kind == OpKind::Count   ||
                             node.kind == OpKind::MaxAgg  ||
                             node.kind == OpKind::MinAgg);
        if (is_aggregate) {
          auto in_conns = inputs_for(graph, op_id);
          std::size_t offset = layout.offsets.at(op_id);
          llvm::Value* inp_v = nullptr;
          llvm::Value* inp_t = nullptr;
          if (!in_conns.empty()) {
            auto it = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
            if (it != value_map.end()) {
              inp_v = it->second.v;
              inp_t = it->second.t;
            }
          }
          llvm::Value* out_v = nullptr;
          switch (node.kind) {
            case OpKind::CumSum:  out_v = emit::emit_cumsum(ec, offset, inp_v); break;
            case OpKind::Count:   out_v = emit::emit_count(ec, offset);         break;
            case OpKind::MaxAgg:  out_v = emit::emit_max_agg(ec, offset, inp_v); break;
            case OpKind::MinAgg:  out_v = emit::emit_min_agg(ec, offset, inp_v); break;
            default:              break;
          }
          value_map[{op_id, 0}] = {inp_t, out_v};
          continue;
        }
      }

      // --- StateLoad: read source op's first state slot ---
      if (node.kind == OpKind::StateLoad) {
        auto src_it = layout.offsets.find(node.state_source_id);
        if (src_it == layout.offsets.end()) {
          throw std::runtime_error(
              "SegmentEmitter: StateLoad op " + op_id +
              " references unknown source op " + node.state_source_id);
        }
        llvm::Value* loaded = emit::emit_state_load(ec, src_it->second);
        // Use the current tick's timestamp (arg_t) as the StateLoad output time.
        value_map[{op_id, 0}] = {arg_t, loaded};
        continue;
      }

      // --- TopK: terminal multi-emit op (writes records and returns count) ---
      if (node.kind == OpKind::TopK) {
        auto in_conns = inputs_for(graph, op_id);
        if (in_conns.empty()) {
          throw std::runtime_error("SegmentEmitter: TopK op " + op_id + " has no input");
        }
        auto inp = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
        if (inp == value_map.end()) {
          throw std::runtime_error("SegmentEmitter: missing input for TopK " + op_id);
        }
        const std::size_t W = (node.topk_row_width == 0) ? 1 : node.topk_row_width;
        std::vector<llvm::Value*> lanes;
        lanes.reserve(W);
        if (inp->second.width <= 1) {
          if (W != 1) {
            throw std::runtime_error(
                "SegmentEmitter: TopK '" + op_id +
                "' row_width > 1 but upstream wire is scalar");
          }
          lanes.push_back(inp->second.v);
        } else {
          if (inp->second.width != W) {
            throw std::runtime_error(
                "SegmentEmitter: TopK '" + op_id + "' row_width " +
                std::to_string(W) +
                " does not match upstream vector width " +
                std::to_string(inp->second.width));
          }
          llvm::Type* i32t = llvm::Type::getInt32Ty(ctx);
          for (std::size_t k = 0; k < W; ++k) {
            llvm::Value* el = b.CreateExtractElement(
                inp->second.v,
                llvm::ConstantInt::get(i32t, static_cast<std::uint32_t>(k)),
                "tk_in_lane");
            lanes.push_back(el);
          }
        }

        std::size_t out_port_id = 0;
        for (const auto& conn : graph.connections) {
          if (conn.from_id == op_id && conn.to_id == output_op_id &&
              conn.from_port == 0) {
            out_port_id = conn.to_port;
            break;
          }
        }

        const std::size_t offset = layout.offsets.at(op_id);
        llvm::Value* count = emit::emit_topk(
            ec, offset, node.topk_k, W, node.topk_score_index,
            node.topk_descending, inp->second.t, lanes,
            arg_out_t, arg_out_v, arg_out_port_id, num_outputs, out_port_id);

        b.CreateRet(count);
        // TopK terminates the segment; bypass any subsequent ops (Output is
        // the only legal successor and is short-circuited by the Ret above).
        break;
      }

      // --- Stateless ---
      emit_stateless_op(ec, graph, node, value_map);
    }
  }

  // =========================================================================
  // Case B: Has Resampler.
  // =========================================================================
  else {
    // Step 1: emit pre-Resampler ops (Input → Resampler predecessor).
    ValueMap pre_value_map;
    for (const auto& op_id : pre_rs_ids) {
      const OpNode& node = find_node(graph, op_id);
      if (node.kind == OpKind::Input) {
        pre_value_map[{op_id, 0}] = {arg_t, arg_v};
        continue;
      }
      emit_stateless_op(ec, graph, node, pre_value_map);
    }

    // Step 2: find the Resampler's input.
    auto rs_in_conns = inputs_for(graph, resampler_op_id);
    if (rs_in_conns.empty()) {
      throw std::runtime_error("SegmentEmitter: Resampler " + resampler_op_id +
                               " has no input connections");
    }
    auto rs_inp = pre_value_map.find(
        {rs_in_conns[0]->from_id, rs_in_conns[0]->from_port});
    if (rs_inp == pre_value_map.end()) {
      throw std::runtime_error("SegmentEmitter: missing input for Resampler " +
                               resampler_op_id);
    }

    const OpNode& rs_node  = find_node(graph, resampler_op_id);
    std::size_t   rs_offset = layout.offsets.at(resampler_op_id);

    // Step 3: emit the Resampler with a callback that runs the downstream chain.
    auto chain_cb = [&](llvm::Value* cb_rt, llvm::Value* cb_rv) {
      emit_post_resampler_chain(
          ec, graph, layout,
          post_rs_ids,
          resampler_op_id,
          output_op_id,
          num_outputs,
          al_out_t_slot, al_out_v, al_did_emit, al_should_emit,
          cb_rt, cb_rv);
    };
    if (rs_node.kind == OpKind::ResamplerHermite) {
      emit::emit_resampler_hermite(
          ec, rs_offset, rs_node.resampler_interval,
          rs_inp->second.t, rs_inp->second.v, chain_cb);
    } else {
      emit::emit_resampler_constant(
          ec, rs_offset, rs_node.resampler_interval,
          rs_node.resampler_t0_set, rs_node.resampler_t0,
          rs_node.resampler_snap_first,
          rs_inp->second.t, rs_inp->second.v, chain_cb);
    }

    // After loop_exit: check al_did_emit.
    // IRBuilder is in bb_loop_exit (set by emit_resampler_hermite).
    llvm::Value* did = b.CreateLoad(i8, al_did_emit, "did_emit");
    llvm::BasicBlock* bb_write_out =
        llvm::BasicBlock::Create(ctx, "write_outputs", fn);
    b.CreateCondBr(
        b.CreateICmpNE(did, llvm::ConstantInt::get(i8, 0), "did_nz"),
        bb_write_out, bb_ret_false);

    // Write from allocas to function output parameters, then ret true.
    b.SetInsertPoint(bb_write_out);
    b.CreateStore(b.CreateLoad(i64, al_out_t_slot, "ot"), arg_out_t);
    for (std::size_t i = 0; i < num_outputs; ++i) {
      llvm::Value* slot = b.CreateGEP(
          f64, arg_out_v,
          llvm::ConstantInt::get(i64, static_cast<int64_t>(i)));
      b.CreateStore(b.CreateLoad(f64, al_out_v[i], "ov"), slot);
    }
    b.CreateBr(bb_ret_true);
  }

  // -------------------------------------------------------------------------
  // ret_true block.
  // -------------------------------------------------------------------------
  b.SetInsertPoint(bb_ret_true);
  b.CreateRet(llvm::ConstantInt::get(i32, 1));

  // -------------------------------------------------------------------------
  // Verify.
  // -------------------------------------------------------------------------
  std::string err;
  llvm::raw_string_ostream errs(err);
  if (llvm::verifyFunction(*fn, &errs)) {
    errs.flush();
    llvm::report_fatal_error(
        llvm::StringRef("emit_segment: IR verification failed for '" +
                        fn_name + "': " + err));
  }

  return EmittedSegment{fn_name, num_outputs, layout.total_size};
}

// ---------------------------------------------------------------------------
// emit_program — multi-segment graph with Join sync operators
// ---------------------------------------------------------------------------
//
// Control-flow shape (mirrors PPGCompiled::process exactly):
//
// The graph is partitioned by SegmentPartitioner into segments and sync_ops.
// For PPG: segments = [[Input, ma_short, ma_long], [minus, peak], [output]]
//          sync_ops = [join_ma, join_out]
//
// For each segment:
//   - Ops in the segment are emitted in order.
//   - Input: seeds value_map; pushes directly to any Join successors unconditionally.
//   - Stateful ops (MA, StdDev, PeakDetector): always run the state update.
//     After the update, gate the downstream push-to-Join behind the emit_flag.
//   - Stateless ops: straight-line; push to any Join successors unconditionally.
//   - Output: write to out params, branch to ret_true.
//
// Between consecutive segments, the sync op (Join) at the boundary:
//   - emit_join_try_sync runs unconditionally (reads from state).
//   - If sync succeeds (then_bb): populate value_map for the Join's outputs,
//     then emit the NEXT segment's ops (which depend on the Join's values).
//   - After the then_bb, resume with the NEXT sync op's try_sync in merge_bb.
//
// This nesting is key: segment[i+1] ops run inside the then_bb of sync_ops[i],
// because those ops depend on the Join's synced values. The next try_sync
// runs in the merge_bb (which is reached unconditionally after the conditional).
//
// Restriction: only Join sync ops; no Resampler mixed with Joins.

// Helper: emit one segment's ops. The IRBuilder must be positioned in the
// current block. After this call, the IRBuilder is in the same block (or a
// continuation block if stateful ops were present).
//
// join_ids / join_n_ports: set of known Join op ids and their arity.
// join_successors: lambda that maps (op_id, from_port) -> [(join_id, to_port)].
// should_emit_alloca: i1* alloca for the program-level Gate suppression flag.
void emit_segment_ops(
    IrEmissionContext& ec,
    llvm::IRBuilder<>& b,
    llvm::LLVMContext& ctx,
    llvm::Function* fn,
    const CompiledGraph& graph,
    const StateLayout& layout,
    const std::vector<std::string>& op_ids,
    const std::string& output_op_id,
    std::size_t num_outputs,
    llvm::Value* arg_out_t,
    llvm::Value* arg_out_v,
    llvm::Value* arg_out_port_id,
    llvm::BasicBlock* bb_ret_true,
    llvm::BasicBlock* bb_ret_false,
    llvm::Value* should_emit_alloca,
    ValueMap& value_map,
    const std::set<std::string>& join_ids,
    const std::unordered_map<std::string, std::size_t>& join_n_ports,
    const std::function<std::vector<std::pair<std::string,std::size_t>>(
        const std::string&, std::size_t)>& join_successors) {

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);

  for (const auto& op_id : op_ids) {
    const OpNode& node = find_node(graph, op_id);

    // --- Input: seed value_map; push unconditionally to Join successors ------
    if (node.kind == OpKind::Input) {
      // Retrieve the function's input arguments from the already-seeded value_map.
      // Input is always in the first segment; its value is already populated.
      // Push to any Join successors. Vector wires (width > 1) feeding a
      // segment-bytecode Pipeline fan out lane-by-lane into the Pipeline's
      // per-lane port queues; other downstream consumers read the wire from
      // value_map directly and need no push.
      auto it = value_map.find({op_id, 0});
      if (it != value_map.end()) {
        const PortValue& pv = it->second;
        for (auto [join_id, to_port] : join_successors(op_id, 0)) {
          std::size_t jo = layout.offsets.at(join_id);
          std::size_t jN = join_n_ports.at(join_id);
          if (pv.width <= 1) {
            emit::emit_join_push(ec, jo, jN, to_port, pv.t, pv.v);
          } else {
            const OpNode& dst = find_node(graph, join_id);
            const bool is_pipe_segbc = dst.kind == OpKind::Pipeline &&
                                        !dst.pipeline_segment_bytecode.empty();
            const bool is_keyed_pipe = dst.kind == OpKind::KeyedPipeline;
            if (!is_pipe_segbc && !is_keyed_pipe) {
              throw std::runtime_error(
                  "emit_segment_ops: vector wire from Input '" + op_id +
                  "' feeds non-segment-bytecode-Pipeline sync op '" + join_id +
                  "', which is not supported");
            }
            llvm::Type* i32t = llvm::Type::getInt32Ty(ctx);
            for (std::size_t k = 0; k < pv.width; ++k) {
              llvm::Value* lane = ec.b().CreateExtractElement(
                  pv.v,
                  llvm::ConstantInt::get(i32t, static_cast<std::uint32_t>(k)),
                  "in_v_lane_to_pipe");
              emit::emit_join_push(ec, jo, jN, k, pv.t, lane);
            }
          }
        }
      }
      continue;
    }

    // --- Output: check Gate suppression, then write to function output --------
    if (node.kind == OpKind::Output) {
      // If any of Output's data inputs come from a Pipeline / KeyedPipeline
      // sync op, that op manages its own writes to out_t_arr / out_v_arr in
      // its sync-op handler. Emitting Output's writes here in an upstream
      // sync op's then_bb (which runs BEFORE the Pipeline iteration) would
      // prematurely return ret_true with stale value_map entries. Skip
      // Output entirely in that case; the Pipeline path returns directly to
      // ret_true on flush.
      {
        auto out_in_conns = inputs_for(graph, op_id);
        for (const auto* c : out_in_conns) {
          const OpNode& src = find_node(graph, c->from_id);
          if (src.kind == OpKind::Pipeline ||
              src.kind == OpKind::KeyedPipeline) {
            return;  // Pipeline / KeyedPipeline will write Output itself
          }
        }
      }
      // Gate may have suppressed this tick — check should_emit_alloca.
      llvm::Value* se = b.CreateLoad(b.getInt1Ty(), should_emit_alloca);
      llvm::BasicBlock* bb_out_write =
          llvm::BasicBlock::Create(ctx, "out_write", fn);
      b.CreateCondBr(se, bb_out_write, bb_ret_false);
      b.SetInsertPoint(bb_out_write);

      auto in_conns = inputs_for(graph, op_id);
      const OpNode& output_node = node;
      if (!in_conns.empty()) {
        auto it = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
        if (it != value_map.end()) {
          b.CreateStore(it->second.t, arg_out_t);
        }
      }
      for (const auto* conn : in_conns) {
        auto it = value_map.find({conn->from_id, conn->from_port});
        if (it == value_map.end()) continue;
        write_port_value_to_slots(
            ec, output_node, conn->to_port, it->second,
            [&](std::size_t slot, llvm::Value* val) {
              if (slot < num_outputs) {
                llvm::Value* g = b.CreateGEP(
                    f64, arg_out_v,
                    llvm::ConstantInt::get(i64, static_cast<int64_t>(slot)));
                b.CreateStore(val, g);
              }
            });
      }
      b.CreateBr(bb_ret_true);
      return;  // Output terminates the segment
    }

    // --- Gate: update should_emit_alloca with (predicate != 0.0) AND prev ----
    if (node.kind == OpKind::Gate) {
      auto in_conns = inputs_for(graph, op_id);
      if (in_conns.empty()) {
        throw std::runtime_error("emit_program: Gate op " + op_id + " has no input");
      }
      auto pred_it = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
      if (pred_it == value_map.end()) {
        throw std::runtime_error("emit_program: missing predicate for Gate op " + op_id);
      }
      emit::emit_gate(ec, should_emit_alloca, pred_it->second.v);
      continue;
    }

    // --- Stateless op: straight-line, then push to join successors -----------
    bool is_stateless = false;
    switch (node.kind) {
      case OpKind::Add: case OpKind::Sub: case OpKind::Mul: case OpKind::Div:
      case OpKind::Scale: case OpKind::Pow: case OpKind::Abs: case OpKind::Sqrt:
      case OpKind::Log: case OpKind::Log10: case OpKind::Exp: case OpKind::Sin:
      case OpKind::Cos: case OpKind::Tan: case OpKind::Sign: case OpKind::Floor:
      case OpKind::Ceil: case OpKind::Round: case OpKind::Neg:
      case OpKind::AddScalar: case OpKind::PowerScalar:
      case OpKind::Gt: case OpKind::Gte: case OpKind::Lt: case OpKind::Lte:
      case OpKind::Eq: case OpKind::Neq:
      case OpKind::EqTol: case OpKind::NeqTol:
      case OpKind::GtScalar: case OpKind::LtScalar:
      case OpKind::GteScalar: case OpKind::LteScalar:
      case OpKind::EqScalar: case OpKind::NeqScalar:
      case OpKind::And: case OpKind::Or: case OpKind::Not:
      case OpKind::Xor: case OpKind::Nand: case OpKind::Nor:
      case OpKind::Xnor: case OpKind::Implication:
      case OpKind::Identity: case OpKind::Constant:
      case OpKind::BooleanToNumber: case OpKind::TimestampExtract:
      case OpKind::Function:
      case OpKind::VectorExtract: case OpKind::VectorProject:
        is_stateless = true;
        break;
      default:
        break;
    }
    if (is_stateless) {
      emit_stateless_op(ec, graph, node, value_map);
      auto it = value_map.find({op_id, 0});
      if (it != value_map.end() && it->second.width <= 1) {
        // Joins / Linears / ReduceJoins consume scalar wires only; vector
        // wires (VectorProject output) flow on a single edge to a vector
        // consumer (VectorExtract / VectorProject / Output) and are never
        // pushed into a sync queue.
        auto joins = join_successors(op_id, 0);
        if (!joins.empty()) {
          // Gate pushes on the current should-emit flag so that upstream
          // stateful-op warmup suppression (e.g. MA emit_flag=false) prevents
          // stale values from filling downstream sync queues (VectorCompose).
          llvm::Value* se_push = b.CreateLoad(
              b.getInt1Ty(), should_emit_alloca, "se_push");
          llvm::BasicBlock* bb_push =
              llvm::BasicBlock::Create(ctx, "sl_push", fn);
          llvm::BasicBlock* bb_after =
              llvm::BasicBlock::Create(ctx, "sl_after", fn);
          b.CreateCondBr(se_push, bb_push, bb_after);
          b.SetInsertPoint(bb_push);
          for (auto [join_id, to_port] : joins) {
            std::size_t jo = layout.offsets.at(join_id);
            std::size_t jN = join_n_ports.at(join_id);
            emit::emit_join_push(ec, jo, jN, to_port, it->second.t, it->second.v);
          }
          b.CreateBr(bb_after);
          b.SetInsertPoint(bb_after);
        }
      }
      continue;
    }

    // --- Stateful 1->1 ops that return StatefulOutput -----------------------
    // MA, StdDev, PeakDetector, MovingSum, Diff, SignChange, WinMin, WinMax,
    // FIR, IIR all return emit_flag + (out_t, out_v). The emitter runs the
    // state update unconditionally. After it returns, the IRBuilder is in the
    // emitter's merge block with emit_flag as a PHI i1.
    // Gate the downstream push-to-Join on emit_flag.
    {
      bool is_stateful_so =
          (node.kind == OpKind::MovingAverage || node.kind == OpKind::StdDev    ||
           node.kind == OpKind::PeakDetector  || node.kind == OpKind::MovingSum  ||
           node.kind == OpKind::Diff          || node.kind == OpKind::SignChange  ||
           node.kind == OpKind::WinMin        || node.kind == OpKind::WinMax     ||
           node.kind == OpKind::FIR           || node.kind == OpKind::IIR        ||
           node.kind == OpKind::TimeShift     ||
           node.kind == OpKind::LessThanOrEqualToReplace ||
           node.kind == OpKind::MovingKeyCount ||
           node.kind == OpKind::FiltGtScalar  || node.kind == OpKind::FiltLtScalar ||
           node.kind == OpKind::FiltEqScalar  || node.kind == OpKind::FiltNeqScalar ||
           node.kind == OpKind::FiltGtSync    || node.kind == OpKind::FiltLtSync ||
           node.kind == OpKind::FiltEqSync    || node.kind == OpKind::FiltNeqSync);
      if (is_stateful_so) {
        auto in_conns = inputs_for(graph, op_id);
        if (in_conns.empty()) {
          throw std::runtime_error("emit_program: stateful op " + op_id + " has no input");
        }
        auto inp = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
        if (inp == value_map.end()) {
          throw std::runtime_error("emit_program: missing input for " + op_id);
        }
        std::size_t offset = layout.offsets.at(op_id);

        emit::StatefulOutput out{};
        switch (node.kind) {
          case OpKind::MovingAverage:
            out = emit::emit_moving_average(ec, offset, node.window_size,
                                            inp->second.t, inp->second.v);
            break;
          case OpKind::StdDev:
            out = emit::emit_stddev(ec, offset, node.window_size,
                                    inp->second.t, inp->second.v);
            break;
          case OpKind::PeakDetector:
            out = emit::emit_peak_detector(ec, offset, node.window_size,
                                           inp->second.t, inp->second.v);
            break;
          case OpKind::MovingSum:
            out = emit::emit_moving_sum(ec, offset, node.window_size,
                                        inp->second.t, inp->second.v);
            break;
          case OpKind::Diff:
            out = emit::emit_diff(ec, offset, /*use_oldest_time=*/true,
                                  inp->second.t, inp->second.v);
            break;
          case OpKind::SignChange:
            out = emit::emit_sign_change(ec, offset, inp->second.t, inp->second.v);
            break;
          case OpKind::WinMin:
            out = emit::emit_win_min(ec, offset, node.window_size,
                                     inp->second.t, inp->second.v);
            break;
          case OpKind::WinMax:
            out = emit::emit_win_max(ec, offset, node.window_size,
                                     inp->second.t, inp->second.v);
            break;
          case OpKind::FIR:
            out = emit::emit_fir(ec, offset, node.window_size,
                                 node.coefficients,
                                 inp->second.t, inp->second.v);
            break;
          case OpKind::IIR:
            out = emit::emit_iir(ec, offset, node.b_len, node.a_len,
                                 node.coefficients,
                                 inp->second.t, inp->second.v);
            break;
          case OpKind::TimeShift:
            out = emit::emit_time_shift(ec, node.time_shift,
                                        inp->second.t, inp->second.v);
            break;
          case OpKind::LessThanOrEqualToReplace:
            out = emit::emit_lte_replace(ec, node.replace_threshold,
                                         node.replace_by,
                                         inp->second.t, inp->second.v);
            break;
          case OpKind::MovingKeyCount:
            out = emit::emit_moving_key_count(ec, offset, node.mkc_window_size,
                                              inp->second.t, inp->second.v);
            break;
          case OpKind::FiltGtScalar:
            out = emit::emit_filt_gt_scalar(ec, node.scalar_value,
                                            inp->second.t, inp->second.v);
            break;
          case OpKind::FiltLtScalar:
            out = emit::emit_filt_lt_scalar(ec, node.scalar_value,
                                            inp->second.t, inp->second.v);
            break;
          case OpKind::FiltEqScalar:
            out = emit::emit_filt_eq_scalar(ec, node.scalar_value, node.tolerance,
                                             inp->second.t, inp->second.v);
            break;
          case OpKind::FiltNeqScalar:
            out = emit::emit_filt_neq_scalar(ec, node.scalar_value, node.tolerance,
                                              inp->second.t, inp->second.v);
            break;
          case OpKind::FiltGtSync:
          case OpKind::FiltLtSync:
          case OpKind::FiltEqSync:
          case OpKind::FiltNeqSync: {
            if (in_conns.size() < 2) {
              throw std::runtime_error("emit_program: filter sync op " + op_id +
                                       " needs 2 inputs");
            }
            auto inp2 = value_map.find({in_conns[1]->from_id, in_conns[1]->from_port});
            if (inp2 == value_map.end()) {
              throw std::runtime_error("emit_program: missing 2nd input for " + op_id);
            }
            if (node.kind == OpKind::FiltGtSync) {
              out = emit::emit_filt_gt_sync(ec, inp->second.t, inp->second.v, inp2->second.v);
            } else if (node.kind == OpKind::FiltLtSync) {
              out = emit::emit_filt_lt_sync(ec, inp->second.t, inp->second.v, inp2->second.v);
            } else if (node.kind == OpKind::FiltEqSync) {
              out = emit::emit_filt_eq_sync(ec, node.tolerance, inp->second.t,
                                             inp->second.v, inp2->second.v);
            } else {
              out = emit::emit_filt_neq_sync(ec, node.tolerance, inp->second.t,
                                              inp->second.v, inp2->second.v);
            }
            break;
          }
          default:
            break;
        }
        // IRBuilder is now in the emitter's merge block; out.emit_flag is a PHI i1.

        // AND this op's emit_flag into the program-level should-emit flag so
        // that downstream sync writes (VectorCompose direct-to-output, etc.)
        // are suppressed during warmup, matching the interpreter behaviour.
        {
          llvm::Value* cur_se = b.CreateLoad(b.getInt1Ty(), should_emit_alloca,
                                              "se_cur");
          b.CreateStore(b.CreateAnd(cur_se, out.emit_flag, "se_and"),
                        should_emit_alloca);
        }

        llvm::BasicBlock* bb_sfds = llvm::BasicBlock::Create(ctx, "sf_ds",   fn);
        llvm::BasicBlock* bb_sfc  = llvm::BasicBlock::Create(ctx, "sf_cont", fn);
        b.CreateCondBr(out.emit_flag, bb_sfds, bb_sfc);

        b.SetInsertPoint(bb_sfds);
        value_map[{op_id, 0}] = {out.out_t, out.out_v};
        for (auto [join_id, to_port] : join_successors(op_id, 0)) {
          std::size_t jo = layout.offsets.at(join_id);
          std::size_t jN = join_n_ports.at(join_id);
          emit::emit_join_push(ec, jo, jN, to_port, out.out_t, out.out_v);
        }
        b.CreateBr(bb_sfc);

        b.SetInsertPoint(bb_sfc);
        continue;
      }
    }

    // --- Always-emit aggregates: CumSum, Count, MaxAgg, MinAgg --------------
    {
      bool is_aggregate = (node.kind == OpKind::CumSum || node.kind == OpKind::Count ||
                           node.kind == OpKind::MaxAgg || node.kind == OpKind::MinAgg);
      if (is_aggregate) {
        auto in_conns = inputs_for(graph, op_id);
        std::size_t offset = layout.offsets.at(op_id);
        llvm::Value* inp_v = nullptr;
        llvm::Value* inp_t = nullptr;
        if (!in_conns.empty()) {
          auto it = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
          if (it != value_map.end()) { inp_v = it->second.v; inp_t = it->second.t; }
        }
        llvm::Value* out_v = nullptr;
        switch (node.kind) {
          case OpKind::CumSum:  out_v = emit::emit_cumsum(ec, offset, inp_v); break;
          case OpKind::Count:   out_v = emit::emit_count(ec, offset);         break;
          case OpKind::MaxAgg:  out_v = emit::emit_max_agg(ec, offset, inp_v); break;
          case OpKind::MinAgg:  out_v = emit::emit_min_agg(ec, offset, inp_v); break;
          default:              break;
        }
        value_map[{op_id, 0}] = {inp_t, out_v};
        for (auto [join_id, to_port] : join_successors(op_id, 0)) {
          std::size_t jo = layout.offsets.at(join_id);
          std::size_t jN = join_n_ports.at(join_id);
          emit::emit_join_push(ec, jo, jN, to_port, inp_t, out_v);
        }
        continue;
      }
    }

    // --- StateLoad: read source op's first state slot ---
    if (node.kind == OpKind::StateLoad) {
      auto src_it = layout.offsets.find(node.state_source_id);
      if (src_it == layout.offsets.end()) {
        throw std::runtime_error(
            "emit_program: StateLoad op " + op_id +
            " references unknown source op " + node.state_source_id);
      }
      llvm::Value* loaded = emit::emit_state_load(ec, src_it->second);
      // Retrieve timestamp from a connected input, fall back to the Input op value.
      auto in_conns_sl = inputs_for(graph, op_id);
      llvm::Value* sl_t = nullptr;
      if (!in_conns_sl.empty()) {
        auto it = value_map.find({in_conns_sl[0]->from_id, in_conns_sl[0]->from_port});
        if (it != value_map.end()) sl_t = it->second.t;
      }
      // StateLoad has no data connections, so we look for the source op's time.
      if (!sl_t) {
        auto it = value_map.find({node.state_source_id, 0});
        if (it != value_map.end()) sl_t = it->second.t;
      }
      // Final fallback: use i64 zero (should not occur in valid graphs).
      if (!sl_t) sl_t = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
      value_map[{op_id, 0}] = {sl_t, loaded};
      for (auto [join_id, to_port] : join_successors(op_id, 0)) {
        std::size_t jo = layout.offsets.at(join_id);
        std::size_t jN = join_n_ports.at(join_id);
        emit::emit_join_push(ec, jo, jN, to_port, sl_t, loaded);
      }
      continue;
    }

    // --- FusedExpressionVector: stateful 1->1 vector-input bytecode walker --
    // Reads its single input from the upstream vector wire (value_map),
    // extracts each lane as a scalar SSA, walks the FE bytecode against the
    // resulting inputs (INPUT k => lane k), then builds an SSA [M x double]
    // output vector. Subsequent ops in this segment are emitted inside the
    // emit branch so they only run on emit_flag = true (mirrors the FE-sync
    // non-Output path).
    if (node.kind == OpKind::FusedExpressionVector) {
      auto in_conns = inputs_for(graph, op_id);
      if (in_conns.empty()) {
        throw std::runtime_error(
            "emit_program: FusedExpressionVector '" + op_id +
            "' has no input");
      }
      auto inp = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
      if (inp == value_map.end()) {
        throw std::runtime_error(
            "emit_program: missing input for FusedExpressionVector " + op_id);
      }
      const std::size_t W = inp->second.width;
      if (W <= 1 || !inp->second.v->getType()->isVectorTy()) {
        throw std::runtime_error(
            "emit_program: FusedExpressionVector '" + op_id +
            "' input is not a vector wire");
      }

      // Compute the minimum INPUT lane this bytecode references; require
      // upstream wire is at least that wide.
      std::size_t min_inputs = 0;
      {
        auto pack_min = rtbot::fuse::pack_bytecode(node.fe_bytecode);
        for (const auto& ins : pack_min.packed) {
          if (ins.op == static_cast<std::uint8_t>(rtbot::fused_op::INPUT)) {
            const std::size_t need = static_cast<std::size_t>(ins.arg) + 1;
            if (need > min_inputs) min_inputs = need;
          }
        }
      }
      if (min_inputs > W) {
        throw std::runtime_error(
            "emit_program: FusedExpressionVector '" + op_id +
            "' bytecode INPUT index out of bounds for vector width " +
            std::to_string(W));
      }

      // Pre-extract each upstream lane into a scalar SSA. The bytecode walker
      // reads INPUT k as inputs[k]; same shape as the scalar-port FE path.
      std::vector<llvm::Value*> lane_values;
      lane_values.reserve(W);
      llvm::Type* i32t = llvm::Type::getInt32Ty(ctx);
      for (std::size_t k = 0; k < W; ++k) {
        llvm::Value* el = b.CreateExtractElement(
            inp->second.v,
            llvm::ConstantInt::get(i32t, static_cast<std::uint32_t>(k)),
            "fev_lane");
        lane_values.push_back(el);
      }

      const std::size_t M = node.fe_num_outputs;
      const std::size_t bc_state_off = layout.offsets.at(op_id);

      emit::FusedExprOutput feo = emit::emit_fused_expression(
          ec, bc_state_off,
          node.fe_bytecode, node.fe_constants, node.fe_coefficients,
          lane_values, M);

      // Branch on combined emit_flag. Subsequent segment ops live inside
      // fev_emit_bb so they only run when this tick emits a value.
      llvm::BasicBlock* fev_emit_bb = llvm::BasicBlock::Create(ctx, "fev_emit_bb", fn);
      llvm::BasicBlock* fev_skip_bb = llvm::BasicBlock::Create(ctx, "fev_skip_bb", fn);
      b.CreateCondBr(feo.emit_flag, fev_emit_bb, fev_skip_bb);

      b.SetInsertPoint(fev_emit_bb);
      llvm::Type* dty = llvm::Type::getDoubleTy(ctx);
      llvm::Type* vty = llvm::VectorType::get(
          dty, llvm::ElementCount::getFixed(static_cast<unsigned>(M)));
      llvm::Value* out_vec = llvm::UndefValue::get(vty);
      for (std::size_t k = 0; k < M; ++k) {
        out_vec = b.CreateInsertElement(
            out_vec, feo.out_vs[k],
            llvm::ConstantInt::get(i32t, static_cast<std::uint32_t>(k)),
            "fev_ins");
      }
      value_map[{op_id, 0}] = {inp->second.t, out_vec, /*width=*/M};

      // Recurse on remaining ops in this segment so they only run on emit.
      const std::size_t cur_idx = static_cast<std::size_t>(
          &op_id - op_ids.data());
      std::vector<std::string> rest(op_ids.begin() + cur_idx + 1,
                                     op_ids.end());
      emit_segment_ops(
          ec, b, ctx, fn, graph, layout, rest,
          output_op_id, num_outputs,
          arg_out_t, arg_out_v, arg_out_port_id,
          bb_ret_true, bb_ret_false, should_emit_alloca,
          value_map, join_ids, join_n_ports, join_successors);

      if (!b.GetInsertBlock()->getTerminator()) {
        b.CreateBr(fev_skip_bb);
      }
      b.SetInsertPoint(fev_skip_bb);
      // The caller will append a br to its merge_bb (or to bb_ret_false if no
      // further sync ops remain) when fev_skip_bb has no terminator.
      return;
    }

    // --- BurstAggregate: stateful 1->{0,1} segmented aggregator -------------
    // Reads its single input from the upstream vector wire (value_map),
    // walks the seg_bytecode to detect segment-predicate transitions, and
    // emits the previous segment's last valid aggregate (with the current
    // row's key columns prepended) at the transitioning row's timestamp.
    // Always updates the agg state from the current row, regardless of
    // emission. Subsequent ops in this segment run inside the emit branch
    // so they only fire on a transition with a banked aggregate output.
    if (node.kind == OpKind::BurstAggregate) {
      auto in_conns = inputs_for(graph, op_id);
      if (in_conns.empty()) {
        throw std::runtime_error(
            "emit_program: BurstAggregate '" + op_id + "' has no input");
      }
      auto inp = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
      if (inp == value_map.end()) {
        throw std::runtime_error(
            "emit_program: missing input for BurstAggregate " + op_id);
      }
      // Accept both scalar wires (width <= 1, single SSA double) and vector
      // wires (width N, [N x double]) so the operator works uniformly when
      // the program-level Input is declared as numInputCols == 1 (scalar
      // signature) or as a wider VECTOR_NUMBER.
      llvm::Type* i32t = llvm::Type::getInt32Ty(ctx);
      llvm::Type* dty  = llvm::Type::getDoubleTy(ctx);
      const bool is_scalar_input =
          inp->second.width <= 1 || !inp->second.v->getType()->isVectorTy();
      const std::size_t W = is_scalar_input ? 1 : inp->second.width;
      if (W < node.ba_num_input_cols) {
        throw std::runtime_error(
            "emit_program: BurstAggregate '" + op_id +
            "' numInputCols " + std::to_string(node.ba_num_input_cols) +
            " exceeds upstream wire width " + std::to_string(W));
      }

      // Pre-extract every upstream lane the agg / seg bytecode might read.
      // Scalar wires unwrap directly; vector wires use ExtractElement per lane.
      std::vector<llvm::Value*> lane_values;
      lane_values.reserve(W);
      if (is_scalar_input) {
        lane_values.push_back(inp->second.v);
      } else {
        for (std::size_t k = 0; k < W; ++k) {
          llvm::Value* el = b.CreateExtractElement(
              inp->second.v,
              llvm::ConstantInt::get(i32t, static_cast<std::uint32_t>(k)),
              "ba_lane");
          lane_values.push_back(el);
        }
      }

      // State layout (matches StateLayout::state_size_for):
      //   [agg_state ...]            agg_state_size
      //   has_seg_value              1
      //   last_seg_value             1
      //   last_key_values[K]         K
      //   has_valid_output           1
      //   last_valid_output[M]       M
      const std::size_t base = layout.offsets.at(op_id);
      auto agg_pack =
          rtbot::fuse::pack_bytecode(node.ba_agg_bytecode);
      const auto agg_fl = rtbot::fuse::compute_state_layout(
          agg_pack.packed, agg_pack.aux_args);
      const std::size_t agg_state_size = agg_fl.total_state_size;
      const std::size_t off_has_seg     = base + agg_state_size;
      const std::size_t off_last_seg    = off_has_seg + 1;
      const std::size_t off_last_keys   = off_last_seg + 1;
      const std::size_t K               = node.ba_key_columns.size();
      const std::size_t off_has_valid   = off_last_keys + K;
      const std::size_t off_last_valid  = off_has_valid + 1;
      const std::size_t M               = node.ba_num_agg_outputs;

      // Bake agg_state init pattern as a private constant array so we can
      // memcpy it into the agg_state region on segment transition. The init
      // pattern combines compute_state_layout's initial_values with
      // pack.state_init (the latter overrides where non-zero).
      llvm::Value* memcpy_dst = nullptr;
      llvm::Value* memcpy_src = nullptr;
      llvm::Value* memcpy_len = nullptr;
      if (agg_state_size > 0) {
        std::vector<double> init_vals(agg_state_size, 0.0);
        for (std::size_t k = 0; k < agg_fl.initial_values.size() &&
                                  k < agg_state_size; ++k) {
          init_vals[k] = agg_fl.initial_values[k];
        }
        for (std::size_t k = 0; k < agg_pack.state_init.size() &&
                                  k < agg_state_size; ++k) {
          if (agg_pack.state_init[k] != 0.0) {
            init_vals[k] = agg_pack.state_init[k];
          }
        }
        auto* arr_ty = llvm::ArrayType::get(dty, agg_state_size);
        std::vector<llvm::Constant*> consts;
        consts.reserve(agg_state_size);
        for (double v : init_vals) {
          consts.push_back(llvm::ConstantFP::get(dty, v));
        }
        auto* init_const = llvm::ConstantArray::get(arr_ty, consts);
        auto* init_global = new llvm::GlobalVariable(
            *fn->getParent(), arr_ty, /*isConstant=*/true,
            llvm::GlobalValue::PrivateLinkage, init_const,
            "ba_agg_init_" + op_id);
        memcpy_src = init_global;
        memcpy_dst = ec.state_gep(base);
        memcpy_len = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(ctx),
            static_cast<std::uint64_t>(agg_state_size * sizeof(double)));
      }

      // Helper to load a slot.
      auto load_slot = [&](std::size_t off, const char* name) -> llvm::Value* {
        return b.CreateLoad(dty, ec.state_gep(off), name);
      };
      auto store_slot = [&](std::size_t off, llvm::Value* v) {
        b.CreateStore(v, ec.state_gep(off));
      };

      // Snapshot the previous-row state before any updates. These are the
      // values we'll need for emission (when transition fires) and for
      // detecting the transition itself.
      llvm::Value* prev_has_seg = load_slot(off_has_seg, "ba_phs");
      llvm::Value* prev_last_seg = load_slot(off_last_seg, "ba_pls");
      llvm::Value* prev_has_valid = load_slot(off_has_valid, "ba_phv");
      std::vector<llvm::Value*> prev_last_valid;
      prev_last_valid.reserve(M);
      for (std::size_t j = 0; j < M; ++j) {
        prev_last_valid.push_back(
            load_slot(off_last_valid + j, "ba_plv"));
      }

      // Compute the new key values and write them through to state. The
      // emitted output uses the current row's keys (matching FE BurstAggregate
      // semantics: keys are constant within a segment by construction).
      std::vector<llvm::Value*> new_keys;
      new_keys.reserve(K);
      for (std::size_t k = 0; k < K; ++k) {
        const std::size_t col = node.ba_key_columns[k];
        if (col >= W) {
          throw std::runtime_error(
              "emit_program: BurstAggregate '" + op_id +
              "' keyColumns entry " + std::to_string(col) +
              " out of bounds for upstream vector width " +
              std::to_string(W));
        }
        new_keys.push_back(lane_values[col]);
        store_slot(off_last_keys + k, lane_values[col]);
      }

      // Walk the segment-predicate bytecode (when present). Empty bytecode
      // means "never transition" — skip everything below and just update
      // the agg state.
      llvm::Value* seg_val = nullptr;
      llvm::Value* transition = llvm::ConstantInt::getFalse(ctx);
      if (!node.ba_seg_bytecode.empty()) {
        seg_val = walk_segment_bytecode(
            ec, node.ba_seg_bytecode, node.ba_seg_constants, lane_values);
        llvm::Value* zero = llvm::ConstantFP::get(dty, 0.0);
        llvm::Value* prev_has_seg_nz =
            b.CreateFCmpONE(prev_has_seg, zero, "ba_phs_nz");
        llvm::Value* seg_diff =
            b.CreateFCmpONE(seg_val, prev_last_seg, "ba_seg_diff");
        transition = b.CreateAnd(prev_has_seg_nz, seg_diff, "ba_trans");
      }
      llvm::Value* prev_has_valid_nz =
          b.CreateFCmpONE(prev_has_valid,
                          llvm::ConstantFP::get(dty, 0.0), "ba_phv_nz");
      llvm::Value* emit_flag =
          b.CreateAnd(transition, prev_has_valid_nz, "ba_emit");

      // On transition: memcpy the baked init pattern over the agg state slots.
      // Always runs (independent of has_valid_output) so the new segment
      // starts from the configured init values.
      llvm::BasicBlock* ba_reset_bb =
          llvm::BasicBlock::Create(ctx, "ba_reset_bb", fn);
      llvm::BasicBlock* ba_post_reset_bb =
          llvm::BasicBlock::Create(ctx, "ba_post_reset_bb", fn);
      b.CreateCondBr(transition, ba_reset_bb, ba_post_reset_bb);

      b.SetInsertPoint(ba_reset_bb);
      if (memcpy_dst != nullptr) {
        b.CreateMemCpy(memcpy_dst, llvm::MaybeAlign(8),
                       memcpy_src, llvm::MaybeAlign(8), memcpy_len);
      }
      b.CreateBr(ba_post_reset_bb);

      b.SetInsertPoint(ba_post_reset_bb);

      // Walk the aggregate bytecode against the (possibly reset) agg state.
      // Use the FE bytecode walker — agg-state lives at the node's base.
      emit::FusedExprOutput ag = emit::emit_fused_expression(
          ec, base, node.ba_agg_bytecode, node.ba_agg_constants,
          /*coefficients=*/{}, lane_values, M);
      if (ag.out_vs.size() != M) {
        throw std::runtime_error(
            "emit_program: BurstAggregate '" + op_id +
            "' agg bytecode produced " + std::to_string(ag.out_vs.size()) +
            " outputs, expected " + std::to_string(M));
      }

      // Stash the new agg outputs into last_valid_output.
      for (std::size_t j = 0; j < M; ++j) {
        store_slot(off_last_valid + j, ag.out_vs[j]);
      }
      store_slot(off_has_valid, llvm::ConstantFP::get(dty, 1.0));

      // Update last_seg_value / has_seg_value when seg bytecode is non-empty.
      if (!node.ba_seg_bytecode.empty() && seg_val != nullptr) {
        store_slot(off_last_seg, seg_val);
        store_slot(off_has_seg, llvm::ConstantFP::get(dty, 1.0));
      }

      // Build the output vector [keys..., prev_last_valid...] and recurse on
      // the remaining segment ops inside the emit branch. Output writes
      // arg_out_v and branches to bb_ret_true; the skip branch falls through
      // to caller (which terminates with br ret_false).
      llvm::BasicBlock* ba_emit_bb =
          llvm::BasicBlock::Create(ctx, "ba_emit_bb", fn);
      llvm::BasicBlock* ba_skip_bb =
          llvm::BasicBlock::Create(ctx, "ba_skip_bb", fn);
      b.CreateCondBr(emit_flag, ba_emit_bb, ba_skip_bb);

      b.SetInsertPoint(ba_emit_bb);
      const std::size_t out_w = K + M;
      llvm::Type* vty = llvm::VectorType::get(
          dty, llvm::ElementCount::getFixed(static_cast<unsigned>(out_w)));
      llvm::Value* out_vec = llvm::UndefValue::get(vty);
      for (std::size_t k = 0; k < K; ++k) {
        out_vec = b.CreateInsertElement(
            out_vec, new_keys[k],
            llvm::ConstantInt::get(i32t, static_cast<std::uint32_t>(k)),
            "ba_ins_k");
      }
      for (std::size_t j = 0; j < M; ++j) {
        out_vec = b.CreateInsertElement(
            out_vec, prev_last_valid[j],
            llvm::ConstantInt::get(i32t, static_cast<std::uint32_t>(K + j)),
            "ba_ins_v");
      }
      value_map[{op_id, 0}] = {inp->second.t, out_vec, /*width=*/out_w};

      // Recurse on remaining ops in this segment so they only run on emit.
      const std::size_t cur_idx = static_cast<std::size_t>(
          &op_id - op_ids.data());
      std::vector<std::string> rest(op_ids.begin() + cur_idx + 1,
                                     op_ids.end());
      emit_segment_ops(
          ec, b, ctx, fn, graph, layout, rest,
          output_op_id, num_outputs,
          arg_out_t, arg_out_v, arg_out_port_id,
          bb_ret_true, bb_ret_false, should_emit_alloca,
          value_map, join_ids, join_n_ports, join_successors);

      if (!b.GetInsertBlock()->getTerminator()) {
        b.CreateBr(ba_skip_bb);
      }
      b.SetInsertPoint(ba_skip_bb);
      return;
    }

    // --- TopK: terminal multi-emit op (writes records and returns count) ----
    if (node.kind == OpKind::TopK) {
      auto in_conns = inputs_for(graph, op_id);
      if (in_conns.empty()) {
        throw std::runtime_error("emit_program: TopK op " + op_id + " has no input");
      }
      auto inp = value_map.find({in_conns[0]->from_id, in_conns[0]->from_port});
      if (inp == value_map.end()) {
        throw std::runtime_error("emit_program: missing input for TopK " + op_id);
      }
      const std::size_t W = (node.topk_row_width == 0) ? 1 : node.topk_row_width;

      // Build the per-lane SSA scalars from the upstream PortValue. Scalar
      // wires (width <= 1) yield a single-lane vector.
      std::vector<llvm::Value*> lanes;
      lanes.reserve(W);
      if (inp->second.width <= 1) {
        if (W != 1) {
          throw std::runtime_error(
              "emit_program: TopK '" + op_id +
              "' row_width > 1 but upstream wire is scalar");
        }
        lanes.push_back(inp->second.v);
      } else {
        if (inp->second.width != W) {
          throw std::runtime_error(
              "emit_program: TopK '" + op_id + "' row_width " +
              std::to_string(W) + " does not match upstream vector width " +
              std::to_string(inp->second.width));
        }
        llvm::Type* i32t = llvm::Type::getInt32Ty(ctx);
        for (std::size_t k = 0; k < W; ++k) {
          llvm::Value* el = b.CreateExtractElement(
              inp->second.v,
              llvm::ConstantInt::get(i32t, static_cast<std::uint32_t>(k)),
              "tk_in_lane");
          lanes.push_back(el);
        }
      }

      // Determine the Output port id this TopK's emission lands on. Default 0.
      std::size_t out_port_id = 0;
      for (const auto& conn : graph.connections) {
        if (conn.from_id == op_id && conn.to_id == output_op_id &&
            conn.from_port == 0) {
          out_port_id = conn.to_port;
          break;
        }
      }

      const std::size_t offset = layout.offsets.at(op_id);
      llvm::Value* count = emit::emit_topk(
          ec, offset, node.topk_k, W, node.topk_score_index,
          node.topk_descending, inp->second.t, lanes,
          arg_out_t, arg_out_v, arg_out_port_id, num_outputs, out_port_id);

      // Return count. TopK is terminal; ignore any subsequent ops in this
      // op_ids list (Output is bypassed — its slot mapping is encoded by
      // out_port_id and num_outputs already).
      b.CreateRet(count);
      return;
    }

    throw std::runtime_error("emit_program: unhandled op kind for op " + op_id);
  }
}

// Internal: emit a program function whose Input op produces a width-N vector
// wire. When input_lane_width_N == 1, the signature is the historical scalar
// (state, t, double v, ...). When > 1, the signature is
// (state, t, const double* in_v_arr, ...) and the entry loads N doubles from
// in_v_arr to build the wire that seeds value_map[{Input, 0}].
EmittedSegment emit_program_with_input_width(
    llvm::LLVMContext& ctx, llvm::Module& mod, const CompiledGraph& graph,
    const StateLayout& layout, std::size_t input_lane_width_N);

EmittedSegment emit_program(llvm::LLVMContext& ctx, llvm::Module& mod,
                             const CompiledGraph& graph,
                             const StateLayout& layout) {
  return emit_program_with_input_width(ctx, mod, graph, layout, 1);
}

EmittedSegment emit_program_with_input_width(
    llvm::LLVMContext& ctx, llvm::Module& mod, const CompiledGraph& graph,
    const StateLayout& layout, std::size_t input_lane_width_N) {
  // -------------------------------------------------------------------------
  // Validate: only Join sync ops; no Resampler mixed with Joins.
  // -------------------------------------------------------------------------
  std::string input_op_id;
  std::string output_op_id;
  bool has_resampler = false;

  for (const auto& node : graph.nodes) {
    if (node.kind == OpKind::Input)            input_op_id  = node.id;
    if (node.kind == OpKind::Output)           output_op_id = node.id;
    if (node.kind == OpKind::ResamplerHermite ||
        node.kind == OpKind::ResamplerConstant) has_resampler = true;
  }

  if (input_op_id.empty()) {
    throw std::runtime_error("emit_program: graph has no Input op");
  }
  if (output_op_id.empty()) {
    throw std::runtime_error("emit_program: graph has no Output op");
  }
  if (has_resampler) {
    throw std::runtime_error(
        "emit_program: graphs combining ResamplerHermite and Join are not yet supported");
  }

  // The JIT segment_fn signature exposes a single scalar input v on every tick
  // (or a width-N vector via in_v_arr when input_lane_width_N > 1). Either way
  // there is exactly one Input port; reject multi-port Inputs so Program
  // falls back to the interpreter.
  for (const auto& node : graph.nodes) {
    if (node.kind == OpKind::Input && node.port_types.size() > 1) {
      throw std::runtime_error(
          "emit_program: multi-port Input op '" + node.id +
          "' is not supported by the JIT (Program falls back to interpreter)");
    }
  }

  const std::size_t num_outputs = compute_num_outputs(graph, output_op_id);

  // -------------------------------------------------------------------------
  // Partition into segments and sync ops.
  // -------------------------------------------------------------------------
  PartitionResult partition = partition_segments(graph);

  // -------------------------------------------------------------------------
  // Build join metadata: set of join ids and per-join port count.
  // -------------------------------------------------------------------------
  std::set<std::string> join_ids;
  std::unordered_map<std::string, std::size_t> join_n_ports;

  for (const auto& sync_op_id : partition.sync_ops) {
    const OpNode& jnode = find_node(graph, sync_op_id);
    if (jnode.kind != OpKind::Join &&
        jnode.kind != OpKind::Linear &&
        jnode.kind != OpKind::ReduceJoin &&
        jnode.kind != OpKind::Demux &&
        jnode.kind != OpKind::Mux &&
        jnode.kind != OpKind::VectorCompose &&
        jnode.kind != OpKind::FusedExpression &&
        jnode.kind != OpKind::Pipeline &&
        jnode.kind != OpKind::KeyedPipeline) {
      throw std::runtime_error(
          "emit_program: unsupported sync op '" + sync_op_id + "'");
    }
    join_ids.insert(sync_op_id);
    // Prefer the parser-supplied count when available; fall back to the
    // largest to_port observed on the connections.
    std::size_t n = jnode.join_num_ports;
    if (n == 0 && jnode.kind == OpKind::Linear) {
      n = jnode.coefficients.size();
    }
    if (jnode.kind == OpKind::FusedExpression && n == 0) {
      n = jnode.fe_num_ports;
    }
    if (jnode.kind == OpKind::Demux || jnode.kind == OpKind::Mux) {
      n = jnode.mux_num_ports;
      if (n == 0) {
        // Fall back to inferring N from the largest control to_port + 1.
        for (const auto& conn : graph.connections) {
          if (conn.to_id == sync_op_id && conn.to_kind == PortKind::Control) {
            n = std::max(n, conn.to_port + 1);
          }
        }
      }
      if (n == 0) n = 1;
    }
    if (jnode.kind == OpKind::Pipeline) {
      // Pipeline data ports + (control port iff control-port mode). The flat
      // sync port count drives try_sync over the combined (data, control)
      // queues so timestamps align before we read the segment key. In
      // segment-bytecode mode the single VECTOR_NUMBER input port unrolls
      // into pipeline_input_lane_width flat ports (one per lane) so the
      // upstream sync op fans the vector across N consecutive port queues.
      if (!jnode.pipeline_segment_bytecode.empty()) {
        n = jnode.pipeline_input_lane_width;
        if (n == 0) n = 1;
      } else {
        n = jnode.pipeline_input_port_types.size();
        if (n == 0) n = 1;
        ++n;  // control port
      }
    }
    if (jnode.kind == OpKind::KeyedPipeline) {
      // KeyedPipeline always has a single VECTOR_NUMBER input port; the lane
      // count drives the queue layout (one queue per lane).
      n = jnode.keyed_pipeline_input_lane_width;
      if (n == 0) n = 1;
    }
    if (n == 0) {
      for (const auto& conn : graph.connections) {
        if (conn.to_id == sync_op_id) n = std::max(n, conn.to_port + 1);
      }
    }
    join_n_ports[sync_op_id] = (n > 0) ? n : 2;
  }

  // For Demux/Mux: map (to_kind, to_port) of a connection to the flat port
  // index used by the per-port ring buffer state. Demux: data port -> 0,
  // control_p -> 1+p. Mux: data_p -> p, control_p -> N+p.
  auto sync_flat_port = [&](const OpNode& sync_node, PortKind kind,
                             std::size_t to_port) -> std::size_t {
    if (sync_node.kind == OpKind::Demux) {
      if (kind == PortKind::Data) return 0;
      return 1 + to_port;
    }
    if (sync_node.kind == OpKind::Mux) {
      std::size_t N = sync_node.mux_num_ports;
      if (kind == PortKind::Data) return to_port;
      return N + to_port;
    }
    if (sync_node.kind == OpKind::Pipeline) {
      // Data ports occupy 0..N-1; the control port (when present) is at N.
      if (kind == PortKind::Data) return to_port;
      std::size_t N = sync_node.pipeline_input_port_types.size();
      if (N == 0) N = 1;
      return N;
    }
    if (sync_node.kind == OpKind::KeyedPipeline) {
      // Single VECTOR_NUMBER data port unrolls into one flat port per lane;
      // upstream lane k feeds flat port k. Control ports are not used.
      return to_port;
    }
    // Join/Linear/ReduceJoin/VectorCompose/FusedExpression: data ports only;
    // flat index == to_port.
    return to_port;
  };

  // join_successors: for a given (op_id, from_port), returns all
  // (join_id, flat_port) pairs this op connects to. flat_port is suitable for
  // emit_join_push regardless of sync op kind.
  auto join_successors_fn = [&](const std::string& op_id, std::size_t from_port)
      -> std::vector<std::pair<std::string, std::size_t>> {
    std::vector<std::pair<std::string, std::size_t>> result;
    for (const auto& conn : graph.connections) {
      if (conn.from_id == op_id && conn.from_port == from_port) {
        if (join_ids.count(conn.to_id)) {
          const OpNode& sync_node = find_node(graph, conn.to_id);
          std::size_t flat = sync_flat_port(sync_node, conn.to_kind, conn.to_port);
          result.push_back({conn.to_id, flat});
        }
      }
    }
    return result;
  };

  // -------------------------------------------------------------------------
  // Build function.
  // -------------------------------------------------------------------------
  static int prog_fn_counter = 0;
  std::string fn_name = "program_process_" + std::to_string(prog_fn_counter++);

  llvm::Type* f64  = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64  = llvm::Type::getInt64Ty(ctx);
  llvm::Type* i32  = llvm::Type::getInt32Ty(ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);
  llvm::Type* i64p = llvm::PointerType::getUnqual(i64);
  llvm::Type* i32p = llvm::PointerType::getUnqual(i32);

  // Function signature: scalar input (N==1) keeps `double v`; vector input
  // (N>1) uses a `const double*` pointer to N consecutive lane values.
  const bool vector_input = (input_lane_width_N > 1);
  llvm::Type* input_arg_ty = vector_input ? f64p : f64;
  llvm::FunctionType* fn_ty = llvm::FunctionType::get(
      i32, {f64p, i64, input_arg_ty, i64p, f64p, i32p}, false);

  llvm::Function* fn = llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, fn_name, mod);

  auto ai = fn->arg_begin();
  llvm::Argument* arg_state       = &*ai++;  arg_state->setName("state");
  llvm::Argument* arg_t           = &*ai++;  arg_t->setName("t");
  llvm::Argument* arg_v           = &*ai++;  arg_v->setName(vector_input ? "in_v_arr" : "v");
  llvm::Argument* arg_out_t       = &*ai++;  arg_out_t->setName("out_t_arr");
  llvm::Argument* arg_out_v       = &*ai++;  arg_out_v->setName("out_v_arr");
  llvm::Argument* arg_out_port_id = &*ai++;  arg_out_port_id->setName("out_port_id_arr");

  llvm::IRBuilder<> b(ctx);
  b.setFastMathFlags(llvm::FastMathFlags{});

  llvm::BasicBlock* bb_entry     = llvm::BasicBlock::Create(ctx, "entry",     fn);
  llvm::BasicBlock* bb_ret_false = llvm::BasicBlock::Create(ctx, "ret_false", fn);
  llvm::BasicBlock* bb_ret_true  = llvm::BasicBlock::Create(ctx, "ret_true",  fn);

  b.SetInsertPoint(bb_ret_false);
  b.CreateRet(llvm::ConstantInt::get(i32, 0));

  b.SetInsertPoint(bb_entry);

  // Default slot 0's port id to 0; multi-emit Demux/Mux paths handle their
  // own per-record writes.
  b.CreateStore(llvm::ConstantInt::get(i32, 0), arg_out_port_id);

  IrEmissionContext ec(ctx, mod, b, arg_state);
  (void)i32p;
  (void)arg_out_port_id;

  // Alloca for the program-level Gate suppression flag.
  // Initialized to true; Gate ops AND it with (predicate != 0.0).
  // The Output op checks it before writing results.
  llvm::Value* al_should_emit_prog =
      b.CreateAlloca(b.getInt1Ty(), nullptr, "al_should_emit");
  b.CreateStore(llvm::ConstantInt::getTrue(ctx), al_should_emit_prog);

  // Accumulators for VectorCompose direct-to-Output (case a) multi-terminal
  // flush: tracks whether any VC case-a wrote this tick and the emit timestamp.
  llvm::Value* al_vc_wrote =
      b.CreateAlloca(b.getInt1Ty(), nullptr, "al_vc_wrote");
  b.CreateStore(llvm::ConstantInt::getFalse(ctx), al_vc_wrote);
  llvm::Value* al_vc_out_t = b.CreateAlloca(i64, nullptr, "al_vc_out_t");
  b.CreateStore(llvm::ConstantInt::get(i64, 0), al_vc_out_t);

  ValueMap value_map;

  // Seed Input into value_map. Scalar input goes in directly; vector input
  // is loaded lane-by-lane from in_v_arr and assembled into an [N x double]
  // SSA so downstream ops (VectorExtract / VectorProject / FE-vector / ...)
  // can read it like any other vector wire.
  if (!vector_input) {
    value_map[{input_op_id, 0}] = {arg_t, arg_v};
  } else {
    llvm::Type* vty = llvm::VectorType::get(
        f64, llvm::ElementCount::getFixed(
                 static_cast<unsigned>(input_lane_width_N)));
    llvm::Value* vec = llvm::UndefValue::get(vty);
    for (std::size_t k = 0; k < input_lane_width_N; ++k) {
      llvm::Value* slot = b.CreateGEP(
          f64, arg_v,
          llvm::ConstantInt::get(i64, static_cast<int64_t>(k)),
          "in_v_slot");
      llvm::Value* lane = b.CreateLoad(f64, slot, "in_v_lane");
      vec = b.CreateInsertElement(
          vec, lane,
          llvm::ConstantInt::get(i32, static_cast<std::uint32_t>(k)),
          "in_v_ins");
    }
    value_map[{input_op_id, 0}] = {arg_t, vec, /*width=*/input_lane_width_N};
  }

  // -------------------------------------------------------------------------
  // Emit segments interleaved with Join sync blocks.
  //
  // The structure for PPG:
  //
  //   emit segment[0] ops (Input, ma_short, ma_long) — with pushes to join_ma
  //   emit try_sync(join_ma) unconditionally
  //   CondBr(sync_flag_ma, bb_jma_then, bb_jma_merge)
  //   bb_jma_then:
  //     populate value_map for join_ma outputs
  //     push join_ma outputs to any downstream joins
  //     emit segment[1] ops (minus, peak) — with pushes to join_out
  //     br bb_jma_merge
  //   bb_jma_merge:
  //     emit try_sync(join_out) unconditionally
  //     CondBr(sync_flag_out, bb_jout_then, bb_ret_false)
  //     bb_jout_then:
  //       populate value_map for join_out outputs
  //       emit segment[2] ops (output) — writes outputs, br ret_true
  //     bb_ret_false: ret false
  //
  // In general, for K sync ops and K+1 segments:
  //   emit segment[0]
  //   for i in [0, K):
  //     emit try_sync(sync_ops[i])
  //     CondBr(sync_flag, then_bb, merge_bb)
  //     then_bb:
  //       populate value_map
  //       emit segment[i+1]  ← nested inside then_bb
  //       br merge_bb
  //     merge_bb:  ← becomes current insertion point
  //   final merge_bb → br ret_false
  //
  // Note: the LAST sync op's merge_bb → ret_false (because if the final Join
  // doesn't sync, we can't produce output). For earlier sync ops, the merge_bb
  // continues to the NEXT sync op's try_sync (which runs unconditionally).
  // -------------------------------------------------------------------------

  // Emit segment[0].
  emit_segment_ops(
      ec, b, ctx, fn, graph, layout,
      partition.segments.empty() ? std::vector<std::string>{} : partition.segments[0].op_ids,
      output_op_id, num_outputs,
      arg_out_t, arg_out_v, arg_out_port_id,
      bb_ret_true, bb_ret_false, al_should_emit_prog,
      value_map, join_ids, join_n_ports, join_successors_fn);

  // For each sync op: emit try_sync, gate next segment inside then_bb.
  for (std::size_t si = 0; si < partition.sync_ops.size(); ++si) {
    const std::string& sync_id = partition.sync_ops[si];
    const OpNode&      snode   = find_node(graph, sync_id);
    std::size_t        offset  = layout.offsets.at(sync_id);
    std::size_t        N       = join_n_ports.at(sync_id);

    // ---- Demultiplexer: multi-record dispatch ----
    if (snode.kind == OpKind::Demux) {
      emit::DemuxTrySyncOutput dso = emit::emit_demux_try_sync(ec, offset, N);

      llvm::BasicBlock* dmx_emit_bb  = llvm::BasicBlock::Create(ctx, "dmx_emit_bb", fn);
      llvm::BasicBlock* dmx_merge_bb = llvm::BasicBlock::Create(ctx, "dmx_merge_bb", fn);
      llvm::BasicBlock* dmx_ret_bb   = llvm::BasicBlock::Create(ctx, "dmx_ret_bb", fn);

      // Alloca for record counter (i32). Reset to 0 each call.
      llvm::Value* al_count = b.CreateAlloca(i32, nullptr, "dmx_count");
      b.CreateStore(llvm::ConstantInt::get(i32, 0), al_count);

      // Look up downstream Output op's port mapping for this Demux's outputs.
      // For each demux output port p (0..N-1), find which Output input port
      // its emission lands on. Default: identity mapping (p -> p).
      std::vector<std::size_t> output_port_for(N, 0);
      for (std::size_t p = 0; p < N; ++p) output_port_for[p] = p;
      for (const auto& conn : graph.connections) {
        if (conn.from_id == sync_id && conn.to_id == output_op_id &&
            conn.from_port < N) {
          output_port_for[conn.from_port] = conn.to_port;
        }
      }

      b.CreateCondBr(dso.sync_flag, dmx_emit_bb, dmx_merge_bb);

      b.SetInsertPoint(dmx_emit_bb);
      // For each output port p, if dso.port_emit[p]: write record at slot
      // count, increment count.
      for (std::size_t p = 0; p < N; ++p) {
        llvm::BasicBlock* bb_yes = llvm::BasicBlock::Create(ctx, "dmx_w_yes", fn);
        llvm::BasicBlock* bb_no  = llvm::BasicBlock::Create(ctx, "dmx_w_no",  fn);
        b.CreateCondBr(dso.port_emit[p], bb_yes, bb_no);

        b.SetInsertPoint(bb_yes);
        llvm::Value* cnt = b.CreateLoad(i32, al_count, "dmx_cnt");
        llvm::Value* cnt_z = b.CreateZExt(cnt, i64, "dmx_cnt_z");

        // out_t_arr[count] = sync_t (i64).
        llvm::Value* t_slot = b.CreateGEP(i64, arg_out_t, cnt_z, "dmx_t_slot");
        b.CreateStore(dso.out_t, t_slot);

        // out_port_id_arr[count] = output_port_for[p] (i32).
        llvm::Value* pid_slot = b.CreateGEP(i32, arg_out_port_id, cnt_z, "dmx_pid_slot");
        b.CreateStore(llvm::ConstantInt::get(i32, static_cast<int32_t>(output_port_for[p])),
                      pid_slot);

        // out_v_arr[count * num_outputs] = port_values[p]. We set just slot
        // 0; downstream translate_jit_to_batch_ uses port_id to route.
        llvm::Value* num_out = llvm::ConstantInt::get(
            i64, static_cast<int64_t>(num_outputs));
        llvm::Value* v_base = b.CreateMul(cnt_z, num_out, "dmx_v_base");
        llvm::Value* v_slot = b.CreateGEP(f64, arg_out_v, v_base, "dmx_v_slot");
        b.CreateStore(dso.port_values[p], v_slot);

        b.CreateStore(b.CreateAdd(cnt, llvm::ConstantInt::get(i32, 1)), al_count);
        b.CreateBr(bb_no);

        b.SetInsertPoint(bb_no);
      }
      b.CreateBr(dmx_ret_bb);

      // dmx_ret_bb: return count.
      b.SetInsertPoint(dmx_ret_bb);
      llvm::Value* ret_cnt = b.CreateLoad(i32, al_count, "dmx_ret_cnt");
      b.CreateRet(ret_cnt);

      // dmx_merge_bb: try_sync failed; fall through to next sync op or ret_false.
      b.SetInsertPoint(dmx_merge_bb);
      continue;
    }

    // ---- VectorCompose: sync N data ports ----
    // Two downstream shapes are supported:
    //   (a) VectorCompose -> Output directly: write N consecutive output
    //       slots and return.
    //   (b) VectorCompose -> non-Output (VectorExtract / VectorProject /
    //       another vector consumer): build an SSA [N x double] and feed it
    //       through value_map so the downstream segment can extract / project.
    if (snode.kind == OpKind::VectorCompose) {
      emit::JoinSyncOutput so = emit::emit_join_try_sync(ec, offset, N);

      // Find the (single) downstream connection from VectorCompose's output.
      const Connection* down_conn = nullptr;
      for (const auto& conn : graph.connections) {
        if (conn.from_id == sync_id && conn.from_port == 0) {
          down_conn = &conn;
          break;
        }
      }
      if (!down_conn) {
        throw std::runtime_error(
            "emit_program: VectorCompose '" + sync_id +
            "' has no downstream connection");
      }
      const bool downstream_is_output = (down_conn->to_id == output_op_id);

      if (downstream_is_output) {
        // (a) Direct-to-Output path.  Writes this VectorCompose's slots to the
        // flat output buffer and marks al_vc_wrote.  Does NOT return immediately
        // so that subsequent VectorCompose sync ops (other terminals) and the
        // post-loop vc_flush block can fill remaining slots (e.g. scalar
        // terminals) before the single combined return.
        const OpNode& out_node = find_node(graph, output_op_id);
        std::size_t out_to_port = down_conn->to_port;
        if (out_node.output_port_width(out_to_port) != N) {
          throw std::runtime_error(
              "emit_program: VectorCompose '" + sync_id +
              "' arity does not match Output port width");
        }
        std::size_t flat_slot_base = output_flat_slot_base(out_node, out_to_port);

        llvm::BasicBlock* vc_emit_bb  = llvm::BasicBlock::Create(ctx, "vc_emit_bb",  fn);
        llvm::BasicBlock* vc_merge_bb = llvm::BasicBlock::Create(ctx, "vc_merge_bb", fn);

        b.CreateCondBr(so.sync_flag, vc_emit_bb, vc_merge_bb);

        b.SetInsertPoint(vc_emit_bb);
        llvm::Value* se = b.CreateLoad(b.getInt1Ty(), al_should_emit_prog);
        llvm::BasicBlock* vc_write_bb = llvm::BasicBlock::Create(ctx, "vc_write_bb", fn);
        // When se=false (Gate-suppressed or upstream warmup) skip the write and
        // fall through to merge — the vc_flush at the end of the loop will see
        // al_vc_wrote=false and return 0.
        b.CreateCondBr(se, vc_write_bb, vc_merge_bb);

        b.SetInsertPoint(vc_write_bb);
        // Record the emit timestamp (all terminals share the same tick).
        b.CreateStore(so.out_t, al_vc_out_t);
        b.CreateStore(llvm::ConstantInt::getTrue(ctx), al_vc_wrote);
        for (std::size_t p = 0; p < N; ++p) {
          llvm::Value* slot = b.CreateGEP(
              f64, arg_out_v,
              llvm::ConstantInt::get(
                  i64, static_cast<int64_t>(flat_slot_base + p)),
              "vc_slot");
          b.CreateStore(so.out_vs[p], slot);
        }
        // Write non-VectorCompose Output connections from value_map.
        // These SSA values originate in the segment-0 blocks that dominate
        // vc_write_bb, so LLVM dominance is satisfied.
        for (const auto& oc : graph.connections) {
          if (oc.to_id != output_op_id || oc.to_kind != PortKind::Data) continue;
          bool oc_from_vc = false;
          for (const auto& n : graph.nodes) {
            if (n.id == oc.from_id && n.kind == OpKind::VectorCompose) {
              oc_from_vc = true; break;
            }
          }
          if (oc_from_vc) continue;
          auto vm_it = value_map.find({oc.from_id, oc.from_port});
          if (vm_it == value_map.end()) continue;
          write_port_value_to_slots(
              ec, out_node, oc.to_port, vm_it->second,
              [&](std::size_t slot, llvm::Value* val) {
                if (slot < num_outputs) {
                  llvm::Value* gep = b.CreateGEP(
                      f64, arg_out_v,
                      llvm::ConstantInt::get(i64, static_cast<int64_t>(slot)));
                  b.CreateStore(val, gep);
                }
              });
        }
        b.CreateBr(vc_merge_bb);

        b.SetInsertPoint(vc_merge_bb);
        continue;
      }

      // (b) Non-Output downstream: build an SSA [N x double] vector and feed
      // it through value_map. The remainder of the program (next segment)
      // runs inside the sync's then_bb, mirroring the generic Join path.
      llvm::BasicBlock* vc_then_bb  = llvm::BasicBlock::Create(ctx, "vc_then_bb",  fn);
      llvm::BasicBlock* vc_merge_bb = llvm::BasicBlock::Create(ctx, "vc_merge_bb", fn);
      b.CreateCondBr(so.sync_flag, vc_then_bb, vc_merge_bb);

      b.SetInsertPoint(vc_then_bb);
      llvm::Type* dty = llvm::Type::getDoubleTy(ctx);
      llvm::Type* vty = llvm::VectorType::get(
          dty, llvm::ElementCount::getFixed(static_cast<unsigned>(N)));
      llvm::Type* i32 = llvm::Type::getInt32Ty(ctx);
      llvm::Value* out_vec = llvm::UndefValue::get(vty);
      for (std::size_t p = 0; p < N; ++p) {
        out_vec = b.CreateInsertElement(
            out_vec, so.out_vs[p],
            llvm::ConstantInt::get(i32, static_cast<std::uint32_t>(p)),
            "vc_ins");
      }
      value_map[{sync_id, 0}] = {so.out_t, out_vec, /*width=*/N};

      // Per-lane fanout to any downstream segment-bytecode Pipeline or
      // KeyedPipeline. Each allocates one port queue per lane, so we push
      // lane k into flat port k. Other downstream consumers (VectorExtract /
      // VectorProject / FE / Output) read the wire from value_map and need
      // no per-lane push.
      for (const auto& conn : graph.connections) {
        if (conn.from_id != sync_id || conn.from_port != 0) continue;
        if (!join_ids.count(conn.to_id)) continue;
        const OpNode& dst = find_node(graph, conn.to_id);
        const bool is_pipe_segbc = dst.kind == OpKind::Pipeline &&
                                    !dst.pipeline_segment_bytecode.empty();
        const bool is_keyed_pipe = dst.kind == OpKind::KeyedPipeline;
        if (!is_pipe_segbc && !is_keyed_pipe) continue;
        const std::size_t pjo = layout.offsets.at(conn.to_id);
        const std::size_t pjN = join_n_ports.at(conn.to_id);
        for (std::size_t k = 0; k < N; ++k) {
          emit::emit_join_push(ec, pjo, pjN, k, so.out_t, so.out_vs[k]);
        }
      }

      // Emit the next segment's ops (they depend on this VectorCompose's
      // vector output).
      const std::size_t next_seg_idx = si + 1;
      if (next_seg_idx < partition.segments.size()) {
        emit_segment_ops(
            ec, b, ctx, fn, graph, layout,
            partition.segments[next_seg_idx].op_ids,
            output_op_id, num_outputs,
            arg_out_t, arg_out_v, arg_out_port_id,
            bb_ret_true, bb_ret_false, al_should_emit_prog,
            value_map, join_ids, join_n_ports, join_successors_fn);
      }

      if (!b.GetInsertBlock()->getTerminator()) {
        b.CreateBr(vc_merge_bb);
      }

      b.SetInsertPoint(vc_merge_bb);
      continue;
    }

    // ---- FusedExpression: sync N data ports + walk RPN bytecode ----
    // After the N-port try_sync succeeds, walk fe_bytecode against the synced
    // input values; the bytecode produces fe_num_outputs SSA doubles plus a
    // combined emit_flag. As with VectorCompose, two downstream shapes are
    // supported: (a) direct-to-Output, (b) feed an SSA [N x double] vector
    // through value_map for downstream vector consumers.
    if (snode.kind == OpKind::FusedExpression) {
      emit::JoinSyncOutput so = emit::emit_join_try_sync(ec, offset, N);

      // Find the (single) downstream connection from FE's output.
      const Connection* down_conn = nullptr;
      for (const auto& conn : graph.connections) {
        if (conn.from_id == sync_id && conn.from_port == 0) {
          down_conn = &conn;
          break;
        }
      }
      if (!down_conn) {
        throw std::runtime_error(
            "emit_program: FusedExpression '" + sync_id +
            "' has no downstream connection");
      }
      const bool downstream_is_output = (down_conn->to_id == output_op_id);

      const std::size_t M = snode.fe_num_outputs;
      // The bytecode-state region begins right after the N port queues.
      const std::size_t bc_state_off =
          offset + N * (2 * rtbot::compiled::kJoinPortCapacity + 2);

      llvm::BasicBlock* fe_then_bb  = llvm::BasicBlock::Create(ctx, "fe_then_bb",  fn);
      llvm::BasicBlock* fe_merge_bb = llvm::BasicBlock::Create(ctx, "fe_merge_bb", fn);
      b.CreateCondBr(so.sync_flag, fe_then_bb, fe_merge_bb);

      b.SetInsertPoint(fe_then_bb);
      emit::FusedExprOutput feo = emit::emit_fused_expression(
          ec, bc_state_off,
          snode.fe_bytecode, snode.fe_constants, snode.fe_coefficients,
          so.out_vs, M);

      if (downstream_is_output) {
        const OpNode& out_node = find_node(graph, output_op_id);
        const std::size_t out_to_port = down_conn->to_port;
        if (out_node.output_port_width(out_to_port) != M) {
          throw std::runtime_error(
              "emit_program: FusedExpression '" + sync_id +
              "' numOutputs does not match Output port width");
        }
        const std::size_t flat_slot_base =
            output_flat_slot_base(out_node, out_to_port);

        // Combined gate: bytecode emit_flag AND program-level should_emit.
        llvm::Value* se = b.CreateLoad(b.getInt1Ty(), al_should_emit_prog);
        llvm::Value* gate = b.CreateAnd(feo.emit_flag, se, "fe_gate");

        llvm::BasicBlock* fe_write_bb =
            llvm::BasicBlock::Create(ctx, "fe_write_bb", fn);
        b.CreateCondBr(gate, fe_write_bb, fe_merge_bb);

        b.SetInsertPoint(fe_write_bb);
        b.CreateStore(so.out_t, arg_out_t);
        for (std::size_t k = 0; k < M; ++k) {
          llvm::Value* slot = b.CreateGEP(
              f64, arg_out_v,
              llvm::ConstantInt::get(
                  i64, static_cast<int64_t>(flat_slot_base + k)),
              "fe_slot");
          b.CreateStore(feo.out_vs[k], slot);
        }
        b.CreateBr(bb_ret_true);

        b.SetInsertPoint(fe_merge_bb);
        continue;
      }

      // Non-Output downstream: build SSA [M x double] and gate insertion on
      // the combined emit_flag. If the FE suppresses this tick, skip the
      // remainder of the program (branch to merge_bb).
      llvm::BasicBlock* fe_emit_bb = llvm::BasicBlock::Create(ctx, "fe_emit_bb", fn);
      b.CreateCondBr(feo.emit_flag, fe_emit_bb, fe_merge_bb);

      b.SetInsertPoint(fe_emit_bb);
      llvm::Type* dty = llvm::Type::getDoubleTy(ctx);
      llvm::Type* vty = llvm::VectorType::get(
          dty, llvm::ElementCount::getFixed(static_cast<unsigned>(M)));
      llvm::Type* i32t = llvm::Type::getInt32Ty(ctx);
      llvm::Value* out_vec = llvm::UndefValue::get(vty);
      for (std::size_t k = 0; k < M; ++k) {
        out_vec = b.CreateInsertElement(
            out_vec, feo.out_vs[k],
            llvm::ConstantInt::get(i32t, static_cast<std::uint32_t>(k)),
            "fe_ins");
      }
      value_map[{sync_id, 0}] = {so.out_t, out_vec, /*width=*/M};

      // Per-lane fanout to any downstream segment-bytecode Pipeline or
      // KeyedPipeline (mirrors the VectorCompose path above).
      for (const auto& conn : graph.connections) {
        if (conn.from_id != sync_id || conn.from_port != 0) continue;
        if (!join_ids.count(conn.to_id)) continue;
        const OpNode& dst = find_node(graph, conn.to_id);
        const bool is_pipe_segbc = dst.kind == OpKind::Pipeline &&
                                    !dst.pipeline_segment_bytecode.empty();
        const bool is_keyed_pipe = dst.kind == OpKind::KeyedPipeline;
        if (!is_pipe_segbc && !is_keyed_pipe) continue;
        const std::size_t pjo = layout.offsets.at(conn.to_id);
        const std::size_t pjN = join_n_ports.at(conn.to_id);
        for (std::size_t k = 0; k < M; ++k) {
          emit::emit_join_push(ec, pjo, pjN, k, so.out_t, feo.out_vs[k]);
        }
      }

      const std::size_t next_seg_idx = si + 1;
      if (next_seg_idx < partition.segments.size()) {
        emit_segment_ops(
            ec, b, ctx, fn, graph, layout,
            partition.segments[next_seg_idx].op_ids,
            output_op_id, num_outputs,
            arg_out_t, arg_out_v, arg_out_port_id,
            bb_ret_true, bb_ret_false, al_should_emit_prog,
            value_map, join_ids, join_n_ports, join_successors_fn);
      }

      if (!b.GetInsertBlock()->getTerminator()) {
        b.CreateBr(fe_merge_bb);
      }

      b.SetInsertPoint(fe_merge_bb);
      continue;
    }

    // ---- Multiplexer: single-record dispatch ----
    if (snode.kind == OpKind::Mux) {
      emit::MuxTrySyncOutput mso = emit::emit_mux_try_sync(ec, offset, N);

      llvm::BasicBlock* mux_emit_bb  = llvm::BasicBlock::Create(ctx, "mux_emit_bb",  fn);
      llvm::BasicBlock* mux_merge_bb = llvm::BasicBlock::Create(ctx, "mux_merge_bb", fn);

      b.CreateCondBr(mso.sync_flag, mux_emit_bb, mux_merge_bb);

      b.SetInsertPoint(mux_emit_bb);
      // Determine the output port id for Mux's single output (port 0). Lookup
      // via connections; default to 0.
      std::size_t mux_out_port_id = 0;
      for (const auto& conn : graph.connections) {
        if (conn.from_id == sync_id && conn.to_id == output_op_id &&
            conn.from_port == 0) {
          mux_out_port_id = conn.to_port;
          break;
        }
      }

      // Write record 0: out_t_arr[0] = mso.out_t, out_port_id_arr[0] = mux_out_port_id,
      // out_v_arr[0] = mso.out_v. Write to slot 0 only.
      b.CreateStore(mso.out_t, arg_out_t);
      b.CreateStore(llvm::ConstantInt::get(i32, static_cast<int32_t>(mux_out_port_id)),
                    arg_out_port_id);
      b.CreateStore(mso.out_v, arg_out_v);
      b.CreateRet(llvm::ConstantInt::get(i32, 1));

      b.SetInsertPoint(mux_merge_bb);
      continue;
    }

    // ---- Pipeline: composite sync + buffered-flush at segment boundary ----
    // Mirrors Pipeline::process_data semantics:
    //   1. try_sync over (data_ports..., control_port?) so timestamps align.
    //   2. Compute new_key (control-port value, or segment-bytecode of data
    //      lanes — only control-port mode is implemented today).
    //   3. If has_key && new_key != current_key: flush buffered_v[] at the
    //      boundary timestamp `t` and reset inner state via memcpy from the
    //      baked init pattern.
    //   4. Call the inner sub-function on the synced inputs; drain its
    //      emissions into buffered_v[port_id], buffered_t, buffered_msg_present.
    //
    // Restriction: today the Pipeline node must connect each output port
    // directly to the program's Output op; per-port emissions write records
    // into out_*_arr at flat slot 0 (port_id carries the routing).
    if (snode.kind == OpKind::Pipeline) {
      // Two layouts:
      //   - control-port mode: total_ports = num_data_ports + 1 (control). The
      //     trailing flat port carries the segment-key control value.
      //   - segment-bytecode mode: total_ports = lane_width. The single
      //     VECTOR_NUMBER input port unrolls into one flat port per lane;
      //     the segment key is computed by walking pipeline_segment_bytecode
      //     over the N synced lanes.
      const bool has_control_port = snode.pipeline_segment_bytecode.empty();
      std::size_t num_data_ports;
      std::size_t total_ports;
      if (has_control_port) {
        num_data_ports = (snode.pipeline_input_port_types.empty())
                             ? 1
                             : snode.pipeline_input_port_types.size();
        total_ports = num_data_ports + 1;
      } else {
        num_data_ports = (snode.pipeline_input_lane_width == 0)
                             ? 1
                             : snode.pipeline_input_lane_width;
        total_ports = num_data_ports;
      }

      if (snode.pipeline_inner_fn_name.empty()) {
        throw std::runtime_error(
            "emit_program: Pipeline '" + sync_id +
            "' inner sub-function not emitted (D2 wiring missing)");
      }

      const std::size_t pipe_num_outputs =
          snode.pipeline_output_port_types.size();
      if (pipe_num_outputs == 0) {
        throw std::runtime_error(
            "emit_program: Pipeline '" + sync_id + "' has no output ports");
      }

      // Confirm every Pipeline output port lands on the program's Output op.
      // Build a per-pipeline-port → outer-Output-port map for the multi-emit
      // record writes below.
      std::vector<std::int32_t> pipe_to_output_port(pipe_num_outputs, -1);
      for (const auto& conn : graph.connections) {
        if (conn.from_id != sync_id) continue;
        if (conn.to_id != output_op_id) {
          throw std::runtime_error(
              "emit_program: Pipeline '" + sync_id +
              "' output port " + std::to_string(conn.from_port) +
              " is not connected directly to the program Output op "
              "(only direct-to-Output Pipelines are supported in this build)");
        }
        if (conn.from_port < pipe_num_outputs) {
          pipe_to_output_port[conn.from_port] =
              static_cast<std::int32_t>(conn.to_port);
        }
      }
      for (std::size_t p = 0; p < pipe_num_outputs; ++p) {
        if (pipe_to_output_port[p] < 0) {
          throw std::runtime_error(
              "emit_program: Pipeline '" + sync_id + "' output port " +
              std::to_string(p) + " has no connection to Output");
        }
      }

      // try_sync over all (data + control) port queues. JoinSyncOutput's
      // out_vs[k] is the synced value for flat port k.
      emit::JoinSyncOutput so = emit::emit_join_try_sync(ec, offset, total_ports);

      llvm::BasicBlock* pipe_then_bb  =
          llvm::BasicBlock::Create(ctx, "pipe_then_bb",  fn);
      llvm::BasicBlock* pipe_merge_bb =
          llvm::BasicBlock::Create(ctx, "pipe_merge_bb", fn);
      b.CreateCondBr(so.sync_flag, pipe_then_bb, pipe_merge_bb);

      b.SetInsertPoint(pipe_then_bb);

      // Pipeline state slots — see plan_state_layout / state_size_for.
      const std::size_t queue_size =
          total_ports * (2 * rtbot::compiled::kJoinPortCapacity + 2);
      const std::size_t key_off            = offset + queue_size;
      const std::size_t msg_present_off    = key_off + 1;
      const std::size_t buf_t_off          = key_off + 2;
      const std::size_t buf_v_base_off     = key_off + 3;
      const std::size_t inner_state_off    = buf_v_base_off + pipe_num_outputs;

      // new_key:
      //   control-port mode: read the synced control-port value (last flat port).
      //   segment-bytecode mode: walk segment_bytecode over the N synced lanes
      //                          (so.out_vs[0..N-1]) to produce the key.
      llvm::Value* new_key = nullptr;
      if (has_control_port) {
        new_key = so.out_vs[num_data_ports];
      } else {
        std::vector<llvm::Value*> lanes(so.out_vs.begin(),
                                         so.out_vs.begin() + num_data_ports);
        new_key = walk_segment_bytecode(
            ec, snode.pipeline_segment_bytecode,
            snode.pipeline_segment_constants, lanes);
      }

      // GEPs for the per-Pipeline state slots are computed once at the top of
      // pipe_then_bb so they dominate every block in the Pipeline emission's
      // sub-CFG (flush, reset, post-flush, drain).
      llvm::Value* key_ptr        = ec.state_gep(key_off);
      llvm::Value* mp_ptr_top     = ec.state_gep(msg_present_off);
      llvm::Value* buf_t_ptr_top  = ec.state_gep(buf_t_off);
      llvm::Value* buf_v_base_top = ec.state_gep(buf_v_base_off);

      // Tracks whether we actually wrote records this tick. set only when
      // both should_flush and msg_present (and the program-level Gate) line
      // up; the tick's return count comes from this alloca, not from
      // should_flush alone (a flush without a buffered message must return 0
      // to match FE's `emit_buffer` semantics).
      llvm::Value* al_did_flush =
          b.CreateAlloca(b.getInt1Ty(), nullptr, "pipe_did_flush");
      b.CreateStore(llvm::ConstantInt::getFalse(ctx), al_did_flush);

      llvm::Value* last_key  = b.CreateLoad(f64, key_ptr, "pipe_last_key");

      // Decide whether to flush:
      //   flush = !isnan(last_key) && (new_key != last_key)
      // FE checks `has_key_ && new_key != current_key_`; the NaN sentinel
      // mirrors `!has_key_` (NaN != NaN is true under FCMP_OEQ semantics, so
      // we test ordered-equal to detect non-NaN).
      llvm::Value* last_is_set =
          b.CreateFCmpOEQ(last_key, last_key, "pipe_last_is_set");
      llvm::Value* key_changed =
          b.CreateFCmpUNE(new_key, last_key, "pipe_key_changed");
      llvm::Value* should_flush =
          b.CreateAnd(last_is_set, key_changed, "pipe_should_flush");

      llvm::BasicBlock* pipe_flush_bb =
          llvm::BasicBlock::Create(ctx, "pipe_flush_bb", fn);
      llvm::BasicBlock* pipe_post_flush_bb =
          llvm::BasicBlock::Create(ctx, "pipe_post_flush_bb", fn);
      b.CreateCondBr(should_flush, pipe_flush_bb, pipe_post_flush_bb);

      // ---- pipe_flush_bb: gated on (msg_present != 0) ----
      b.SetInsertPoint(pipe_flush_bb);

      llvm::Value* mp_d   = b.CreateLoad(f64, mp_ptr_top, "pipe_mp_d");
      llvm::Value* mp_nz  =
          b.CreateFCmpONE(mp_d, llvm::ConstantFP::get(f64, 0.0), "pipe_mp_nz");

      llvm::BasicBlock* pipe_write_bb =
          llvm::BasicBlock::Create(ctx, "pipe_write_bb", fn);
      llvm::BasicBlock* pipe_reset_bb =
          llvm::BasicBlock::Create(ctx, "pipe_reset_bb", fn);
      b.CreateCondBr(mp_nz, pipe_write_bb, pipe_reset_bb);

      // pipe_write_bb: write num_outputs records at the BOUNDARY timestamp
      // (the current synced t), each at out_v_arr[p * num_outputs + 0] with
      // out_port_id_arr[p] = the corresponding outer Output port.
      b.SetInsertPoint(pipe_write_bb);
      // Apply the program-level Gate before emitting buffered records — FE's
      // emit_buffer respects upstream emit suppression because buffered values
      // were never produced when Gate was off, but the JIT's outer Gate gates
      // OUTPUT writes uniformly. Match it for safety.
      llvm::Value* se_pipe = b.CreateLoad(b.getInt1Ty(), al_should_emit_prog);
      llvm::BasicBlock* pipe_write_do_bb =
          llvm::BasicBlock::Create(ctx, "pipe_write_do_bb", fn);
      b.CreateCondBr(se_pipe, pipe_write_do_bb, pipe_reset_bb);

      b.SetInsertPoint(pipe_write_do_bb);
      for (std::size_t p = 0; p < pipe_num_outputs; ++p) {
        llvm::Value* rec_idx = llvm::ConstantInt::get(
            i64, static_cast<int64_t>(p));

        // out_t_arr[p] = boundary timestamp (current synced t)
        llvm::Value* t_slot =
            b.CreateGEP(i64, arg_out_t, rec_idx, "pipe_t_slot");
        b.CreateStore(so.out_t, t_slot);

        // out_port_id_arr[p] = pipe_to_output_port[p]
        llvm::Value* pid_slot =
            b.CreateGEP(i32, arg_out_port_id, rec_idx, "pipe_pid_slot");
        b.CreateStore(
            llvm::ConstantInt::get(i32, pipe_to_output_port[p]),
            pid_slot);

        // out_v_arr[p * num_outputs + 0] = buffered_v[p]
        llvm::Value* num_out_const = llvm::ConstantInt::get(
            i64, static_cast<int64_t>(num_outputs));
        llvm::Value* v_base =
            b.CreateMul(rec_idx, num_out_const, "pipe_v_base");
        llvm::Value* v_slot =
            b.CreateGEP(f64, arg_out_v, v_base, "pipe_v_slot");
        llvm::Value* buf_v_ptr = ec.state_gep(buf_v_base_off + p);
        llvm::Value* buf_v     = b.CreateLoad(f64, buf_v_ptr, "pipe_buf_v");
        b.CreateStore(buf_v, v_slot);
      }
      // Mark that we actually emitted records this tick.
      b.CreateStore(llvm::ConstantInt::getTrue(ctx), al_did_flush);
      // Clear buffered_msg_present after flushing.
      b.CreateStore(llvm::ConstantFP::get(f64, 0.0), mp_ptr_top);
      // We will return num_outputs after running the inner once on the
      // current input — the inner's emissions go back into the buffer for
      // the next boundary. Stash the count in an alloca + branch through
      // the inner-call block.
      b.CreateBr(pipe_reset_bb);

      // pipe_reset_bb: zero msg_present (idempotent if already cleared above)
      // and memcpy the inner state from the baked init pattern.
      b.SetInsertPoint(pipe_reset_bb);
      // Always reset: the FE resets internals on key change regardless of
      // whether anything was buffered (an inner op might have advanced its
      // state without producing an emission). msg_present was either cleared
      // above or was already 0; storing 0 is harmless.
      b.CreateStore(llvm::ConstantFP::get(f64, 0.0), mp_ptr_top);

      // memcpy the inner state slots from snode.pipeline_inner_state_init.
      // The init pattern is baked into the IR as constant stores so LLVM can
      // fold consecutive zeros into a single memset.
      for (std::size_t k = 0; k < snode.pipeline_inner_state_init.size(); ++k) {
        const double v = snode.pipeline_inner_state_init[k];
        llvm::Value* slot_ptr = ec.state_gep(inner_state_off + k);
        b.CreateStore(llvm::ConstantFP::get(f64, v), slot_ptr);
      }

      // Track flushed-record count for the return.
      // We reach pipe_reset_bb either via pipe_write_do_bb (after writing N
      // records) or via mp_nz=false / se_pipe=false (no records written).
      // The flag is what the function should return when the inner does not
      // add more records (which it can't directly — inner emissions go into
      // the buffer, not into the function's out arrays this tick).
      b.CreateBr(pipe_post_flush_bb);

      // ---- pipe_post_flush_bb: stash key, then call inner sub-function ----
      b.SetInsertPoint(pipe_post_flush_bb);

      // last_segment_key = new_key (always — both first-tick and same-key
      // ticks fall through here without flushing).
      b.CreateStore(new_key, key_ptr);

      // Build the in_v_arr alloca. Control-port mode: single scalar lane
      // (data port 0 = so.out_vs[0]). Segment-bytecode mode: pack N synced
      // lanes (so.out_vs[0..N-1]) into N consecutive slots so the inner
      // sub-function can read each lane via in_v_arr[k].
      const std::size_t in_v_lanes =
          has_control_port ? 1 : num_data_ports;
      llvm::Value* al_in_v =
          b.CreateAlloca(f64, llvm::ConstantInt::get(
                                  i32, static_cast<int>(in_v_lanes)),
                         "pipe_in_v");
      for (std::size_t k = 0; k < in_v_lanes; ++k) {
        llvm::Value* slot = b.CreateGEP(
            f64, al_in_v,
            llvm::ConstantInt::get(i64, static_cast<int64_t>(k)),
            "pipe_in_v_slot");
        b.CreateStore(so.out_vs[k], slot);
      }

      // Allocate scratch buffers for the inner's emissions. K records of
      // (t, port_id, value) plus the inner's num_outputs scalar slots per
      // record. The inner only emits at most one record per (t, port_id)
      // pair — width is 1 for today's all-scalar Pipelines.
      const std::size_t inner_K = compute_max_emits_per_call(
          *snode.pipeline_inner_graph);
      const std::size_t inner_num_outputs = pipe_num_outputs;
      llvm::Value* al_inner_t = b.CreateAlloca(
          i64, llvm::ConstantInt::get(i32, static_cast<int>(inner_K)),
          "pipe_inner_t");
      llvm::Value* al_inner_v = b.CreateAlloca(
          f64, llvm::ConstantInt::get(i32, static_cast<int>(inner_K * inner_num_outputs)),
          "pipe_inner_v");
      // out_port_id_arr is always written by the inner sub-function (defaults
      // to 0 unless an inner Demux / TopK overrides it). We allocate it to
      // keep the inner's signature happy; the drain reads only out_v_arr.
      llvm::Value* al_inner_pid = b.CreateAlloca(
          i32, llvm::ConstantInt::get(i32, static_cast<int>(inner_K)),
          "pipe_inner_pid");

      // inner_state pointer = state + inner_state_off.
      llvm::Value* inner_state_ptr = ec.state_gep(inner_state_off);

      // Look up the inner sub-function emitted by emit_inner_program.
      llvm::Function* inner_fn =
          mod.getFunction(snode.pipeline_inner_fn_name);
      if (inner_fn == nullptr) {
        throw std::runtime_error(
            "emit_program: Pipeline '" + sync_id +
            "' inner sub-function '" + snode.pipeline_inner_fn_name +
            "' not found in module");
      }

      // call inner_fn(inner_state, t, in_v_arr, out_t, out_v, out_port_id)
      llvm::Value* call_args[6] = {
          inner_state_ptr, so.out_t, al_in_v,
          al_inner_t, al_inner_v, al_inner_pid};
      llvm::Value* inner_count = b.CreateCall(inner_fn, call_args,
                                              "pipe_inner_count");

      // ---- drain inner emissions into buffered_v[], buffered_t,
      // buffered_msg_present. The inner sub-function (per emit_inner_program)
      // emits at most one combined record where slot k of out_v_arr holds
      // the value for outer Pipeline output port k. We copy each slot into
      // its corresponding buffered_v[k] when inner_count > 0. Inner programs
      // containing multi-record emit ops (Demux, TopK) inside a Pipeline are
      // not supported by this dispatch path.
      llvm::Value* has_inner_emit = b.CreateICmpSGT(
          inner_count, llvm::ConstantInt::get(i32, 0), "pipe_has_inner_emit");
      llvm::BasicBlock* drain_body_bb =
          llvm::BasicBlock::Create(ctx, "pipe_drain_body", fn);
      llvm::BasicBlock* drain_exit_bb =
          llvm::BasicBlock::Create(ctx, "pipe_drain_exit", fn);
      b.CreateCondBr(has_inner_emit, drain_body_bb, drain_exit_bb);

      b.SetInsertPoint(drain_body_bb);

      // For each Pipeline output port slot k, copy al_inner_v[k] -> buffered_v[k].
      for (std::size_t k = 0; k < pipe_num_outputs; ++k) {
        llvm::Value* slot_idx = llvm::ConstantInt::get(
            i64, static_cast<int64_t>(k));
        llvm::Value* in_ptr =
            b.CreateGEP(f64, al_inner_v, slot_idx, "pipe_drain_in_p");
        llvm::Value* in_v = b.CreateLoad(f64, in_ptr, "pipe_drain_in_v");
        llvm::Value* buf_ptr = ec.state_gep(buf_v_base_off + k);
        b.CreateStore(in_v, buf_ptr);
      }

      // buffered_t = al_inner_t[0] (bit-cast i64 -> double for state slot)
      llvm::Value* tt0_ptr = al_inner_t;  // GEP[0] is the base pointer.
      llvm::Value* tt0 = b.CreateLoad(i64, tt0_ptr, "pipe_drain_t0");
      llvm::Value* tt0_d = b.CreateBitCast(tt0, f64, "pipe_drain_t0_d");
      b.CreateStore(tt0_d, buf_t_ptr_top);

      // buffered_msg_present = 1
      b.CreateStore(llvm::ConstantFP::get(f64, 1.0), mp_ptr_top);

      b.CreateBr(drain_exit_bb);

      // ---- drain_exit_bb: return num_outputs if we flushed this tick,
      // else 0. al_did_flush already records whether a flush actually
      // wrote records; the inner-call drain does not emit records, only
      // updates the buffer for the NEXT boundary.
      b.SetInsertPoint(drain_exit_bb);

      // Build the return count from al_did_flush. should_flush alone is not
      // sufficient — we must also have had a buffered message and passed
      // the program-level Gate (matching FE's emit_buffer semantics).
      llvm::Value* did_flush_load =
          b.CreateLoad(b.getInt1Ty(), al_did_flush, "pipe_did_flush_l");
      llvm::Value* ret_count = b.CreateSelect(
          did_flush_load,
          llvm::ConstantInt::get(i32, static_cast<int32_t>(pipe_num_outputs)),
          llvm::ConstantInt::get(i32, 0),
          "pipe_ret_count");
      b.CreateRet(ret_count);

      // ---- pipe_merge_bb: try_sync did not fire — fall through to next
      // sync op or ret_false (no records emitted this call).
      b.SetInsertPoint(pipe_merge_bb);
      continue;
    }

    // ---- KeyedPipeline: per-key dispatch + per-tick inner emission ----
    // Mirrors KeyedPipeline::process_data semantics:
    //   1. try_sync over the lane queues (one queue per lane of the input
    //      vector wire) so timestamps align.
    //   2. Compute the key — either lane[key_index] (simple-key) or a
    //      polynomial hash over selected lanes (computed-key).
    //   3. Look up the per-key state buffer via the runtime helper.
    //   4. Pack the synced lanes into an in_v_arr and call the inner
    //      sub-function on that key's state buffer.
    //   5. For each emitted record from the inner, write a record to the
    //      outer out_*_arr at slot r. In simple-key mode the key is
    //      prepended to the value vector (out_v[r * num_outputs + 0] = key,
    //      then the inner's per-port emissions follow). In computed-key
    //      mode the inner's emissions pass through directly.
    if (snode.kind == OpKind::KeyedPipeline) {
      const std::size_t lane_w = snode.keyed_pipeline_input_lane_width;
      if (lane_w == 0) {
        throw std::runtime_error(
            "emit_program: KeyedPipeline '" + sync_id +
            "' has zero input lane width");
      }
      if (snode.keyed_pipeline_inner_fn_name.empty()) {
        throw std::runtime_error(
            "emit_program: KeyedPipeline '" + sync_id +
            "' inner sub-function not emitted");
      }
      // Total scalar slots written by the inner sub-function. When the inner
      // emits a wide vector wire (e.g. BurstAggregate producing
      // key_columns + num_agg_outputs lanes via a single output mapping),
      // the slot count is the lane width, not just the inner port count.
      std::size_t inner_out_n = 0;
      if (snode.keyed_pipeline_inner_graph) {
        std::unordered_map<std::string, std::size_t> idx;
        const auto& inner_nodes = snode.keyed_pipeline_inner_graph->nodes;
        idx.reserve(inner_nodes.size());
        for (std::size_t i = 0; i < inner_nodes.size(); ++i) {
          idx[inner_nodes[i].id] = i;
        }
        for (const auto& m : snode.keyed_pipeline_output_mappings) {
          auto it = idx.find(m.second.first);
          if (it == idx.end()) continue;
          const std::size_t w =
              inner_nodes[it->second].output_port_width(m.second.second);
          inner_out_n += (w == 0 ? 1 : w);
        }
      }
      if (inner_out_n == 0) {
        inner_out_n = snode.keyed_pipeline_output_port_types.size();
      }
      const bool simple_key = (snode.keyed_pipeline_key_index >= 0);
      const std::size_t outer_out_w =
          simple_key ? (1 + inner_out_n) : inner_out_n;

      // Collect downstream output chains from the KeyedPipeline:
      //   (a) direct-to-Output: base view terminal.
      //   (b) FEV-then-Output:  a FusedExpressionVector node (WHERE+SELECT
      //       absorbed) that connects KP output to a downstream view terminal.
      //       Each FEV chain produces at most one additional record per inner
      //       emission, gated by the FEV's GATE opcode.
      //
      // Multi-view session programs (rtbot-sql) produce a mixture of (a) and
      // (b).  Single-view programs use only (a).  The per-record emit loop
      // below handles both shapes through a shared output-slot counter.
      const Connection* out_conn = nullptr;   // (a) direct-to-Output conn
      std::size_t flat_slot_base = 0;         // flat slot base for (a)

      struct KpFevChain {
        const OpNode* fev;          // FEV node
        std::size_t   fev_state_off; // state layout offset for stateful FEV ops
        std::size_t   out_port;     // to_port on Output op
        std::size_t   fev_flat_slot; // flat_slot_base at that Output port
      };
      std::vector<KpFevChain> fev_chains;

      const OpNode& out_node = find_node(graph, output_op_id);

      for (const auto& conn : graph.connections) {
        if (conn.from_id != sync_id || conn.from_port != 0) continue;
        if (conn.to_id == output_op_id) {
          // (a) Direct-to-Output connection.
          if (out_node.output_port_width(conn.to_port) != outer_out_w) {
            throw std::runtime_error(
                "emit_program: KeyedPipeline '" + sync_id +
                "' output width does not match Output port width at port " +
                std::to_string(conn.to_port));
          }
          out_conn        = &conn;
          flat_slot_base  = output_flat_slot_base(out_node, conn.to_port);
        } else {
          // (b) Non-Output downstream: must be a FusedExpressionVector whose
          // output then connects directly to the program Output.  Any other
          // topology (Demux, FE sync, etc.) is not yet supported here.
          const OpNode& down = find_node(graph, conn.to_id);
          if (down.kind != OpKind::FusedExpressionVector) {
            throw std::runtime_error(
                "emit_program: KeyedPipeline '" + sync_id +
                "' has downstream op '" + conn.to_id +
                "' of unsupported kind (only FusedExpressionVector is "
                "supported for multi-view KeyedPipeline fanout)");
          }
          // Find the FEV's output connection to the program Output.
          const Connection* fev_out_conn = nullptr;
          for (const auto& c2 : graph.connections) {
            if (c2.from_id == conn.to_id && c2.from_port == 0 &&
                c2.to_id == output_op_id) {
              fev_out_conn = &c2;
              break;
            }
          }
          if (!fev_out_conn) {
            throw std::runtime_error(
                "emit_program: KeyedPipeline '" + sync_id +
                "' FEV downstream '" + conn.to_id +
                "' has no connection to Output");
          }
          KpFevChain chain;
          chain.fev          = &down;
          chain.fev_state_off =
              layout.offsets.count(conn.to_id)
                  ? layout.offsets.at(conn.to_id)
                  : 0;
          chain.out_port     = fev_out_conn->to_port;
          chain.fev_flat_slot =
              output_flat_slot_base(out_node, fev_out_conn->to_port);
          fev_chains.push_back(chain);
        }
      }
      if (!out_conn && fev_chains.empty()) {
        throw std::runtime_error(
            "emit_program: KeyedPipeline '" + sync_id +
            "' has no Output connection");
      }

      emit::JoinSyncOutput so = emit::emit_join_try_sync(ec, offset, lane_w);

      llvm::BasicBlock* kp_then_bb  =
          llvm::BasicBlock::Create(ctx, "kp_then_bb", fn);
      llvm::BasicBlock* kp_merge_bb =
          llvm::BasicBlock::Create(ctx, "kp_merge_bb", fn);
      b.CreateCondBr(so.sync_flag, kp_then_bb, kp_merge_bb);

      b.SetInsertPoint(kp_then_bb);

      // KeyedPipeline state layout: [port queues][ctx_ptr_slot]
      const std::size_t queue_size =
          lane_w * (2 * rtbot::compiled::kJoinPortCapacity + 2);
      const std::size_t ctx_ptr_off = offset + queue_size;

      // Compute the key.
      llvm::Value* key_v = nullptr;
      if (simple_key) {
        const std::size_t k_idx =
            static_cast<std::size_t>(snode.keyed_pipeline_key_index);
        key_v = so.out_vs[k_idx];
      } else {
        key_v = llvm::ConstantFP::get(f64, 0.0);
        for (std::size_t i = 0;
             i < snode.keyed_pipeline_key_column_indices.size(); ++i) {
          const std::size_t lane =
              static_cast<std::size_t>(
                  snode.keyed_pipeline_key_column_indices[i]);
          llvm::Value* coeff = llvm::ConstantFP::get(
              f64, snode.keyed_pipeline_key_coefficients[i]);
          llvm::Value* term =
              b.CreateFMul(so.out_vs[lane], coeff, "kp_term");
          key_v = b.CreateFAdd(key_v, term, "kp_key_acc");
        }
      }

      // Load the runtime ctx pointer from the state slot. The slot stores
      // a void* memcpy'd into a double; reinterpret as i8* for the helper.
      llvm::Value* ctx_slot_ptr = ec.state_gep(ctx_ptr_off);
      llvm::Value* ctx_d = b.CreateLoad(f64, ctx_slot_ptr, "kp_ctx_d");
      llvm::Type* i8p = llvm::PointerType::getUnqual(
          llvm::Type::getInt8Ty(ctx));
      llvm::Value* ctx_i64 = b.CreateBitCast(ctx_d, i64, "kp_ctx_i64");
      llvm::Value* ctx_ptr = b.CreateIntToPtr(ctx_i64, i8p, "kp_ctx_ptr");

      // Bake the runtime helper's address as a 64-bit integer constant and
      // intToPtr it to a function pointer at IR time. This sidesteps LLJIT
      // symbol resolution (no dynamic library generator is registered for
      // the host process) and gives the optimizer a fully-resolved callee.
      // Signature: double* rtbot_jit_keyed_pipeline_lookup(void* ctx, double key)
      llvm::Type* dblp = llvm::PointerType::getUnqual(f64);
      llvm::FunctionType* helper_ty =
          llvm::FunctionType::get(dblp, {i8p, f64}, false);
      void* helper_addr =
          reinterpret_cast<void*>(&rtbot_jit_keyed_pipeline_lookup);
      std::uintptr_t helper_addr_int =
          reinterpret_cast<std::uintptr_t>(helper_addr);
      llvm::Type* helper_fn_ptr_ty = llvm::PointerType::getUnqual(helper_ty);
      llvm::Value* helper_addr_v = llvm::ConstantInt::get(
          i64, static_cast<std::uint64_t>(helper_addr_int));
      llvm::Value* helper_callee = b.CreateIntToPtr(
          helper_addr_v, helper_fn_ptr_ty, "kp_helper");
      llvm::Value* helper_args[2] = {ctx_ptr, key_v};
      llvm::Value* per_key_state = b.CreateCall(helper_ty, helper_callee,
                                                  helper_args, "kp_state");

      // Pack synced lanes into an in_v_arr alloca for the inner sub-function.
      llvm::Value* al_in_v = b.CreateAlloca(
          f64, llvm::ConstantInt::get(i32, static_cast<int>(lane_w)),
          "kp_in_v");
      for (std::size_t k = 0; k < lane_w; ++k) {
        llvm::Value* slot = b.CreateGEP(
            f64, al_in_v,
            llvm::ConstantInt::get(i64, static_cast<int64_t>(k)),
            "kp_in_v_slot");
        b.CreateStore(so.out_vs[k], slot);
      }

      // Allocate scratch buffers for the inner's emissions. K records of
      // (t, port_id, value) plus the inner's num_outputs scalar slots per
      // record. Reuse compute_max_emits_per_call on the inner graph to
      // size the scratch — Pipeline-style multi-emit (Demux / TopK) inside
      // the prototype works as long as inner_K is honored here.
      const std::size_t inner_K = compute_max_emits_per_call(
          *snode.keyed_pipeline_inner_graph);
      llvm::Value* al_inner_t = b.CreateAlloca(
          i64, llvm::ConstantInt::get(i32, static_cast<int>(inner_K)),
          "kp_inner_t");
      llvm::Value* al_inner_v = b.CreateAlloca(
          f64, llvm::ConstantInt::get(
                   i32, static_cast<int>(inner_K * inner_out_n)),
          "kp_inner_v");
      llvm::Value* al_inner_pid = b.CreateAlloca(
          i32, llvm::ConstantInt::get(i32, static_cast<int>(inner_K)),
          "kp_inner_pid");

      llvm::Function* inner_fn =
          mod.getFunction(snode.keyed_pipeline_inner_fn_name);
      if (inner_fn == nullptr) {
        throw std::runtime_error(
            "emit_program: KeyedPipeline '" + sync_id +
            "' inner sub-function '" +
            snode.keyed_pipeline_inner_fn_name + "' not found in module");
      }

      llvm::Value* inner_args[6] = {
          per_key_state, so.out_t, al_in_v,
          al_inner_t, al_inner_v, al_inner_pid};
      llvm::Value* inner_count = b.CreateCall(inner_fn, inner_args,
                                                "kp_inner_count");

      // Gate by program-level Gate before writing records.
      llvm::Value* se_kp = b.CreateLoad(b.getInt1Ty(), al_should_emit_prog);
      llvm::Value* gate_count = b.CreateSelect(
          se_kp, inner_count, llvm::ConstantInt::get(i32, 0),
          "kp_gate_count");

      // Forward records: for r in [0, gate_count):
      //   Write base record (direct-to-Output, if out_conn != nullptr).
      //   Write FEV-chain records (one per FEV downstream, gated by GATE).
      //
      // `al_out_slot` tracks the shared output slot across base + FEV
      // emissions so that every record lands at a distinct index in
      // out_*_arr regardless of which chain(s) fire.
      llvm::BasicBlock* kp_loop_bb =
          llvm::BasicBlock::Create(ctx, "kp_loop", fn);
      llvm::BasicBlock* kp_loop_body_bb =
          llvm::BasicBlock::Create(ctx, "kp_loop_body", fn);
      llvm::BasicBlock* kp_loop_exit_bb =
          llvm::BasicBlock::Create(ctx, "kp_loop_exit", fn);

      // Key loop counter (0..gate_count).
      llvm::Value* al_r = b.CreateAlloca(i32, nullptr, "kp_r");
      b.CreateStore(llvm::ConstantInt::get(i32, 0), al_r);
      // Shared output slot counter across base + FEV chain records.
      llvm::Value* al_out_slot = b.CreateAlloca(i32, nullptr, "kp_out_slot");
      b.CreateStore(llvm::ConstantInt::get(i32, 0), al_out_slot);

      b.CreateBr(kp_loop_bb);

      b.SetInsertPoint(kp_loop_bb);
      llvm::Value* r_v = b.CreateLoad(i32, al_r, "kp_r_l");
      llvm::Value* in_range = b.CreateICmpSLT(r_v, gate_count, "kp_in_range");
      b.CreateCondBr(in_range, kp_loop_body_bb, kp_loop_exit_bb);

      b.SetInsertPoint(kp_loop_body_bb);
      llvm::Value* r_i64 = b.CreateZExt(r_v, i64, "kp_r_i64");

      // Load the timestamp for this inner record.
      llvm::Value* in_t_ptr = b.CreateGEP(i64, al_inner_t, r_i64,
                                            "kp_in_t_p");
      llvm::Value* in_t = b.CreateLoad(i64, in_t_ptr, "kp_in_t");

      // Compute inner row base for al_inner_v[r * inner_out_n + k].
      llvm::Value* inner_out_n_const = llvm::ConstantInt::get(
          i64, static_cast<int64_t>(inner_out_n));
      llvm::Value* inner_row_base = b.CreateMul(r_i64, inner_out_n_const,
                                                 "kp_inner_row_base");

      // ---- (a) Base record: direct-to-Output ----
      if (out_conn != nullptr) {
        llvm::Value* slot_v    = b.CreateLoad(i32, al_out_slot, "kp_base_slot");
        llvm::Value* slot_i64  = b.CreateZExt(slot_v, i64, "kp_base_slot_i64");

        // out_t_arr[slot] = in_t
        llvm::Value* out_t_ptr = b.CreateGEP(i64, arg_out_t, slot_i64,
                                              "kp_out_t_p");
        b.CreateStore(in_t, out_t_ptr);

        // out_port_id_arr[slot] = out_conn->to_port
        llvm::Value* out_pid_ptr = b.CreateGEP(i32, arg_out_port_id, slot_i64,
                                                "kp_out_pid_p");
        b.CreateStore(
            llvm::ConstantInt::get(i32, static_cast<int32_t>(out_conn->to_port)),
            out_pid_ptr);

        // out_v[slot * num_outputs + flat_slot_base + ...] = values
        llvm::Value* num_out_const2 = llvm::ConstantInt::get(
            i64, static_cast<int64_t>(num_outputs));
        llvm::Value* row_base = b.CreateMul(slot_i64, num_out_const2,
                                             "kp_row_base");
        llvm::Value* slot_base_const = llvm::ConstantInt::get(
            i64, static_cast<int64_t>(flat_slot_base));
        llvm::Value* row_slot_base = b.CreateAdd(row_base, slot_base_const,
                                                  "kp_row_slot_base");

        std::size_t prepend = simple_key ? 1 : 0;
        if (simple_key) {
          llvm::Value* key_dst =
              b.CreateGEP(f64, arg_out_v, row_slot_base, "kp_key_dst");
          b.CreateStore(key_v, key_dst);
        }
        for (std::size_t k = 0; k < inner_out_n; ++k) {
          llvm::Value* inner_idx = b.CreateAdd(
              inner_row_base,
              llvm::ConstantInt::get(i64, static_cast<int64_t>(k)),
              "kp_inner_idx");
          llvm::Value* inner_p =
              b.CreateGEP(f64, al_inner_v, inner_idx, "kp_inner_p");
          llvm::Value* inner_val = b.CreateLoad(f64, inner_p, "kp_inner_val");
          llvm::Value* out_idx = b.CreateAdd(
              row_slot_base,
              llvm::ConstantInt::get(i64, static_cast<int64_t>(prepend + k)),
              "kp_out_idx");
          llvm::Value* out_p =
              b.CreateGEP(f64, arg_out_v, out_idx, "kp_out_p");
          b.CreateStore(inner_val, out_p);
        }

        // Increment shared slot counter.
        llvm::Value* slot_next = b.CreateAdd(
            slot_v, llvm::ConstantInt::get(i32, 1), "kp_base_slot_next");
        b.CreateStore(slot_next, al_out_slot);
      }

      // ---- (b) FEV chain records ----
      // For each downstream FEV, reconstruct the outer output vector lanes
      // (key prepended in simple_key mode), run the FEV bytecode, and
      // conditionally write a record when the FEV's GATE fires.
      if (!fev_chains.empty()) {
        // Rebuild the outer output vector as SSA scalars for FEV INPUT k.
        std::vector<llvm::Value*> kp_lanes;
        kp_lanes.reserve(outer_out_w);
        if (simple_key) {
          kp_lanes.push_back(key_v);  // lane 0 = key
        }
        for (std::size_t k = 0; k < inner_out_n; ++k) {
          llvm::Value* inner_idx = b.CreateAdd(
              inner_row_base,
              llvm::ConstantInt::get(i64, static_cast<int64_t>(k)),
              "kp_fev_lane_idx");
          llvm::Value* inner_p =
              b.CreateGEP(f64, al_inner_v, inner_idx, "kp_fev_lane_p");
          kp_lanes.push_back(b.CreateLoad(f64, inner_p, "kp_fev_lane"));
        }

        for (std::size_t ci = 0; ci < fev_chains.size(); ++ci) {
          const KpFevChain& chain = fev_chains[ci];
          const OpNode& fev_node = *chain.fev;

          // Run the FEV bytecode against kp_lanes.
          emit::FusedExprOutput feo = emit::emit_fused_expression(
              ec, chain.fev_state_off,
              fev_node.fe_bytecode, fev_node.fe_constants,
              fev_node.fe_coefficients,
              kp_lanes, fev_node.fe_num_outputs);

          // Conditional write: only when GATE fires (emit_flag == true).
          std::string ci_s = std::to_string(ci);
          llvm::BasicBlock* fev_write_bb =
              llvm::BasicBlock::Create(ctx, "kp_fev_write_" + ci_s, fn);
          llvm::BasicBlock* fev_skip_bb =
              llvm::BasicBlock::Create(ctx, "kp_fev_skip_" + ci_s, fn);
          b.CreateCondBr(feo.emit_flag, fev_write_bb, fev_skip_bb);

          b.SetInsertPoint(fev_write_bb);
          {
            llvm::Value* fev_slot_v   = b.CreateLoad(i32, al_out_slot,
                                                       "kp_fev_slot");
            llvm::Value* fev_slot_i64 = b.CreateZExt(fev_slot_v, i64,
                                                       "kp_fev_slot_i64");

            // out_t_arr[slot] = in_t
            llvm::Value* fev_t_ptr = b.CreateGEP(i64, arg_out_t, fev_slot_i64,
                                                   "kp_fev_t_p");
            b.CreateStore(in_t, fev_t_ptr);

            // out_port_id_arr[slot] = chain.out_port
            llvm::Value* fev_pid_ptr = b.CreateGEP(i32, arg_out_port_id,
                                                     fev_slot_i64,
                                                     "kp_fev_pid_p");
            b.CreateStore(
                llvm::ConstantInt::get(i32,
                    static_cast<int32_t>(chain.out_port)),
                fev_pid_ptr);

            // out_v[slot * num_outputs + fev_flat_slot + k] = feo.out_vs[k]
            llvm::Value* fev_num_out = llvm::ConstantInt::get(
                i64, static_cast<int64_t>(num_outputs));
            llvm::Value* fev_row_base = b.CreateMul(fev_slot_i64, fev_num_out,
                                                     "kp_fev_row_base");
            for (std::size_t k = 0; k < fev_node.fe_num_outputs; ++k) {
              llvm::Value* out_idx = b.CreateAdd(
                  fev_row_base,
                  llvm::ConstantInt::get(i64,
                      static_cast<int64_t>(chain.fev_flat_slot + k)),
                  "kp_fev_out_idx");
              llvm::Value* fev_out_p =
                  b.CreateGEP(f64, arg_out_v, out_idx, "kp_fev_out_p");
              b.CreateStore(feo.out_vs[k], fev_out_p);
            }

            // Increment shared slot counter.
            llvm::Value* fev_slot_next = b.CreateAdd(
                fev_slot_v, llvm::ConstantInt::get(i32, 1),
                "kp_fev_slot_next");
            b.CreateStore(fev_slot_next, al_out_slot);
          }
          b.CreateBr(fev_skip_bb);
          b.SetInsertPoint(fev_skip_bb);
        }
      }

      // r++
      llvm::Value* r_next = b.CreateAdd(
          r_v, llvm::ConstantInt::get(i32, 1), "kp_r_next");
      b.CreateStore(r_next, al_r);
      b.CreateBr(kp_loop_bb);

      b.SetInsertPoint(kp_loop_exit_bb);
      // Return total output slot count (base records + FEV chain records).
      llvm::Value* total_out = b.CreateLoad(i32, al_out_slot, "kp_total_out");
      b.CreateRet(total_out);

      b.SetInsertPoint(kp_merge_bb);
      continue;
    }

    // sync_flag, the synced timestamp, and the per-out-port value(s).
    // For OpKind::Join: N output ports each carry a synced value.
    // For OpKind::Linear / OpKind::ReduceJoin: 1 output port carries the
    // reduced value; the input port count is N (used to drive try_sync).
    llvm::Value* sync_flag = nullptr;
    llvm::Value* sync_t    = nullptr;
    std::vector<llvm::Value*> out_vs;
    std::size_t out_n = 0;

    if (snode.kind == OpKind::Join) {
      emit::JoinSyncOutput so = emit::emit_join_try_sync(ec, offset, N);
      sync_flag = so.sync_flag;
      sync_t    = so.out_t;
      out_vs    = so.out_vs;
      out_n     = N;
    } else if (snode.kind == OpKind::Linear) {
      emit::ReducedSyncOutput so = emit::emit_linear_try_sync(
          ec, offset, snode.coefficients);
      sync_flag = so.emit_flag;
      sync_t    = so.out_t;
      out_vs    = {so.out_v};
      out_n     = 1;
    } else {
      // OpKind::ReduceJoin
      emit::ReducedSyncOutput so = emit::emit_reduce_join_try_sync(
          ec, offset, N, snode.reduce_op);
      sync_flag = so.emit_flag;
      sync_t    = so.out_t;
      out_vs    = {so.out_v};
      out_n     = 1;
    }

    llvm::BasicBlock* then_bb  = llvm::BasicBlock::Create(ctx, "jn_then",  fn);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(ctx, "jn_merge", fn);

    b.CreateCondBr(sync_flag, then_bb, merge_bb);

    // then_bb: populate value_map with synced outputs, push to any downstream
    // Joins, then emit the next segment.
    b.SetInsertPoint(then_bb);
    for (std::size_t p = 0; p < out_n; ++p) {
      value_map[{sync_id, p}] = {sync_t, out_vs[p]};
    }
    for (std::size_t p = 0; p < out_n; ++p) {
      for (auto [next_join_id, to_port] : join_successors_fn(sync_id, p)) {
        std::size_t njo = layout.offsets.at(next_join_id);
        std::size_t njN = join_n_ports.at(next_join_id);
        emit::emit_join_push(ec, njo, njN, to_port, sync_t, out_vs[p]);
      }
    }

    // Emit next segment's ops (they depend on this Join's output).
    const std::size_t next_seg_idx = si + 1;
    if (next_seg_idx < partition.segments.size()) {
      emit_segment_ops(
          ec, b, ctx, fn, graph, layout,
          partition.segments[next_seg_idx].op_ids,
          output_op_id, num_outputs,
          arg_out_t, arg_out_v, arg_out_port_id,
          bb_ret_true, bb_ret_false, al_should_emit_prog,
          value_map, join_ids, join_n_ports, join_successors_fn);
    }

    // After the segment, branch to merge_bb (if not already terminated by Output).
    if (!b.GetInsertBlock()->getTerminator()) {
      b.CreateBr(merge_bb);
    }

    // The merge_bb continues to the next iteration (next sync op) or ret_false.
    b.SetInsertPoint(merge_bb);
  }

  // After all sync ops' merge blocks: if any VectorCompose case-a wrote to the
  // flat output buffer this tick (including non-VC scalar terminal slots that
  // were written in vc_write_bb), commit the timestamp and return 1 record.
  // Otherwise fall through to ret_false (nothing emitted this tick).
  if (!b.GetInsertBlock()->getTerminator()) {
    llvm::BasicBlock* bb_vc_flush =
        llvm::BasicBlock::Create(ctx, "vc_flush", fn);
    llvm::Value* vc_wrote = b.CreateLoad(b.getInt1Ty(), al_vc_wrote, "vc_wrote");
    b.CreateCondBr(vc_wrote, bb_vc_flush, bb_ret_false);

    b.SetInsertPoint(bb_vc_flush);
    // Restore the output timestamp (saved by the VC case-a write above).
    llvm::Value* vc_out_t = b.CreateLoad(i64, al_vc_out_t, "vc_out_t");
    b.CreateStore(vc_out_t, arg_out_t);
    b.CreateBr(bb_ret_true);
  }

  // -------------------------------------------------------------------------
  // ret_true block.
  // -------------------------------------------------------------------------
  b.SetInsertPoint(bb_ret_true);
  b.CreateRet(llvm::ConstantInt::get(i32, 1));

  // -------------------------------------------------------------------------
  // Verify.
  // -------------------------------------------------------------------------
  std::string err;
  llvm::raw_string_ostream errs(err);
  if (llvm::verifyFunction(*fn, &errs)) {
    errs.flush();
    llvm::report_fatal_error(
        llvm::StringRef("emit_program: IR verification failed for '" +
                        fn_name + "': " + err));
  }

  return EmittedSegment{fn_name, num_outputs, layout.total_size};
}

// ---------------------------------------------------------------------------
// emit_inner_program — JIT function for a Pipeline node's inner sub-program
// ---------------------------------------------------------------------------
//
// Strategy: build a transformed CompiledGraph that wraps `inner` with a pair
// of synthetic Input + Output adapter ops (so it has the same shape as a
// top-level program), call emit_program on it, then emit a thin wrapper
// function with the documented signature that loads each scalar input port
// from `in_v_arr` and tail-calls the underlying program function.
//
// (These adapter ops are an IR-emission-time graph-rewrite shortcut so the
// existing emit_program body can be reused unchanged. They are not "sentinels"
// in the rtbot/SQL sense — there is no testing or segment-close meaning here;
// they exist purely to make the inner graph look like a top-level program.)
//
// Synthetic adapter op ids are derived from `fn_name` to avoid collisions with
// any user-supplied op id. Input/Output have zero state, so the inner state
// layout is unaffected.
//
// Today's parser only produces single-port scalar Pipelines; multi-port
// inputs would require a different underlying signature (emit_program takes
// a single `double v`). For now we assert single-port and throw otherwise.
llvm::Function* emit_inner_program(
    llvm::LLVMContext& ctx, llvm::Module& mod,
    const CompiledGraph& inner,
    const std::string& fn_name,
    const std::vector<std::string>& outer_input_port_types,
    const std::vector<std::pair<std::size_t, std::pair<std::string, std::size_t>>>&
        outer_output_mappings,
    std::size_t outer_num_outputs,
    std::size_t /*max_emits_per_call_K*/,
    std::size_t outer_input_lane_width,
    const std::string& prototype_input_id) {
  // ---- Validate: single input port (scalar or VECTOR_NUMBER) -------------
  if (outer_input_port_types.size() != 1) {
    throw std::runtime_error(
        "emit_inner_program: only single-port Pipeline inputs are supported "
        "(got " +
        std::to_string(outer_input_port_types.size()) + " ports)");
  }
  if (inner.entry_op_id.empty()) {
    throw std::runtime_error(
        "emit_inner_program: inner graph has no entry op");
  }

  // Detect vector input. The outer input port type is VECTOR_NUMBER iff the
  // Pipeline runs in segment-bytecode mode; outer_input_lane_width must be
  // > 0 in that case (caller resolves it from the upstream wire).
  const bool vector_input = (outer_input_port_types[0] == "vector_number");
  if (vector_input && outer_input_lane_width == 0) {
    throw std::runtime_error(
        "emit_inner_program: vector input requires outer_input_lane_width > 0");
  }

  // ---- Build the transformed graph ---------------------------------------
  // Synthetic adapter op ids — namespaced under fn_name to be unique.
  const std::string syn_input_id  = fn_name + ".__in";
  const std::string syn_output_id = fn_name + ".__out";

  CompiledGraph t_graph = inner;  // copy nodes, connections, outputs, entry

  // If a prototype Input op is declared, rewrite every `from=prototype_input_id`
  // connection to point at the synthetic Input we are about to add, then drop
  // the prototype Input op from the transformed graph. Used by KeyedPipeline
  // whose FE prototype carries its own Input op.
  std::string effective_entry_op_id = inner.entry_op_id;
  if (!prototype_input_id.empty()) {
    // Pick the first downstream consumer of the prototype Input as the new
    // entry. emit_program seeds value_map[{syn_input_id, 0}] from the wrapper
    // arg, so any op consuming that wire reads it directly.
    std::string new_entry;
    for (const auto& conn : inner.connections) {
      if (conn.from_id == prototype_input_id && conn.from_port == 0 &&
          conn.from_kind == PortKind::Data) {
        new_entry = conn.to_id;
        break;
      }
    }
    if (new_entry.empty()) {
      throw std::runtime_error(
          "emit_inner_program: prototype Input '" + prototype_input_id +
          "' has no downstream consumer");
    }
    effective_entry_op_id = new_entry;

    for (auto& conn : t_graph.connections) {
      if (conn.from_id == prototype_input_id) {
        conn.from_id = syn_input_id;
      }
    }
    auto& nodes = t_graph.nodes;
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                                [&](const OpNode& n) {
                                  return n.id == prototype_input_id;
                                }),
                nodes.end());
  }

  OpNode in_node;
  in_node.id = syn_input_id;
  in_node.kind = OpKind::Input;
  in_node.port_types = outer_input_port_types;
  // Annotate the synthetic Input's output port width so VectorExtract /
  // VectorProject / FE-vector consumers can resolve the upstream wire width
  // via output_port_width(0). For scalar input this stays empty (width 1).
  if (vector_input) {
    in_node.port_widths = {outer_input_lane_width};
  }
  t_graph.nodes.push_back(in_node);

  OpNode out_node;
  out_node.id = syn_output_id;
  out_node.kind = OpKind::Output;
  // Output port_types: one entry per outer pipeline output port.
  // outer_num_outputs is the total scalar slot count; today's Pipeline only
  // supports scalar output ports, so port count == outer_num_outputs.
  out_node.port_types.assign(outer_num_outputs, "number");
  // When an inner mapping feeds the synthetic Output from an upstream wire
  // wider than 1 (e.g. KeyedPipeline whose inner BurstAggregate emits a
  // width-N vector into a single outer port), collapse to one VECTOR_NUMBER
  // port of width N. The downstream slot writer extracts each lane to the
  // matching slot in out_v_arr.
  if (outer_output_mappings.size() == 1 && outer_num_outputs > 1) {
    const auto& src = outer_output_mappings.front().second;
    std::size_t src_w = 0;
    for (const auto& nn : t_graph.nodes) {
      if (nn.id == src.first) {
        src_w = nn.output_port_width(src.second);
        break;
      }
    }
    if (src_w == outer_num_outputs) {
      out_node.port_types = {"vector_number"};
      out_node.port_widths = {outer_num_outputs};
    }
  }
  t_graph.nodes.push_back(out_node);

  // Wire Input port 0 -> entry op port 0 (single scalar port). Skip the
  // synthetic connection when the prototype Input was stripped — the
  // rewriting above already redirected its downstream consumers to the
  // synthetic Input id, so an extra explicit edge would duplicate.
  if (prototype_input_id.empty()) {
    Connection in_conn;
    in_conn.from_id = syn_input_id;
    in_conn.from_port = 0;
    in_conn.to_id = effective_entry_op_id;
    in_conn.to_port = 0;
    in_conn.from_kind = PortKind::Data;
    in_conn.to_kind = PortKind::Data;
    t_graph.connections.push_back(in_conn);
  }

  // Wire each output mapping: (inner_op_id, inner_port) -> Output port outer_port.
  for (const auto& mapping : outer_output_mappings) {
    const std::size_t outer_port = mapping.first;
    const std::string& inner_op_id = mapping.second.first;
    const std::size_t inner_port = mapping.second.second;
    Connection oc;
    oc.from_id = inner_op_id;
    oc.from_port = inner_port;
    oc.to_id = syn_output_id;
    oc.to_port = outer_port;
    oc.from_kind = PortKind::Data;
    oc.to_kind = PortKind::Data;
    t_graph.connections.push_back(oc);
  }

  // Output collection map: enumerate the synthetic Output's collected ports.
  // When the synthetic Output was collapsed to a single VECTOR_NUMBER port
  // (single mapping feeding a wide wire), only enumerate the one port.
  {
    const std::size_t syn_port_count = out_node.port_types.size();
    std::vector<std::string> port_names;
    port_names.reserve(syn_port_count);
    for (std::size_t p = 0; p < syn_port_count; ++p) {
      port_names.push_back("o" + std::to_string(p + 1));
    }
    t_graph.outputs[syn_output_id] = std::move(port_names);
  }

  // entry_op_id of the transformed graph points to the synthetic Input so
  // emit_program's seeding (value_map[{input_op_id, 0}] = {arg_t, arg_v})
  // routes through the Input node we just added.
  t_graph.entry_op_id = syn_input_id;

  // ---- Plan the inner state layout on the transformed graph --------------
  // Input/Output have zero state, so this matches the inner graph's layout.
  StateLayout t_layout = plan_state_layout(t_graph);

  // ---- Emit the inner program function via emit_program ------------------
  // emit_program assigns its own auto-generated name; we rename it after.
  // For vector input, the impl is emitted with a (state, t, double*, ...)
  // signature so the wrapper can pass in_v_arr through directly.
  EmittedSegment es = vector_input
      ? emit_program_with_input_width(ctx, mod, t_graph, t_layout,
                                       outer_input_lane_width)
      : emit_program(ctx, mod, t_graph, t_layout);
  llvm::Function* impl_fn = mod.getFunction(es.function_name);
  if (impl_fn == nullptr) {
    throw std::runtime_error(
        "emit_inner_program: emit_program returned unknown function name '" +
        es.function_name + "'");
  }
  // Mark the impl as internal so LLVM is free to inline / remove it once the
  // wrapper has subsumed it.
  impl_fn->setLinkage(llvm::Function::InternalLinkage);

  // ---- Emit the wrapper function with the documented signature ----------
  llvm::Type* f64  = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64  = llvm::Type::getInt64Ty(ctx);
  llvm::Type* i32  = llvm::Type::getInt32Ty(ctx);
  llvm::Type* f64p = llvm::PointerType::getUnqual(f64);
  llvm::Type* i64p = llvm::PointerType::getUnqual(i64);
  llvm::Type* i32p = llvm::PointerType::getUnqual(i32);

  llvm::FunctionType* wrapper_ty = llvm::FunctionType::get(
      i32, {f64p, i64, f64p, i64p, f64p, i32p}, false);

  if (mod.getFunction(fn_name) != nullptr) {
    throw std::runtime_error(
        "emit_inner_program: function name '" + fn_name + "' already exists");
  }
  llvm::Function* wrapper = llvm::Function::Create(
      wrapper_ty, llvm::Function::ExternalLinkage, fn_name, mod);

  auto wai = wrapper->arg_begin();
  llvm::Argument* w_state       = &*wai++; w_state->setName("state");
  llvm::Argument* w_t           = &*wai++; w_t->setName("t");
  llvm::Argument* w_in_v        = &*wai++; w_in_v->setName("in_v_arr");
  llvm::Argument* w_out_t       = &*wai++; w_out_t->setName("out_t_arr");
  llvm::Argument* w_out_v       = &*wai++; w_out_v->setName("out_v_arr");
  llvm::Argument* w_out_port_id = &*wai++; w_out_port_id->setName("out_port_id_arr");

  llvm::IRBuilder<> wb(ctx);
  llvm::BasicBlock* w_entry = llvm::BasicBlock::Create(ctx, "entry", wrapper);
  wb.SetInsertPoint(w_entry);

  // Scalar impl takes the scalar v from in_v_arr[0]; vector impl takes the
  // raw in_v_arr pointer (the impl loads N consecutive lanes itself).
  llvm::Value* impl_input = vector_input
      ? static_cast<llvm::Value*>(w_in_v)
      : static_cast<llvm::Value*>(wb.CreateLoad(f64, w_in_v, "in_v0"));

  llvm::Value* call_args[6] = {w_state, w_t, impl_input,
                                w_out_t, w_out_v, w_out_port_id};
  llvm::CallInst* call = wb.CreateCall(impl_fn, call_args);
  call->setTailCall(true);
  wb.CreateRet(call);

  // ---- Verify the wrapper + impl ------------------------------------------
  // Both functions are emitted into the same module; verify each individually.
  // The full verifyModule pass runs in JitCompiler when the outer function is
  // emitted; for D2 the outer Pipeline still throws, so this function-level
  // check is the only safety net for the inner sub-function until D3 wires it.
  std::string err;
  llvm::raw_string_ostream errs(err);
  if (llvm::verifyFunction(*wrapper, &errs)) {
    errs.flush();
    llvm::report_fatal_error(
        llvm::StringRef("emit_inner_program: IR verification failed for '" +
                        fn_name + "': " + err));
  }
  if (llvm::verifyFunction(*impl_fn, &errs)) {
    errs.flush();
    llvm::report_fatal_error(
        llvm::StringRef("emit_inner_program: IR verification failed for impl '" +
                        impl_fn->getName().str() + "': " + err));
  }

  return wrapper;
}

}  // namespace rtbot::jit
