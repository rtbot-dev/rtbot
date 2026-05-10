#include "rtbot/compiled/jit/JitCompiler.h"

#include <atomic>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

#include "rtbot/compiled/jit/JsonParser.h"
#include "rtbot/compiled/jit/SegmentEmitter.h"
#include "rtbot/compiled/jit/SegmentPartitioner.h"
#include "rtbot/compiled/jit/StateLayout.h"

#include "rtbot/compiled/JoinStage.h"

#include <cstring>

namespace rtbot::jit {

namespace {

// Helper: compute the ctx-pointer state slot for a KeyedPipeline node and
// build its config (init pattern + size). Mirrors StateLayout's KeyedPipeline
// arithmetic — keep in sync if the layout changes.
// Helper: compute the ctx-pointer state slot for a MovingKeyCount node and
// build its config. Mirrors StateLayout's MovingKeyCount arithmetic — keep
// in sync if the layout changes.
MovingKeyCountNodeConfig make_moving_key_count_config(
    const StateLayout& layout, const OpNode& node) {
  MovingKeyCountNodeConfig cfg;
  const std::size_t base = layout.offsets.at(node.id);
  // Layout: ring[W], ring_count, ring_head, ctx_ptr.
  cfg.ctx_ptr_state_slot = base + node.mkc_window_size + 2;
  return cfg;
}

KeyedPipelineNodeConfig make_keyed_pipeline_config(const StateLayout& layout,
                                                    const OpNode& node) {
  KeyedPipelineNodeConfig cfg;
  std::size_t n_ports = node.join_num_ports;
  if (n_ports == 0) n_ports = node.keyed_pipeline_input_lane_width;
  if (n_ports == 0) n_ports = 1;
  const std::size_t queue_size =
      n_ports * (2 * rtbot::compiled::kJoinPortCapacity + 2);
  const std::size_t base = layout.offsets.at(node.id);
  cfg.ctx_ptr_state_slot = base + queue_size;
  cfg.init_pattern = node.keyed_pipeline_inner_state_init;
  cfg.state_size = node.keyed_pipeline_inner_state_size;
  return cfg;
}

}  // namespace

std::unique_ptr<JitCompiledProgram> JitCompiler::compile(
    const std::string& pipeline_json) {
  // 1. Parse JSON -> CompiledGraph
  CompiledGraph graph = parse_program_json(pipeline_json);

  // 2. Partition into segments
  PartitionResult partition = partition_segments(graph);

  // 3. Plan state layout
  StateLayout layout = plan_state_layout(graph);

  // Compute K = max emissions per segment_fn call (Demultiplexer fan-out).
  const std::size_t K = compute_max_emits_per_call(graph);

  // 4. Build LLVM module
  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>("rtbot_jit", *ctx);

  // Walk Pipeline nodes (depth-first via recursive plan_state_layout already
  // having traversed inner graphs) and emit each inner sub-function with a
  // stable name. The outer SegmentEmitter still throws on OpKind::Pipeline so
  // the Program falls back to FE; the sub-functions remain unreferenced for
  // now and will be called from the outer Pipeline IR in D3.
  static std::atomic<int> pipeline_fn_counter{0};
  auto sanitize_id = [](const std::string& id) {
    std::string s;
    s.reserve(id.size());
    for (char c : id) {
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '_') {
        s.push_back(c);
      } else {
        s.push_back('_');
      }
    }
    return s;
  };
  for (auto& node : graph.nodes) {
    if (node.kind == OpKind::Pipeline && node.pipeline_inner_graph) {
      const std::string sanitized = sanitize_id(node.id);
      const int seq = pipeline_fn_counter.fetch_add(1, std::memory_order_relaxed);
      const std::string inner_fn_name =
          "pipeline_inner_" + sanitized + "_" + std::to_string(seq);
      const std::size_t outer_num_outputs =
          node.pipeline_output_port_types.size();
      emit_inner_program(*ctx, *mod, *node.pipeline_inner_graph, inner_fn_name,
                          node.pipeline_input_port_types,
                          node.pipeline_output_mappings, outer_num_outputs, K,
                          node.pipeline_input_lane_width);
      node.pipeline_inner_fn_name = inner_fn_name;
    } else if (node.kind == OpKind::KeyedPipeline &&
               node.keyed_pipeline_inner_graph) {
      const std::string sanitized = sanitize_id(node.id);
      const int seq = pipeline_fn_counter.fetch_add(1, std::memory_order_relaxed);
      const std::string inner_fn_name =
          "keyed_pipeline_inner_" + sanitized + "_" + std::to_string(seq);
      // KeyedPipeline always feeds the prototype the FULL input vector. Mirror
      // segment-bytecode-mode Pipeline by declaring a single VECTOR_NUMBER
      // input port of width keyed_pipeline_input_lane_width.
      const std::vector<std::string> inner_in_types{"vector_number"};
      // outer_num_outputs is the total scalar slot count for the synthetic
      // Output op the inner program writes into. When the inner emits a
      // wide vector wire (e.g. BurstAggregate producing key_columns +
      // num_agg_outputs lanes) we must size the synthetic Output to match
      // the actual lane count, not just the inner output port count.
      std::size_t outer_num_outputs = 0;
      {
        std::unordered_map<std::string, std::size_t> inner_id_to_idx;
        const auto& inner_nodes = node.keyed_pipeline_inner_graph->nodes;
        inner_id_to_idx.reserve(inner_nodes.size());
        for (std::size_t i = 0; i < inner_nodes.size(); ++i) {
          inner_id_to_idx[inner_nodes[i].id] = i;
        }
        for (const auto& m : node.keyed_pipeline_output_mappings) {
          auto it = inner_id_to_idx.find(m.second.first);
          if (it == inner_id_to_idx.end()) continue;
          const std::size_t w =
              inner_nodes[it->second].output_port_width(m.second.second);
          outer_num_outputs += (w == 0 ? 1 : w);
        }
        if (outer_num_outputs == 0) {
          outer_num_outputs = node.keyed_pipeline_output_port_types.size();
        }
      }
      // Locate the prototype Input op (if any) so emit_inner_program can
      // strip it and rewrite its outgoing edges to the synthetic Input.
      std::string proto_input_id;
      for (const auto& nn : node.keyed_pipeline_inner_graph->nodes) {
        if (nn.kind == OpKind::Input) {
          proto_input_id = nn.id;
          break;
        }
      }
      emit_inner_program(*ctx, *mod, *node.keyed_pipeline_inner_graph,
                          inner_fn_name, inner_in_types,
                          node.keyed_pipeline_output_mappings,
                          outer_num_outputs, K,
                          node.keyed_pipeline_input_lane_width,
                          proto_input_id);
      node.keyed_pipeline_inner_fn_name = inner_fn_name;
    }
  }

