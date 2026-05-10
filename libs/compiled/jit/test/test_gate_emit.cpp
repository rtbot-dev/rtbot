#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <vector>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "rtbot/compiled/jit/CompiledGraph.h"
#include "rtbot/compiled/jit/SegmentEmitter.h"
#include "rtbot/compiled/jit/SegmentPartitioner.h"
#include "rtbot/compiled/jit/StateLayout.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"
#include "rtbot/fuse/FusedScalarEval.h"
#include "libs/compiled/jit_spike/JitContext.h"

using namespace rtbot::jit;
namespace fused_op = rtbot::fused_op;

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

}  // namespace

// ---------------------------------------------------------------------------
// SCENARIO: emit_gate parity vs FE GATE opcode
//
// FE bytecode: INPUT 0, CONST 0, GT, GATE, INPUT 0, END
//   - pushes input[0]
//   - pushes constants[0] = 0.0
//   - GT → 1.0 if input > 0.0, else 0.0
//   - GATE → if result is 0.0, suppress entire output (emit=false); rewinds sp
//   - INPUT 0 → pushes input value again
//   - END → records output[0] = input[0]
//
// JIT graph equivalent:
//   in_op (Input) → out_op (Output, port 0)          — pass-through value
//   in_op         → const_op (Scale×0.0, port 0)     — always produces 0.0
//   in_op/const_op → gt_op (Gt)                      — input > 0.0
//   gt_op          → gate_op (Gate)                  — suppress if predicate == 0
//
// Expected: for the ~50% of inputs > 0.0, both paths emit the same value;
//           for the rest, both paths suppress.
// ---------------------------------------------------------------------------
SCENARIO("emit_gate matches FE GATE bit-exactly", "[gate][parity]") {
  constexpr std::size_t N = 100;

  std::mt19937_64 rng(0xCA7E);
  std::uniform_real_distribution<double> dist(-5.0, 5.0);
  std::vector<double> values(N);
  for (auto& x : values) x = dist(rng);

  // -------------------------------------------------------------------------
  // FE reference path
  // -------------------------------------------------------------------------
  // Bytecode: INPUT 0, CONST 0, GT, GATE, INPUT 0, END
  std::vector<double> bc_raw = {
      fused_op::INPUT, 0.0,
      fused_op::CONST, 0.0,  // constants[0] = 0.0
      fused_op::GT,
      fused_op::GATE,
      fused_op::INPUT, 0.0,
      fused_op::END,
  };
  auto pack = rtbot::fuse::pack_bytecode(bc_raw);
  // No stateful ops — state is empty.
  std::vector<double> fe_state = pack.state_init;

  // constants array: one entry, 0.0.
  const double constants[1] = {0.0};

  std::vector<std::pair<std::int64_t, double>> fe_out;
  for (std::size_t i = 0; i < N; ++i) {
    double out_v = 0.0;
    double inputs[1] = {values[i]};
    bool emitted = rtbot::fuse::evaluate_one(
        pack.packed.data(), pack.packed.size(),
        constants,
        pack.aux_args.data(),
        /*coefficients=*/nullptr,
        inputs, fe_state.data(), &out_v, 1);
    if (emitted)
      fe_out.push_back({static_cast<std::int64_t>(i + 1), out_v});
  }

  // Sanity: roughly half should emit (inputs drawn from [-5, 5]).
  REQUIRE(fe_out.size() > 20u);
  REQUIRE(fe_out.size() < 90u);

  // -------------------------------------------------------------------------
  // JIT path: build a CompiledGraph and emit_segment
  //
  // Nodes (in topological order):
  //   "in_op"    — Input  (single port)
  //   "const_op" — Scale×0.0 from in_op   → always 0.0
  //   "gt_op"    — Gt(in_op, const_op)    → 1.0 or 0.0
  //   "gate_op"  — Gate(gt_op)            → side-effects should_emit
  //   "out_op"   — Output (single port)   ← in_op
  //
  // Connections:
  //   in_op:0    → const_op:0
  //   in_op:0    → gt_op:0
  //   const_op:0 → gt_op:1
  //   gt_op:0    → gate_op:0
  //   in_op:0    → out_op:0
  // -------------------------------------------------------------------------
  CompiledGraph graph;
  graph.entry_op_id = "in_op";

  {
    OpNode n; n.id = "in_op"; n.kind = OpKind::Input;
    n.port_types = {"number"}; graph.nodes.push_back(n);
  }
  {
    OpNode n; n.id = "const_op"; n.kind = OpKind::Scale;
    n.scale_constant = 0.0; graph.nodes.push_back(n);
  }
  {
    OpNode n; n.id = "gt_op"; n.kind = OpKind::Gt; graph.nodes.push_back(n);
  }
  {
    OpNode n; n.id = "gate_op"; n.kind = OpKind::Gate; graph.nodes.push_back(n);
  }
  {
    OpNode n; n.id = "out_op"; n.kind = OpKind::Output;
    n.port_types = {"number"}; graph.nodes.push_back(n);
  }

  // Connections
  graph.connections.push_back({"in_op",    0, "const_op", 0});
  graph.connections.push_back({"in_op",    0, "gt_op",    0});
  graph.connections.push_back({"const_op", 0, "gt_op",    1});
  graph.connections.push_back({"gt_op",    0, "gate_op",  0});
  graph.connections.push_back({"in_op",    0, "out_op",   0});

  // Output mapping: out_op has one port named "o1".
  graph.outputs["out_op"] = {"o1"};

  // Build a single segment that covers all ops.
  Segment seg;
  seg.op_ids = {"in_op", "const_op", "gt_op", "gate_op", "out_op"};

  StateLayout layout = plan_state_layout(graph);

  auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
  auto mod      = std::make_unique<llvm::Module>("gate_test_mod", *llvm_ctx);

  EmittedSegment es = emit_segment(*llvm_ctx, *mod, graph, seg, layout);

  static rtbot::JitContext jit_gate;
  jit_gate.compile_module(std::move(mod), std::move(llvm_ctx));

  auto gate_fn = jit_gate.lookup<
      bool (*)(double*, std::int64_t, double, std::int64_t*, double*, std::int32_t*)>(
      es.function_name);
  REQUIRE(gate_fn != nullptr);

  // State buffer (no stateful ops, state_size = 0, but allocate 1 slot anyway).
  const std::size_t state_size = (es.state_size_doubles > 0) ? es.state_size_doubles : 1;
  std::vector<double> jit_state(state_size, 0.0);
  std::vector<double> out_v_buf(es.num_outputs, 0.0);

  std::vector<std::pair<std::int64_t, double>> jit_out;
  for (std::size_t i = 0; i < N; ++i) {
    std::int64_t out_t = 0;
    std::int32_t out_pid = 0;
    for (auto& x : out_v_buf) x = 0.0;
    bool emitted = gate_fn(jit_state.data(),
                           static_cast<std::int64_t>(i + 1),
                           values[i],
                           &out_t,
                           out_v_buf.data(),
                           &out_pid);
    if (emitted)
      jit_out.push_back({out_t, out_v_buf[0]});
  }

  // -------------------------------------------------------------------------
  // Parity: FE and JIT must agree on which ticks emit and the emitted values.
  // -------------------------------------------------------------------------
  REQUIRE(fe_out.size() == jit_out.size());

  for (std::size_t i = 0; i < fe_out.size(); ++i) {
    INFO("i=" << i
         << " fe_t="  << fe_out[i].first  << " jit_t=" << jit_out[i].first
         << " fe_v="  << fe_out[i].second << " jit_v=" << jit_out[i].second);
    REQUIRE(fe_out[i].first  == jit_out[i].first);
    REQUIRE(dbits(fe_out[i].second) == dbits(jit_out[i].second));
  }
}