  EmittedSegment emitted;

  // Detect the program Input op's lane width — if > 1, the emitted function
  // takes a const double* in_v_arr instead of a scalar v.
  std::size_t prog_input_lane_width = 1;
  for (const auto& nn : graph.nodes) {
    if (nn.kind == OpKind::Input) {
      const std::size_t w = nn.output_port_width(0);
      if (w > 1) prog_input_lane_width = w;
      break;
    }
  }

  // Vector-wire-consuming linear ops (FusedExpressionVector, BurstAggregate)
  // need the program-level vector-input plumbing in emit_program even when
  // the graph has no sync ops. Route those graphs through emit_program too.
  bool has_vector_consumer = false;
  for (const auto& nn : graph.nodes) {
    if (nn.kind == OpKind::FusedExpressionVector ||
        nn.kind == OpKind::BurstAggregate) {
      has_vector_consumer = true;
      break;
    }
  }

  const bool route_through_program =
      !partition.sync_ops.empty() ||
      prog_input_lane_width > 1 ||
      has_vector_consumer;

  if (route_through_program) {
    // Multi-segment graph or vector-input linear graph: emit a single fused
    // function for the whole graph. emit_program rejects Demux/Mux/Pipeline.
    if (prog_input_lane_width > 1) {
      emitted = emit_program_with_input_width(*ctx, *mod, graph, layout,
                                                prog_input_lane_width);
    } else {
      emitted = emit_program(*ctx, *mod, graph, layout);
    }
  } else {
    if (partition.segments.size() != 1) {
      throw std::runtime_error(
          "JitCompiler: expected exactly 1 segment, got " +
          std::to_string(partition.segments.size()));
    }
    const Segment& segment = partition.segments[0];
    emitted = emit_segment(*ctx, *mod, graph, segment, layout);
  }

  // 5. Verify the module (catches malformed IR before JIT)
  std::string verify_err;
  llvm::raw_string_ostream verify_os(verify_err);
  if (llvm::verifyModule(*mod, &verify_os)) {
    throw std::runtime_error(
        "JitCompiler: IR verification failed: " + verify_err);
  }

  // 6. Construct JitContext and hand the module to it.
  // JitContext::compile_module runs the O3 IR pipeline + Aggressive codegen.
  auto jit_ctx = std::make_shared<rtbot::JitContext>();
  jit_ctx->compile_module(std::move(mod), std::move(ctx));

  // 7. Look up the JIT'd function. The symbol's actual ABI depends on whether
  // the program was emitted with a vector input (signature uses
  // `const double* in_v_arr` instead of scalar `double v`); both pointer
  // shapes share the same address — we just store under the matching field.
  const bool input_is_vector = (prog_input_lane_width > 1);
  auto fn_ptr =
      jit_ctx->lookup<JitCompiledProgram::SegmentFnT>(emitted.function_name);
  if (fn_ptr == nullptr) {
    throw std::runtime_error(
        "JitCompiler: failed to resolve symbol " + emitted.function_name);
  }

  // 8. Wrap into JitCompiledProgram
  auto prog = std::make_unique<JitCompiledProgram>();
  prog->state_.resize(emitted.state_size_doubles, 0.0);
  prog->state_init_overrides_ = layout.state_init_overrides;
  for (const auto& [off, val] : layout.state_init_overrides) {
    prog->state_[off] = val;
  }
  prog->num_outputs_ = emitted.num_outputs;
  prog->max_emits_per_call_ = K;
  // Per-call scratch buffers sized for K records each.
  prog->out_t_buf_.resize(K, 0);
  prog->out_v_buf_.resize(K * emitted.num_outputs, 0.0);
  prog->out_port_id_buf_.resize(K, 0);
  // Pre-reserve emit_buf_ for ~500K emits to avoid reallocation during benchmarks.
  prog->emit_buf_.reserve(500000 * (2 + emitted.num_outputs));
  prog->input_is_vector_ = input_is_vector;
  prog->input_lane_width_ = prog_input_lane_width;
  // Both function-pointer fields hold the JIT'd entry-point address. The two
  // pointer types alias each other; the active one is selected by callers
  // based on input_is_vector_.
  prog->segment_fn_ = fn_ptr;
  prog->segment_fn_vec_ =
      reinterpret_cast<JitCompiledProgram::SegmentFnVecT>(fn_ptr);
  prog->jit_ctx_ = std::move(jit_ctx);

  // Capture per-KeyedPipeline-node configs and allocate runtime contexts.
  // The state slot at cfg.ctx_ptr_state_slot holds an opaque void* (bit-cast
  // to double) that the IR loads + passes to rtbot_jit_keyed_pipeline_lookup.
  for (const auto& node : graph.nodes) {
    if (node.kind != OpKind::KeyedPipeline) continue;
    prog->keyed_pipeline_configs_.push_back(
        make_keyed_pipeline_config(layout, node));
  }
  for (const auto& cfg : prog->keyed_pipeline_configs_) {
    auto kp_ctx = std::make_unique<KeyedPipelineNodeCtx>();
    kp_ctx->init_pattern = cfg.init_pattern;
    kp_ctx->state_size   = cfg.state_size;
    void* ctx_void = kp_ctx.get();
    double ctx_as_double = 0.0;
    static_assert(sizeof(void*) <= sizeof(double),
                  "KeyedPipeline ctx pointer must fit in a double slot");
    std::memcpy(&ctx_as_double, &ctx_void, sizeof(ctx_void));
    prog->state_[cfg.ctx_ptr_state_slot] = ctx_as_double;
    prog->keyed_pipeline_ctxs_.push_back(std::move(kp_ctx));
  }

  // Capture per-MovingKeyCount-node configs and allocate runtime contexts.
  // The state slot at cfg.ctx_ptr_state_slot holds an opaque void* (bit-cast
  // to double) that the IR loads + passes to rtbot_jit_mkc_step.
  for (const auto& node : graph.nodes) {
    if (node.kind != OpKind::MovingKeyCount) continue;
    prog->moving_key_count_configs_.push_back(
        make_moving_key_count_config(layout, node));
  }
  for (const auto& cfg : prog->moving_key_count_configs_) {
    auto mkc_ctx = std::make_unique<MovingKeyCountNodeCtx>();
    void* ctx_void = mkc_ctx.get();
    double ctx_as_double = 0.0;
    static_assert(sizeof(void*) <= sizeof(double),
                  "MovingKeyCount ctx pointer must fit in a double slot");
    std::memcpy(&ctx_as_double, &ctx_void, sizeof(ctx_void));
    prog->state_[cfg.ctx_ptr_state_slot] = ctx_as_double;
    prog->moving_key_count_ctxs_.push_back(std::move(mkc_ctx));
  }
  return prog;
}

bool probe_runtime_support() {
  static std::atomic<int> result{-1};  // -1=untested, 0=unavail, 1=avail
  int cached = result.load(std::memory_order_acquire);
  if (cached >= 0) return cached == 1;

  bool ok = false;
  try {
    rtbot::JitContext probe_ctx;
    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto mod = std::make_unique<llvm::Module>("rtbot_jit_probe", *ctx);

    // Emit: extern "C" double rtbot_jit_probe_identity(double v) { return v; }
    auto* dbl = llvm::Type::getDoubleTy(*ctx);
    auto* fn_ty = llvm::FunctionType::get(dbl, {dbl}, false);
    auto* fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage,
                                      "rtbot_jit_probe_identity", mod.get());
    auto* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
    llvm::IRBuilder<> b(bb);
    b.CreateRet(fn->getArg(0));

    probe_ctx.compile_module(std::move(mod), std::move(ctx));
    auto fn_ptr = probe_ctx.lookup<double (*)(double)>("rtbot_jit_probe_identity");
    ok = (fn_ptr != nullptr) && (fn_ptr(2.5) == 2.5);
  } catch (...) {
    ok = false;
  }
  result.store(ok ? 1 : 0, std::memory_order_release);
  return ok;
}

}  // namespace rtbot::jit
