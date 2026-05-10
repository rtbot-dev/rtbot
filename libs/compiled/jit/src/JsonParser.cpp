#include "rtbot/compiled/jit/JsonParser.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"

namespace rtbot::jit {

namespace {

// Convert a port name like "o1", "i2", "c1" to a zero-based index plus kind.
// Prefix 'i' / 'o' = Data; prefix 'c' = Control. The numeric suffix is
// 1-based and converted to a 0-based index.
std::pair<std::size_t, PortKind> port_name_to_index(const std::string& port_name) {
  if (port_name.size() < 2) {
    throw std::runtime_error("invalid port name: '" + port_name + "'");
  }
  PortKind kind;
  switch (port_name[0]) {
    case 'i':
    case 'o':
      kind = PortKind::Data;
      break;
    case 'c':
      kind = PortKind::Control;
      break;
    default:
      throw std::runtime_error("invalid port name prefix: '" + port_name + "'");
  }
  int num = std::stoi(port_name.substr(1));
  if (num < 1) {
    throw std::runtime_error("port index out of range in port name: '" + port_name + "'");
  }
  return {static_cast<std::size_t>(num - 1), kind};
}

OpKind op_kind_from_string(const std::string& type_str) {
  if (type_str == "Input")                  return OpKind::Input;
  if (type_str == "Output")                 return OpKind::Output;
  if (type_str == "Addition")               return OpKind::Add;
  if (type_str == "Subtraction")            return OpKind::Sub;
  if (type_str == "Multiplication")         return OpKind::Mul;
  if (type_str == "Division")               return OpKind::Div;
  if (type_str == "Scale")                  return OpKind::Scale;
  if (type_str == "MovingAverage")          return OpKind::MovingAverage;
  if (type_str == "StandardDeviation")      return OpKind::StdDev;
  if (type_str == "ResamplerHermite")       return OpKind::ResamplerHermite;
  if (type_str == "ResamplerConstant")      return OpKind::ResamplerConstant;
  if (type_str == "PeakDetector")           return OpKind::PeakDetector;
  if (type_str == "Difference")             return OpKind::Diff;
  if (type_str == "Join")                   return OpKind::Join;
  if (type_str == "Linear")                 return OpKind::Linear;
  if (type_str == "CumulativeSum")          return OpKind::CumSum;
  if (type_str == "Count")                  return OpKind::Count;
  if (type_str == "CountNumber")            return OpKind::Count;
  if (type_str == "TopK")                   return OpKind::TopK;
  if (type_str == "MaxTracker")             return OpKind::MaxAgg;
  if (type_str == "MinTracker")             return OpKind::MinAgg;
  if (type_str == "MovingSum")              return OpKind::MovingSum;
  if (type_str == "SignChange")             return OpKind::SignChange;
  if (type_str == "FiniteImpulseResponse")  return OpKind::FIR;
  if (type_str == "InfiniteImpulseResponse") return OpKind::IIR;
  if (type_str == "WindowMinMax")           return OpKind::WinMin;  // mode resolved in parse_op_node
  if (type_str == "Gate")                   return OpKind::Gate;
  if (type_str == "StateLoad")              return OpKind::StateLoad;
  if (type_str == "Identity")               return OpKind::Identity;
  if (type_str == "Constant")               return OpKind::Constant;
  if (type_str == "ConstantNumber")         return OpKind::Constant;
  if (type_str == "BooleanToNumber")        return OpKind::BooleanToNumber;
  if (type_str == "TimeShift")              return OpKind::TimeShift;
  if (type_str == "TimestampExtract")       return OpKind::TimestampExtract;
  if (type_str == "LessThanOrEqualToReplace") return OpKind::LessThanOrEqualToReplace;
  if (type_str == "Function")               return OpKind::Function;
  if (type_str == "Demultiplexer")          return OpKind::Demux;
  if (type_str == "Multiplexer")            return OpKind::Mux;
  if (type_str == "VectorCompose")          return OpKind::VectorCompose;
  if (type_str == "VectorExtract")          return OpKind::VectorExtract;
  if (type_str == "VectorProject")          return OpKind::VectorProject;
  if (type_str == "FusedExpression")        return OpKind::FusedExpression;
  if (type_str == "FusedExpressionVector")  return OpKind::FusedExpressionVector;
  if (type_str == "BurstAggregate")         return OpKind::BurstAggregate;
  if (type_str == "Pipeline")               return OpKind::Pipeline;
  if (type_str == "KeyedPipeline")          return OpKind::KeyedPipeline;
  if (type_str == "MovingKeyCount")         return OpKind::MovingKeyCount;

  // ArithmeticScalar (1-input + constant)
  if (type_str == "Add")                    return OpKind::AddScalar;
  if (type_str == "Power")                  return OpKind::PowerScalar;
  if (type_str == "Sin")                    return OpKind::Sin;
  if (type_str == "Cos")                    return OpKind::Cos;
  if (type_str == "Tan")                    return OpKind::Tan;
  if (type_str == "Exp")                    return OpKind::Exp;
  if (type_str == "Log")                    return OpKind::Log;
  if (type_str == "Log10")                  return OpKind::Log10;
  if (type_str == "Abs")                    return OpKind::Abs;
  if (type_str == "Sign")                   return OpKind::Sign;
  if (type_str == "Floor")                  return OpKind::Floor;
  if (type_str == "Ceil")                   return OpKind::Ceil;
  if (type_str == "Round")                  return OpKind::Round;

  // CompareScalar (1-input + constant; emits 0.0/1.0)
  if (type_str == "CompareGT")              return OpKind::GtScalar;
  if (type_str == "CompareLT")              return OpKind::LtScalar;
  if (type_str == "CompareGTE")             return OpKind::GteScalar;
  if (type_str == "CompareLTE")             return OpKind::LteScalar;
  if (type_str == "CompareEQ")              return OpKind::EqScalar;
  if (type_str == "CompareNEQ")             return OpKind::NeqScalar;

  // CompareSync (2-input; emits 0.0/1.0)
  if (type_str == "CompareSyncGT")          return OpKind::Gt;
  if (type_str == "CompareSyncLT")          return OpKind::Lt;
  if (type_str == "CompareSyncGTE")         return OpKind::Gte;
  if (type_str == "CompareSyncLTE")         return OpKind::Lte;
  if (type_str == "CompareSyncEQ")          return OpKind::EqTol;
  if (type_str == "CompareSyncNEQ")         return OpKind::NeqTol;

  // BooleanSync (2-input top-level boolean; emits 0.0/1.0)
  if (type_str == "LogicalAnd")             return OpKind::And;
  if (type_str == "LogicalOr")              return OpKind::Or;
  if (type_str == "LogicalXor")             return OpKind::Xor;
  if (type_str == "LogicalNand")            return OpKind::Nand;
  if (type_str == "LogicalNor")             return OpKind::Nor;
  if (type_str == "LogicalXnor")            return OpKind::Xnor;
  if (type_str == "LogicalImplication")     return OpKind::Implication;

  // FilterScalar (1-input predicate filter; emits original value when predicate holds)
  if (type_str == "GreaterThan")            return OpKind::FiltGtScalar;
  if (type_str == "LessThan")               return OpKind::FiltLtScalar;
  if (type_str == "EqualTo")                return OpKind::FiltEqScalar;
  if (type_str == "NotEqualTo")             return OpKind::FiltNeqScalar;

  // FilterSync (2-input predicate filter; emits first value when predicate holds)
  if (type_str == "SyncGreaterThan")        return OpKind::FiltGtSync;
  if (type_str == "SyncLessThan")           return OpKind::FiltLtSync;
  if (type_str == "SyncEqual")              return OpKind::FiltEqSync;
  if (type_str == "SyncNotEqual")           return OpKind::FiltNeqSync;

  throw std::runtime_error("unknown operator type: '" + type_str + "'");
}

// Map an FE op type (e.g. "Addition") to its ReduceJoin variant. Returns
// nullopt if the type is not a ReduceJoin family member.
std::optional<ReduceOp> reduce_op_from_type(const std::string& type_str) {
  if (type_str == "Addition")          return ReduceOp::AddReduce;
  if (type_str == "Subtraction")       return ReduceOp::SubReduce;
  if (type_str == "Multiplication")    return ReduceOp::MulReduce;
  if (type_str == "Division")          return ReduceOp::DivReduce;
  if (type_str == "LogicalAnd")        return ReduceOp::AndReduce;
  if (type_str == "LogicalOr")         return ReduceOp::OrReduce;
  if (type_str == "LogicalXor")        return ReduceOp::XorReduce;
  if (type_str == "LogicalNand")       return ReduceOp::NandReduce;
  if (type_str == "LogicalNor")        return ReduceOp::NorReduce;
  if (type_str == "LogicalXnor")       return ReduceOp::XnorReduce;
  if (type_str == "LogicalImplication")return ReduceOp::ImplReduce;
  return std::nullopt;
}

// Maps the external composite id to its internal "input adapter" / "output
// adapter" ids. External connections that mention the composite id get
// rewritten to point at these adapters during graph fix-up.
struct CompositeAdapter {
  std::string input_adapter_id;
  std::string output_adapter_id;
};

// Composite operator detection. RelativeStrengthIndex is flattened to a
// primitive sub-graph at parse time. Pipeline, TriggerSet and KeyedPipeline
// remain handled by their respective C++ operators.
bool is_composite_type(const std::string& type_str) {
  return type_str == "RelativeStrengthIndex";
}

// Expand "RelativeStrengthIndex" into a primitive sub-graph. Mirrors the
// textbook formula:
//   diff   = Difference(input)
//   gains  = max(diff, 0)        -> LessThanOrEqualToReplace(0, 0)
//   losses = max(-diff, 0)       -> Scale(-1) -> LessThanOrEqualToReplace(0, 0)
//   sg     = MovingSum(N, gains)
//   sl     = MovingSum(N, losses)
//   rs     = sg / sl             (Division 2-port)
//   rsi    = 100 - 100 / (1 + rs)
//          = AddScalar(1) -> PowerScalar(-1) -> Scale(-100) -> AddScalar(100)
//
// Status: NOT YET ACTIVATED (is_composite_type returns false). The
// SegmentEmitter currently bails out of a segment as soon as the first
// stateful op reports emit_flag=false (see SegmentEmitter.cpp around the
// CreateCondBr(out.emit_flag, cont, bb_ret_false) site). With two parallel
// stateful branches (sum_gains and sum_losses) the second branch's state
// never advances during the warmup of the first, breaking RSI parity.
// Activating this expander requires the segment emitter to defer the
// emit-suppression branch until after every stateful op has had its
// state-advance IR executed.
//
// Internal id scheme uses the double-underscore separator to avoid collisions
// with user IDs.
[[maybe_unused]] CompositeAdapter expand_rsi(CompiledGraph& graph,
                                              const nlohmann::json& j) {
  const auto rsi_id = j.at("id").get<std::string>();
  const auto window = j.at("window_size").get<std::size_t>();
  if (window < 2) {
    throw std::runtime_error("RelativeStrengthIndex '" + rsi_id +
                             "' requires window_size >= 2");
  }

  auto sub_id = [&](const char* role) {
    return rsi_id + "__" + role;
  };

  auto add_node = [&](OpNode n) {
    graph.nodes.push_back(std::move(n));
  };

  auto add_conn = [&](const std::string& from, const std::string& to) {
    Connection c;
    c.from_id   = from;
    c.from_port = 0;
    c.to_id     = to;
    c.to_port   = 0;
    c.from_kind = PortKind::Data;
    c.to_kind   = PortKind::Data;
    graph.connections.push_back(c);
  };

  auto add_conn_p = [&](const std::string& from, const std::string& to,
                        std::size_t to_port) {
    Connection c;
    c.from_id   = from;
    c.from_port = 0;
    c.to_id     = to;
    c.to_port   = to_port;
    c.from_kind = PortKind::Data;
    c.to_kind   = PortKind::Data;
    graph.connections.push_back(c);
  };

  // diff = Difference(input)
  {
    OpNode n;
    n.id = sub_id("diff");
    n.kind = OpKind::Diff;
    add_node(std::move(n));
  }

  // gains_pos = max(diff, 0)
  {
    OpNode n;
    n.id = sub_id("gains_pos");
    n.kind = OpKind::LessThanOrEqualToReplace;
    n.replace_threshold = 0.0;
    n.replace_by = 0.0;
    add_node(std::move(n));
  }

  // diff_neg = -diff
  {
    OpNode n;
    n.id = sub_id("diff_neg");
    n.kind = OpKind::Scale;
    n.scale_constant = -1.0;
    add_node(std::move(n));
  }

  // losses_pos = max(-diff, 0)
  {
    OpNode n;
    n.id = sub_id("losses_pos");
    n.kind = OpKind::LessThanOrEqualToReplace;
    n.replace_threshold = 0.0;
    n.replace_by = 0.0;
    add_node(std::move(n));
  }

  // sum_gains = MovingSum(N, gains_pos)
  {
    OpNode n;
    n.id = sub_id("sum_gains");
    n.kind = OpKind::MovingSum;
    n.window_size = window;
    add_node(std::move(n));
  }

  // sum_losses = MovingSum(N, losses_pos)
  {
    OpNode n;
    n.id = sub_id("sum_losses");
    n.kind = OpKind::MovingSum;
    n.window_size = window;
    add_node(std::move(n));
  }

  // rs = sum_gains / sum_losses (2-port Division)
  {
    OpNode n;
    n.id = sub_id("rs");
    n.kind = OpKind::Div;
    add_node(std::move(n));
  }

  // 1 + rs
  {
    OpNode n;
    n.id = sub_id("one_plus_rs");
    n.kind = OpKind::AddScalar;
    n.scalar_value = 1.0;
    add_node(std::move(n));
  }

  // (1 + rs)^-1
  {
    OpNode n;
    n.id = sub_id("inv");
    n.kind = OpKind::PowerScalar;
    n.scalar_value = -1.0;
    add_node(std::move(n));
  }

  // -100 * inv
  {
    OpNode n;
    n.id = sub_id("scale_neg100");
    n.kind = OpKind::Scale;
    n.scale_constant = -100.0;
    add_node(std::move(n));
  }

  // 100 + (-100 * inv) = 100 - 100/(1+rs) = RSI
  {
    OpNode n;
    n.id = sub_id("rsi_out");
    n.kind = OpKind::AddScalar;
    n.scalar_value = 100.0;
    add_node(std::move(n));
  }

  // Wire the sub-graph.
  add_conn(sub_id("diff"), sub_id("gains_pos"));
  add_conn(sub_id("diff"), sub_id("diff_neg"));
  add_conn(sub_id("diff_neg"), sub_id("losses_pos"));
  add_conn(sub_id("gains_pos"), sub_id("sum_gains"));
  add_conn(sub_id("losses_pos"), sub_id("sum_losses"));
  add_conn_p(sub_id("sum_gains"), sub_id("rs"), 0);
  add_conn_p(sub_id("sum_losses"), sub_id("rs"), 1);
  add_conn(sub_id("rs"), sub_id("one_plus_rs"));
  add_conn(sub_id("one_plus_rs"), sub_id("inv"));
  add_conn(sub_id("inv"), sub_id("scale_neg100"));
  add_conn(sub_id("scale_neg100"), sub_id("rsi_out"));

  return CompositeAdapter{sub_id("diff"), sub_id("rsi_out")};
}

// Forward decl — parse_op_node may recurse into this for Pipeline's inner
// sub-program, and the implementation is defined later in this file.
CompiledGraph parse_program_json_obj(const nlohmann::json& j);

// Convert FE-style port name ("o1", "i2") to a 0-based data-port index. Used
// while translating Pipeline output mappings, where the JSON keys are port
// names rather than indices.
std::size_t port_name_to_data_index(const std::string& port_name) {
  auto [idx, kind] = port_name_to_index(port_name);
  (void)kind;
  return idx;
}

OpNode parse_op_node(const nlohmann::json& j) {
  OpNode node;
  node.id   = j.at("id").get<std::string>();
  const auto type_str = j.at("type").get<std::string>();

  // ReduceJoin family: Addition / Subtraction / ... / LogicalImplication.
  // For numPorts >= 3, route to OpKind::ReduceJoin (true N-port sync). For
  // numPorts <= 2, keep the existing 2-input stateless mapping (OpKind::Add
  // etc.) for backward compatibility with the existing state layout and
  // parity tests.
  if (auto rop = reduce_op_from_type(type_str)) {
    std::size_t n = j.value("numPorts", 2);
    if (n >= 3) {
      node.kind            = OpKind::ReduceJoin;
      node.reduce_op       = *rop;
      node.join_num_ports  = n;
      return node;
    }
  }

  node.kind = op_kind_from_string(type_str);

  switch (node.kind) {
    case OpKind::MovingAverage:
    case OpKind::StdDev:
    case OpKind::MovingSum:
    case OpKind::PeakDetector:
      node.window_size = j.at("window_size").get<std::size_t>();
      break;
    case OpKind::Scale:
      node.scale_constant = j.at("value").get<double>();
      break;
    case OpKind::ResamplerHermite:
      node.resampler_interval = j.at("interval").get<std::int64_t>();
      break;
    case OpKind::ResamplerConstant: {
      node.resampler_interval = j.at("interval").get<std::int64_t>();
      if (j.contains("t0")) {
        node.resampler_t0_set = true;
        // FE OperatorJson reads t0 as a double; cast to int64 timestamp.
        node.resampler_t0 = static_cast<std::int64_t>(
            j.at("t0").get<double>());
      } else {
        node.resampler_t0_set = false;
        node.resampler_t0 = 0;
      }
      if (j.contains("snapFirst")) {
        node.resampler_snap_first = (j.at("snapFirst").get<int>() != 0);
      } else {
        node.resampler_snap_first = false;
      }
      break;
    }
    case OpKind::Input:
    case OpKind::Output:
      if (j.contains("portTypes")) {
        node.port_types = j.at("portTypes").get<std::vector<std::string>>();
      }
      break;
    case OpKind::WinMin:
    case OpKind::WinMax: {
      node.window_size = j.at("window_size").get<std::size_t>();
      // "WindowMinMax" maps to WinMin by default; override to WinMax if mode == "max".
      if (j.contains("mode") && j.at("mode").get<std::string>() == "max") {
        node.kind = OpKind::WinMax;
      } else {
        node.kind = OpKind::WinMin;
      }
      break;
    }
    case OpKind::FIR: {
      node.coefficients = j.at("coefficients").get<std::vector<double>>();
      node.window_size  = node.coefficients.size();
      break;
    }
    case OpKind::Linear: {
      node.coefficients   = j.at("coefficients").get<std::vector<double>>();
      node.join_num_ports = node.coefficients.size();
      if (node.join_num_ports < 2) {
        throw std::runtime_error("Linear operator '" + node.id +
                                 "' requires at least 2 coefficients");
      }
      break;
    }
    case OpKind::Join: {
      node.join_num_ports = j.value("numPorts", 2);
      break;
    }
    case OpKind::VectorCompose: {
      node.join_num_ports = j.at("numPorts").get<std::size_t>();
      if (node.join_num_ports < 1) {
        throw std::runtime_error("VectorCompose '" + node.id +
                                 "' requires numPorts >= 1");
      }
      break;
    }
    case OpKind::FusedExpression: {
      node.fe_num_ports   = j.at("numPorts").get<std::size_t>();
      node.fe_num_outputs = j.at("numOutputs").get<std::size_t>();
      if (node.fe_num_ports < 1) {
        throw std::runtime_error("FusedExpression '" + node.id +
                                 "' requires numPorts >= 1");
      }
      if (node.fe_num_outputs < 1) {
        throw std::runtime_error("FusedExpression '" + node.id +
                                 "' requires numOutputs >= 1");
      }
      node.fe_bytecode = j.at("bytecode").get<std::vector<double>>();
      if (j.contains("constants")) {
        node.fe_constants = j.at("constants").get<std::vector<double>>();
      }
      if (j.contains("coefficients")) {
        node.fe_coefficients = j.at("coefficients").get<std::vector<double>>();
      }
      if (j.contains("stateInit")) {
        node.fe_state_init = j.at("stateInit").get<std::vector<double>>();
      }
      // Reuse join_num_ports for the port-queue layout (FusedExpression
      // inherits Join's N-port queueing) so existing Join machinery applies.
      node.join_num_ports = node.fe_num_ports;
      break;
    }
    case OpKind::FusedExpressionVector: {
      node.fe_num_outputs = j.at("numOutputs").get<std::size_t>();
      if (node.fe_num_outputs < 1) {
        throw std::runtime_error("FusedExpressionVector '" + node.id +
                                 "' requires numOutputs >= 1");
      }
      node.fe_bytecode = j.at("bytecode").get<std::vector<double>>();
      if (j.contains("constants")) {
        node.fe_constants = j.at("constants").get<std::vector<double>>();
      }
      if (j.contains("coefficients")) {
        node.fe_coefficients = j.at("coefficients").get<std::vector<double>>();
      }
      if (j.contains("stateInit")) {
        node.fe_state_init = j.at("stateInit").get<std::vector<double>>();
      }
      // FEV has a single input port that carries a vector wire; no port
      // queues. fe_num_ports is left at 0 to distinguish the layout.
      node.fe_num_ports = 0;
      break;
    }
    case OpKind::BurstAggregate: {
      node.ba_agg_bytecode = j.at("aggBytecode").get<std::vector<double>>();
      if (j.contains("aggConstants")) {
        node.ba_agg_constants =
            j.at("aggConstants").get<std::vector<double>>();
      }
      if (j.contains("segBytecode")) {
        node.ba_seg_bytecode = j.at("segBytecode").get<std::vector<double>>();
      }
      if (j.contains("segConstants")) {
        node.ba_seg_constants =
            j.at("segConstants").get<std::vector<double>>();
      }
      if (j.contains("keyColumns")) {
        const auto& arr = j.at("keyColumns");
        node.ba_key_columns.reserve(arr.size());
        for (const auto& el : arr) {
          double idx = el.is_number_integer()
                           ? static_cast<double>(el.get<std::int64_t>())
                           : el.get<double>();
          if (idx < 0.0) {
            throw std::runtime_error("BurstAggregate '" + node.id +
                                     "' keyColumns must be non-negative");
          }
          node.ba_key_columns.push_back(static_cast<std::size_t>(idx));
        }
      }
      node.ba_num_agg_outputs =
          j.at("numAggOutputs").get<std::size_t>();
      node.ba_num_input_cols =
          j.at("numInputCols").get<std::size_t>();
      if (node.ba_num_agg_outputs < 1) {
        throw std::runtime_error("BurstAggregate '" + node.id +
                                 "' requires numAggOutputs >= 1");
      }
      if (node.ba_num_input_cols < 1) {
        throw std::runtime_error("BurstAggregate '" + node.id +
                                 "' requires numInputCols >= 1");
      }
      for (std::size_t k : node.ba_key_columns) {
        if (k >= node.ba_num_input_cols) {
          throw std::runtime_error("BurstAggregate '" + node.id +
                                   "' keyColumns entry " + std::to_string(k) +
                                   " out of bounds for numInputCols " +
                                   std::to_string(node.ba_num_input_cols));
        }
      }
      break;
    }
    case OpKind::VectorExtract: {
      // FE encodes "index" as a JSON number; nlohmann accepts both integer
      // and floating-point parsings. Read as double then cast to size_t to
      // tolerate either shape.
      const auto& idx_v = j.at("index");
      double idx = idx_v.is_number_integer()
                       ? static_cast<double>(idx_v.get<std::int64_t>())
                       : idx_v.get<double>();
      if (idx < 0.0) {
        throw std::runtime_error("VectorExtract '" + node.id +
                                 "' index must be non-negative");
      }
      node.vector_index = static_cast<std::size_t>(idx);
      break;
    }
    case OpKind::VectorProject: {
      const auto& arr = j.at("indices");
      if (!arr.is_array() || arr.empty()) {
        throw std::runtime_error("VectorProject '" + node.id +
                                 "' requires a non-empty 'indices' array");
      }
      node.vector_indices.reserve(arr.size());
      for (const auto& el : arr) {
        double idx = el.is_number_integer()
                         ? static_cast<double>(el.get<std::int64_t>())
                         : el.get<double>();
        if (idx < 0.0) {
          throw std::runtime_error("VectorProject '" + node.id +
                                   "' indices must be non-negative");
        }
        node.vector_indices.push_back(static_cast<std::size_t>(idx));
      }
      break;
    }
    case OpKind::Demux: {
      // FE OperatorJson.h reads numPorts (default 1).
      node.mux_num_ports = j.value("numPorts", 1);
      if (node.mux_num_ports < 1) {
        throw std::runtime_error("Demultiplexer '" + node.id +
                                 "' requires numPorts >= 1");
      }
      break;
    }
    case OpKind::Mux: {
      // FE OperatorJson.h reads numPorts (default 2).
      node.mux_num_ports = j.value("numPorts", 2);
      if (node.mux_num_ports < 1) {
        throw std::runtime_error("Multiplexer '" + node.id +
                                 "' requires numPorts >= 1");
      }
      break;
    }
    case OpKind::IIR: {
      auto b = j.at("b_coefficients").get<std::vector<double>>();
      auto a = j.at("a_coefficients").get<std::vector<double>>();
      node.b_len = b.size();
      node.a_len = a.size();
      node.coefficients.insert(node.coefficients.end(), b.begin(), b.end());
      node.coefficients.insert(node.coefficients.end(), a.begin(), a.end());
      break;
    }
    case OpKind::StateLoad:
      // source: id of the op whose first state slot to read.
      node.state_source_id = j.at("source").get<std::string>();
      break;
    case OpKind::Constant:
      node.constant_value = j.at("value").get<double>();
      break;
    case OpKind::TimeShift:
      node.time_shift = j.at("shift").get<std::int64_t>();
      break;
    case OpKind::LessThanOrEqualToReplace:
      // FE JSON convention: "value" is the threshold, "replaceBy" is the substitute.
      node.replace_threshold = j.at("value").get<double>();
      node.replace_by        = j.at("replaceBy").get<double>();
      break;
    case OpKind::Function: {
      auto pts = j.at("points")
                     .get<std::vector<std::pair<double, double>>>();
      if (pts.size() < 2) {
        throw std::runtime_error(
            "Function operator '" + node.id +
            "' requires at least 2 points");
      }
      std::sort(pts.begin(), pts.end());
      node.function_points = pts;
      node.function_use_hermite = false;
      if (j.contains("interpolation_type")) {
        const auto s = j.at("interpolation_type").get<std::string>();
        node.function_use_hermite = (s == "HERMITE");
      }
      // Pre-compute tangents (only used for Hermite). Mirrors
      // Function::get_tangent: forward/backward difference at endpoints,
      // central difference in the interior.
      const std::size_t N = pts.size();
      node.function_tangents.assign(N, 0.0);
      if (node.function_use_hermite) {
        node.function_tangents[0] =
            (pts[1].second - pts[0].second) /
            (pts[1].first  - pts[0].first);
        node.function_tangents[N - 1] =
            (pts[N - 1].second - pts[N - 2].second) /
            (pts[N - 1].first  - pts[N - 2].first);
        for (std::size_t k = 1; k + 1 < N; ++k) {
          node.function_tangents[k] =
              (pts[k + 1].second - pts[k - 1].second) /
              (pts[k + 1].first  - pts[k - 1].first);
        }
      }
      break;
    }
    // ArithmeticScalar: 1-input + "value"
    case OpKind::AddScalar:
    case OpKind::PowerScalar:
      node.scalar_value = j.at("value").get<double>();
      break;
    // CompareScalar: GT/LT/GTE/LTE take "value"; EQ/NEQ also take "tolerance"
    case OpKind::GtScalar:
    case OpKind::LtScalar:
    case OpKind::GteScalar:
    case OpKind::LteScalar:
      node.scalar_value = j.at("value").get<double>();
      break;
    case OpKind::EqScalar:
    case OpKind::NeqScalar:
      node.scalar_value = j.at("value").get<double>();
      node.tolerance    = j.value("tolerance", 0.0);
      break;
    // CompareSyncEQ/NEQ: tolerance only
    case OpKind::EqTol:
    case OpKind::NeqTol:
      node.tolerance    = j.value("tolerance", 0.0);
      break;
    // FilterScalar: GreaterThan/LessThan take "value" (threshold);
    // EqualTo/NotEqualTo take "value" + "epsilon".
    case OpKind::FiltGtScalar:
    case OpKind::FiltLtScalar:
      node.scalar_value = j.at("value").get<double>();
      break;
    case OpKind::FiltEqScalar:
    case OpKind::FiltNeqScalar:
      node.scalar_value = j.at("value").get<double>();
      node.tolerance    = j.value("epsilon", 1e-10);
      break;
    // FilterSync: SyncEqual/SyncNotEqual take "epsilon"; Sync{Greater,Less}Than no params
    case OpKind::FiltEqSync:
    case OpKind::FiltNeqSync:
      node.tolerance    = j.value("epsilon", 1e-10);
      break;
    case OpKind::Pipeline: {
      // Inline inner program: operators + connections + entryOperator + (no
      // Output op; instead outputMappings drives the outer pipeline ports).
      // Recurse through parse_program_json_obj to compile the inner graph.
      // Any unsupported inner op throws and is caught by Program's outer
      // try/catch, falling back to FE.
      node.pipeline_input_port_types =
          j.at("input_port_types").get<std::vector<std::string>>();
      node.pipeline_output_port_types =
          j.at("output_port_types").get<std::vector<std::string>>();

      if (j.contains("segmentBytecode") && j.at("segmentBytecode").is_array()) {
        node.pipeline_segment_bytecode =
            j.at("segmentBytecode").get<std::vector<double>>();
      }
      if (j.contains("segmentConstants") && j.at("segmentConstants").is_array()) {
        node.pipeline_segment_constants =
            j.at("segmentConstants").get<std::vector<double>>();
      }

      const std::size_t num_input_ports =
          node.pipeline_input_port_types.size();
      const std::size_t num_output_ports =
          node.pipeline_output_port_types.size();

      // join_num_ports drives the outer port-queue layout (Pipeline inherits
      // Join-shaped per-port queueing for its data inputs).
      node.join_num_ports = num_input_ports;

      // Build a synthetic inner-program JSON object so we can reuse
      // parse_program_json_obj. The inner JSON keeps its "operators",
      // "connections", and "entryOperator" fields and drops "outputMappings"
      // (which lives on the outer OpNode instead).
      nlohmann::json inner_j = nlohmann::json::object();
      inner_j["entryOperator"] = j.at("entryOperator");
      inner_j["operators"]     = j.at("operators");
      inner_j["connections"]   = j.at("connections");
      node.pipeline_inner_graph = std::make_shared<CompiledGraph>(
          parse_program_json_obj(inner_j));

      // Translate outputMappings to (outer_port_idx, (inner_op_id, inner_op_port)).
      if (j.contains("outputMappings")) {
        for (const auto& [op_id, port_map] : j.at("outputMappings").items()) {
          for (const auto& [op_port_name, outer_port_name] : port_map.items()) {
            const std::size_t inner_port =
                port_name_to_data_index(op_port_name);
            const std::size_t outer_port =
                port_name_to_data_index(
                    outer_port_name.template get<std::string>());
            node.pipeline_output_mappings.push_back(
                {outer_port, {op_id, inner_port}});
          }
        }
      }

      // Default output port widths from the declared output port types
      // (Pipeline currently only supports scalar output ports — width 1).
      // Leave node.port_widths empty; output_port_width() falls back to 1.
      (void)num_output_ports;
      break;
    }
    case OpKind::KeyedPipeline: {
      // KeyedPipeline: per-key dispatch. Key extraction is either a single
      // lane index ("key_index") or a polynomial hash over selected lanes
      // ("keyColumnIndices"). The prototype subgraph has its own operators,
      // connections, entry, and output op — we recursively compile it as
      // the inner sub-program.
      if (j.contains("keyColumnIndices")) {
        node.keyed_pipeline_key_column_indices =
            j.at("keyColumnIndices").get<std::vector<int>>();
        if (node.keyed_pipeline_key_column_indices.empty()) {
          throw std::runtime_error(
              "KeyedPipeline '" + node.id +
              "' keyColumnIndices must be non-empty");
        }
        for (int idx : node.keyed_pipeline_key_column_indices) {
          if (idx < 0) {
            throw std::runtime_error(
                "KeyedPipeline '" + node.id +
                "' keyColumnIndices entries must be non-negative");
          }
        }
        // Pre-compute polynomial hash coefficients PRIME^(N-1), ..., 1.
        constexpr double kKeyedPipelinePrime = 1000003.0;
        const std::size_t N = node.keyed_pipeline_key_column_indices.size();
        node.keyed_pipeline_key_coefficients.assign(N, 0.0);
        double coeff = 1.0;
        for (std::size_t i = 0; i < N; ++i) {
          node.keyed_pipeline_key_coefficients[N - 1 - i] = coeff;
          coeff *= kKeyedPipelinePrime;
        }
        node.keyed_pipeline_key_index = -1;
      } else {
        node.keyed_pipeline_key_index = j.at("key_index").get<int>();
        if (node.keyed_pipeline_key_index < 0) {
          throw std::runtime_error(
              "KeyedPipeline '" + node.id +
              "' key_index must be non-negative");
        }
      }

      const auto& proto = j.at("prototype");
      // Build a Pipeline-shaped inner JSON object so we can reuse
      // parse_program_json_obj. The prototype is required to contain its own
      // Input/Output ops; we treat it as a self-contained program.
      nlohmann::json inner_j = nlohmann::json::object();
      inner_j["operators"]   = proto.at("operators");
      inner_j["connections"] = proto.at("connections");
      // The prototype's entry op id may live under "entry.operator" (FE shape)
      // or "entryOperator" (Pipeline shape).
      if (proto.contains("entry") && proto.at("entry").contains("operator")) {
        inner_j["entryOperator"] =
            proto.at("entry").at("operator").get<std::string>();
      } else {
        inner_j["entryOperator"] = proto.at("entryOperator");
      }
      // Output collection mapping is required for emit_inner_program. The FE
      // shape declares the output op id under "output.operator"; mirror Pipeline
      // by collecting all of that op's data input ports.
      std::string proto_output_op_id;
      if (proto.contains("output") && proto.at("output").contains("operator")) {
        proto_output_op_id =
            proto.at("output").at("operator").get<std::string>();
      } else if (proto.contains("output")) {
        // Fall back: "output" is a map { op_id: [port_names] } already.
        inner_j["output"] = proto.at("output");
      }
      if (!proto_output_op_id.empty()) {
        // Find the prototype output op to enumerate its data inputs.
        // The output op declares its own port_types; collect every port name.
        std::vector<std::string> port_names;
        for (const auto& op_json : proto.at("operators")) {
          if (op_json.value("id", std::string{}) == proto_output_op_id) {
            std::size_t n_ports = 0;
            if (op_json.contains("portTypes")) {
              n_ports = op_json.at("portTypes").size();
            }
            if (n_ports == 0) n_ports = 1;
            port_names.reserve(n_ports);
            for (std::size_t p = 0; p < n_ports; ++p) {
              port_names.push_back("o" + std::to_string(p + 1));
            }
            break;
          }
        }
        if (port_names.empty()) port_names.push_back("o1");
        nlohmann::json out_obj = nlohmann::json::object();
        out_obj[proto_output_op_id] = port_names;
        inner_j["output"] = out_obj;
      }

      auto inner_graph =
          std::make_shared<CompiledGraph>(parse_program_json_obj(inner_j));

      // Walk the inner graph to find its Output op and harvest its port_types
      // → these become the KeyedPipeline's outer output port types.
      // Compute outputMappings: every connection feeding the inner Output op
      // is one (outer_port, (inner_op_id, inner_port)) entry.
      std::string inner_output_op_id = proto_output_op_id;
      if (inner_output_op_id.empty()) {
        for (const auto& nn : inner_graph->nodes) {
          if (nn.kind == OpKind::Output) {
            inner_output_op_id = nn.id;
            break;
          }
        }
      }
      if (inner_output_op_id.empty()) {
        throw std::runtime_error(
            "KeyedPipeline '" + node.id +
            "' prototype must contain an Output op");
      }

      // Inner output port types and inner output port count drive the outer
      // KeyedPipeline output port types (with key prepended in simple-key mode).
      std::vector<std::string> inner_output_types;
      for (const auto& nn : inner_graph->nodes) {
        if (nn.id == inner_output_op_id) {
          inner_output_types = nn.port_types;
          break;
        }
      }
      if (inner_output_types.empty()) inner_output_types.push_back("number");

      // Build the output mapping: each connection feeding the inner Output's
      // data input port becomes (outer_port, (from_id, from_port)).
      for (const auto& conn : inner_graph->connections) {
        if (conn.to_id != inner_output_op_id) continue;
        if (conn.to_kind != PortKind::Data) continue;
        node.keyed_pipeline_output_mappings.push_back(
            {conn.to_port, {conn.from_id, conn.from_port}});
      }
      // Strip the inner Output op + its incoming connections so the inner
      // graph matches the shape emit_inner_program expects (no Output op;
      // emissions are routed via outer_output_mappings). Also drop the
      // graph-level "outputs" map for that op.
      {
        auto& nodes = inner_graph->nodes;
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                                    [&](const OpNode& n) {
                                      return n.id == inner_output_op_id;
                                    }),
                    nodes.end());
      }
      {
        auto& conns = inner_graph->connections;
        conns.erase(std::remove_if(conns.begin(), conns.end(),
                                    [&](const Connection& c) {
                                      return c.to_id == inner_output_op_id;
                                    }),
                    conns.end());
      }
      inner_graph->outputs.erase(inner_output_op_id);

      // Detect a prototype-declared Input op (FE-shape prototypes always
      // include one). emit_inner_program rewrites its outgoing connections
      // to use its synthetic Input adapter and drops the prototype Input
      // node from the transformed graph. We just remember its id here.
      // (Stored implicitly by leaving inner_graph->entry_op_id as the
      // prototype Input — emit_inner_program detects it via the nodes list.)

      // KeyedPipeline always feeds the inner with the FULL input vector (the
      // FE forwards the same VectorNumberData message into the prototype).
      // We treat the inner as a Pipeline-shape inner with a single VECTOR
      // input port; the lane width is resolved post-parse from the upstream
      // node feeding our data port 0.
      node.keyed_pipeline_inner_graph = inner_graph;
      node.keyed_pipeline_output_port_types = inner_output_types;

      // Outer port-queue layout: a single VECTOR_NUMBER input port whose
      // lane count is resolved post-parse. join_num_ports drives the queue
      // count (one queue per lane, matching segment-bytecode-mode Pipeline).
      node.join_num_ports = 0;  // resolved post-parse to the lane width

      // Output port_widths are set during the post-parse width-resolution pass
      // once the upstream lane width is known (simple key prepends the key).
      break;
    }
    case OpKind::MovingKeyCount: {
      const auto W = j.at("window_size").get<std::size_t>();
      if (W < 1) {
        throw std::runtime_error("MovingKeyCount '" + node.id +
                                 "' window_size must be >= 1");
      }
      node.mkc_window_size = W;
      break;
    }
    case OpKind::TopK: {
      const int k = j.at("k").get<int>();
      const int sidx = j.at("score_index").get<int>();
      if (k <= 0) {
        throw std::runtime_error("TopK '" + node.id + "' requires k > 0");
      }
      if (sidx < 0) {
        throw std::runtime_error("TopK '" + node.id +
                                 "' requires score_index >= 0");
      }
      node.topk_k = static_cast<std::size_t>(k);
      node.topk_score_index = static_cast<std::size_t>(sidx);
      // FE accepts boolean or string-bool. Accept both for robustness.
      if (j.contains("descending")) {
        const auto& dv = j.at("descending");
        if (dv.is_boolean())      node.topk_descending = dv.get<bool>();
        else if (dv.is_string())  node.topk_descending = (dv.get<std::string>() == "true");
        else                      node.topk_descending = true;
      } else {
        node.topk_descending = true;
      }
      // row_width is resolved from upstream wire width during a post-parse
      // walk (see parse_program_json). Default 1 (scalar input).
      node.topk_row_width = 1;
      break;
    }
    default:
      // Stateless operators and zero-parameter stateful ops (CumSum, Count,
      // MaxAgg, MinAgg, SignChange, Gate, Diff) need no extra fields.
      break;
  }

  // Optional per-port scalar width metadata. The current FE does not emit this
  // field for any of the JIT-supported opcodes (all ports are width 1, encoded
  // implicitly by "number"/"boolean" in portTypes — "vector_number" cannot be
  // JIT-compiled today because its width is not known at compile time).
  // Future vector-aware ops (VectorCompose / Extract / Project / TopK) will
  // attach an explicit "portWidths" array so the JIT can size its slot layout.
  if (j.contains("portWidths")) {
    node.port_widths = j.at("portWidths").get<std::vector<std::size_t>>();
  }

  // Auto-default output port widths for the vector-typed opcodes when the
  // FE has not provided an explicit "portWidths" field.
  if (node.port_widths.empty()) {
    switch (node.kind) {
      case OpKind::VectorCompose:
        node.port_widths = {node.join_num_ports};
        break;
      case OpKind::FusedExpression:
      case OpKind::FusedExpressionVector:
        node.port_widths = {node.fe_num_outputs};
        break;
      case OpKind::BurstAggregate:
        node.port_widths = {node.ba_key_columns.size() +
                            node.ba_num_agg_outputs};
        break;
      case OpKind::VectorProject:
        node.port_widths = {node.vector_indices.size()};
        break;
      case OpKind::VectorExtract:
        node.port_widths = {1};
        break;
      default:
        break;
    }
  }

  return node;
}

Connection parse_connection(const nlohmann::json& j) {
  Connection conn;
  conn.from_id   = j.at("from").get<std::string>();
  conn.to_id     = j.at("to").get<std::string>();
  auto [fp, fk]  = port_name_to_index(j.at("fromPort").get<std::string>());
  auto [tp, tk]  = port_name_to_index(j.at("toPort").get<std::string>());
  conn.from_port = fp;
  conn.from_kind = fk;
  conn.to_port   = tp;
  conn.to_kind   = tk;
  return conn;
}

CompiledGraph parse_program_json_obj(const nlohmann::json& j) {
  CompiledGraph graph;

  graph.entry_op_id = j.at("entryOperator").get<std::string>();

  // Composite operators (e.g. RelativeStrengthIndex) expand at parse time
  // into a sub-graph of primitive ops. We collect the external composite id
  // -> (input_adapter_id, output_adapter_id) mapping here; afterwards we
  // rewrite any connections / output mappings that reference the composite
  // id so downstream stages (StateLayout, SegmentPartitioner, SegmentEmitter)
  // see only primitives.
  std::unordered_map<std::string, CompositeAdapter> composite_adapters;

  // Parse operators.
  for (const auto& op_json : j.at("operators")) {
    const auto type_str = op_json.at("type").get<std::string>();
    if (is_composite_type(type_str)) {
      const auto ext_id = op_json.at("id").get<std::string>();
      if (type_str == "RelativeStrengthIndex") {
        composite_adapters[ext_id] = expand_rsi(graph, op_json);
      }
      continue;
    }
    graph.nodes.push_back(parse_op_node(op_json));
  }

  // Parse connections, rewriting endpoints that reference a composite id
  // to point at the corresponding adapter op inside the expanded sub-graph.
  for (const auto& conn_json : j.at("connections")) {
    Connection conn = parse_connection(conn_json);
    if (auto it = composite_adapters.find(conn.from_id);
        it != composite_adapters.end()) {
      conn.from_id = it->second.output_adapter_id;
    }
    if (auto it = composite_adapters.find(conn.to_id);
        it != composite_adapters.end()) {
      conn.to_id = it->second.input_adapter_id;
    }
    graph.connections.push_back(conn);
  }

  // Parse output mapping (optional field; not all programs define it).
  // Composite ids in the output mapping are also rewritten.
  if (j.contains("output")) {
    for (const auto& [op_id, port_list] : j.at("output").items()) {
      auto it = composite_adapters.find(op_id);
      const std::string& target = (it == composite_adapters.end())
          ? op_id
          : it->second.output_adapter_id;
      graph.outputs[target] = port_list.get<std::vector<std::string>>();
    }
  }

  // The entry op id may also reference a composite — rewrite to its input
  // adapter so the downstream pipeline knows where data lands.
  if (auto it = composite_adapters.find(graph.entry_op_id);
      it != composite_adapters.end()) {
    graph.entry_op_id = it->second.input_adapter_id;
  }

  // Resolve TopK row_width from the upstream node's output port width on the
  // connected from_port. Defaults to 1 when the upstream is scalar.
  std::unordered_map<std::string, std::size_t> id_to_idx;
  id_to_idx.reserve(graph.nodes.size());
  for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
    id_to_idx[graph.nodes[i].id] = i;
  }

  // Infer Input op output width from downstream vector-wire consumers. Run
  // BEFORE Pipeline / KeyedPipeline lane-width resolution so those passes see
  // the inferred width on their upstream Input op. FE's program JSON does not
  // stamp portWidths on the Input op; the JIT relies on that width to emit a
  // `const double* in_v_arr` signature for vector-input programs.
  //   - FusedExpressionVector: lane count = max INPUT lane referenced + 1.
  //   - BurstAggregate:        lane count = numInputCols.
  //   - KeyedPipeline:         max(keyColumnIndices)+1, plus the inner
  //                            prototype's first-consumer requirement (the
  //                            inner BurstAggregate / FEV downstream of the
  //                            proto Input).
  auto infer_kp_required_lane_width =
      [&](const OpNode& kp_node) -> std::size_t {
    std::size_t need = 0;
    for (int idx : kp_node.keyed_pipeline_key_column_indices) {
      const std::size_t n = static_cast<std::size_t>(idx) + 1;
      if (n > need) need = n;
    }
    if (!kp_node.keyed_pipeline_inner_graph) return need;
    const CompiledGraph& inner = *kp_node.keyed_pipeline_inner_graph;
    // Find the prototype Input op id, then walk inner connections from it to
    // direct vector-consumer ops.
    std::string proto_input_id;
    for (const auto& nn : inner.nodes) {
      if (nn.kind == OpKind::Input) { proto_input_id = nn.id; break; }
    }
    if (proto_input_id.empty()) return need;
    std::unordered_map<std::string, std::size_t> inner_id_to_idx;
    inner_id_to_idx.reserve(inner.nodes.size());
    for (std::size_t i = 0; i < inner.nodes.size(); ++i) {
      inner_id_to_idx[inner.nodes[i].id] = i;
    }
    for (const auto& conn : inner.connections) {
      if (conn.from_id != proto_input_id ||
          conn.from_kind != PortKind::Data) continue;
      if (conn.from_port != 0) continue;
      auto it = inner_id_to_idx.find(conn.to_id);
      if (it == inner_id_to_idx.end()) continue;
      const OpNode& dn = inner.nodes[it->second];
      if (dn.kind == OpKind::FusedExpressionVector) {
        auto pack = rtbot::fuse::pack_bytecode(dn.fe_bytecode);
        for (const auto& ins : pack.packed) {
          if (ins.op == static_cast<std::uint8_t>(rtbot::fused_op::INPUT)) {
            const std::size_t n = static_cast<std::size_t>(ins.arg) + 1;
            if (n > need) need = n;
          }
        }
      } else if (dn.kind == OpKind::BurstAggregate) {
        if (dn.ba_num_input_cols > need) need = dn.ba_num_input_cols;
      }
    }
    return need;
  };

  for (auto& node : graph.nodes) {
    if (node.kind != OpKind::Input) continue;
    if (!node.port_widths.empty() && node.port_widths[0] > 1) continue;
    std::size_t inferred_w = 0;
    for (const auto& conn : graph.connections) {
      if (conn.from_id != node.id || conn.from_kind != PortKind::Data) continue;
      if (conn.from_port != 0) continue;
      auto it = id_to_idx.find(conn.to_id);
      if (it == id_to_idx.end()) continue;
      const OpNode& dn = graph.nodes[it->second];
      if (dn.kind == OpKind::FusedExpressionVector) {
        std::size_t need = 0;
        auto pack = rtbot::fuse::pack_bytecode(dn.fe_bytecode);
        for (const auto& ins : pack.packed) {
          if (ins.op == static_cast<std::uint8_t>(rtbot::fused_op::INPUT)) {
            const std::size_t n = static_cast<std::size_t>(ins.arg) + 1;
            if (n > need) need = n;
          }
        }
        if (need > inferred_w) inferred_w = need;
      } else if (dn.kind == OpKind::BurstAggregate) {
        if (dn.ba_num_input_cols > inferred_w) {
          inferred_w = dn.ba_num_input_cols;
        }
      } else if (dn.kind == OpKind::KeyedPipeline) {
        const std::size_t need = infer_kp_required_lane_width(dn);
        if (need > inferred_w) inferred_w = need;
      }
    }
    if (inferred_w > 1) {
      if (node.port_widths.empty()) node.port_widths.assign(1, 0);
      node.port_widths[0] = inferred_w;
    }
  }

  for (auto& node : graph.nodes) {
    if (node.kind != OpKind::TopK) continue;
    for (const auto& conn : graph.connections) {
      if (conn.to_id != node.id || conn.to_kind != PortKind::Data) continue;
      if (conn.to_port != 0) continue;
      auto it = id_to_idx.find(conn.from_id);
      if (it == id_to_idx.end()) continue;
      const OpNode& up = graph.nodes[it->second];
      const std::size_t w = up.output_port_width(conn.from_port);
      node.topk_row_width = (w == 0 ? 1 : w);
      break;
    }
    if (node.topk_score_index >= node.topk_row_width) {
      throw std::runtime_error(
          "TopK '" + node.id + "' score_index " +
          std::to_string(node.topk_score_index) +
          " out of bounds for row width " +
          std::to_string(node.topk_row_width));
    }
  }

  // Resolve Pipeline (segment-bytecode mode) input lane width from the
  // upstream node feeding data port 0. The upstream is expected to be a
  // vector-producing sync op (VectorCompose, FusedExpression, VectorProject)
  // whose output_port_width carries the lane count.
  for (auto& node : graph.nodes) {
    if (node.kind != OpKind::Pipeline) continue;
    if (node.pipeline_segment_bytecode.empty()) continue;
    for (const auto& conn : graph.connections) {
      if (conn.to_id != node.id || conn.to_kind != PortKind::Data) continue;
      if (conn.to_port != 0) continue;
      auto it = id_to_idx.find(conn.from_id);
      if (it == id_to_idx.end()) continue;
      const OpNode& up = graph.nodes[it->second];
      const std::size_t w = up.output_port_width(conn.from_port);
      node.pipeline_input_lane_width = (w == 0 ? 1 : w);
      break;
    }
    if (node.pipeline_input_lane_width == 0) {
      throw std::runtime_error(
          "Pipeline '" + node.id +
          "' segment-bytecode mode requires a vector input on data port 0");
    }
    // join_num_ports drives the outer port-queue count. Replace the value
    // set during parse_op_node (which was the input port count = 1 for a
    // single vector_number port) with the lane count, so the Pipeline gets
    // one port queue per lane.
    node.join_num_ports = node.pipeline_input_lane_width;
  }

  // Resolve KeyedPipeline input lane width from the upstream node feeding
  // data port 0. Mirrors the Pipeline segment-bytecode resolution above —
  // the KeyedPipeline always treats its single input as a VECTOR_NUMBER
  // wire (the FE feeds the full vector into the prototype). The outer Output
  // op's port_widths are also stamped here (simple-key mode prepends the key).
  for (auto& node : graph.nodes) {
    if (node.kind != OpKind::KeyedPipeline) continue;
    for (const auto& conn : graph.connections) {
      if (conn.to_id != node.id || conn.to_kind != PortKind::Data) continue;
      if (conn.to_port != 0) continue;
      auto it = id_to_idx.find(conn.from_id);
      if (it == id_to_idx.end()) continue;
      const OpNode& up = graph.nodes[it->second];
      const std::size_t w = up.output_port_width(conn.from_port);
      node.keyed_pipeline_input_lane_width = (w == 0 ? 1 : w);
      break;
    }
    if (node.keyed_pipeline_input_lane_width == 0) {
      throw std::runtime_error(
          "KeyedPipeline '" + node.id +
          "' requires a vector input on data port 0");
    }
    if (node.keyed_pipeline_key_index >= 0 &&
        static_cast<std::size_t>(node.keyed_pipeline_key_index) >=
            node.keyed_pipeline_input_lane_width) {
      throw std::runtime_error(
          "KeyedPipeline '" + node.id + "' key_index " +
          std::to_string(node.keyed_pipeline_key_index) +
          " out of bounds for input lane width " +
          std::to_string(node.keyed_pipeline_input_lane_width));
    }
    for (int idx : node.keyed_pipeline_key_column_indices) {
      if (static_cast<std::size_t>(idx) >=
          node.keyed_pipeline_input_lane_width) {
        throw std::runtime_error(
            "KeyedPipeline '" + node.id + "' keyColumnIndices entry " +
            std::to_string(idx) +
            " out of bounds for input lane width " +
            std::to_string(node.keyed_pipeline_input_lane_width));
      }
    }
    // One port queue per lane, matching segment-bytecode-mode Pipeline.
    node.join_num_ports = node.keyed_pipeline_input_lane_width;
    // Stamp the outer output port width — simple-key mode prepends the key
    // so width = 1 + inner_output_width; computed-key passes through directly.
    // The inner output width is taken from the actual upstream op feeding the
    // (now-stripped) inner Output op via keyed_pipeline_output_mappings, since
    // the inner port type list only counts ports — not lane widths.
    std::size_t inner_out_w = 0;
    if (node.keyed_pipeline_inner_graph) {
      const CompiledGraph& inner = *node.keyed_pipeline_inner_graph;
      std::unordered_map<std::string, std::size_t> inner_id_to_idx;
      inner_id_to_idx.reserve(inner.nodes.size());
      for (std::size_t i = 0; i < inner.nodes.size(); ++i) {
        inner_id_to_idx[inner.nodes[i].id] = i;
      }
      for (const auto& [outer_port, src] : node.keyed_pipeline_output_mappings) {
        auto it = inner_id_to_idx.find(src.first);
        if (it == inner_id_to_idx.end()) continue;
        const std::size_t w =
            inner.nodes[it->second].output_port_width(src.second);
        inner_out_w += (w == 0 ? 1 : w);
      }
    }
    if (inner_out_w == 0) {
      inner_out_w = node.keyed_pipeline_output_port_types.size();
    }
    const bool simple_key = (node.keyed_pipeline_key_index >= 0);
    const std::size_t outer_w =
        simple_key ? (1 + inner_out_w) : inner_out_w;
    node.port_widths = {outer_w};
  }

  // Session shape: when the program JSON declares terminals via the top-level
  // "output" map but does NOT contain an OpKind::Output node (rtbot-sql's
  // compile_session_program produces this for fused multi-view programs),
  // synthesize an Output op so the rest of the JIT pipeline (SegmentEmitter,
  // StateLayout, partitioner) can treat the graph identically to a single-
  // terminal program. The synthesized Output collects every (terminal_op,
  // port_name) entry of graph.outputs in iteration order; each entry becomes
  // one input port of the synth Output. Per-port widths are inferred from
  // the upstream op's output_port_width, so vector terminals (KeyedPipeline,
  // FusedExpressionVector, BurstAggregate, VectorCompose) fan out across
  // consecutive flat slots like their single-terminal counterparts.
  bool has_physical_output = false;
  for (const auto& node : graph.nodes) {
    if (node.kind == OpKind::Output) { has_physical_output = true; break; }
  }
  if (!has_physical_output && !graph.outputs.empty()) {
    OpNode synth_out;
    // Pick an id that cannot collide with any existing op id.
    synth_out.id = "__rtbot_jit_synth_output__";
    while (id_to_idx.count(synth_out.id)) {
      synth_out.id += "_";
    }
    synth_out.kind = OpKind::Output;

    // Rebuild graph.outputs against the synthetic op while we walk the
    // existing entries. The original (terminal_op, port_list) shape is kept
    // intact in `synth_terminals` for translate_jit_to_batch_ to reconstruct.
    std::map<std::string, std::vector<std::string>> new_outputs;
    new_outputs[synth_out.id] = {};

    std::size_t to_port_cursor = 0;
    for (const auto& [term_op_id, port_names] : graph.outputs) {
      auto it = id_to_idx.find(term_op_id);
      if (it == id_to_idx.end()) {
        throw std::runtime_error(
            "JsonParser: outputs map references unknown op '" + term_op_id +
            "'");
      }
      const OpNode& src = graph.nodes[it->second];
      for (const auto& port_name : port_names) {
        // Parse "o1"/"o2"/... -> 0-based index. Mirrors
        // OperatorJson::parse_port_name without dragging the api dep.
        std::size_t from_port = 0;
        if (port_name.size() >= 2 &&
            (port_name[0] == 'o' || port_name[0] == 'i' ||
             port_name[0] == 'c')) {
          try {
            const long n = std::stol(port_name.substr(1));
            from_port = (n > 0) ? static_cast<std::size_t>(n - 1) : 0;
          } catch (...) {
            from_port = 0;
          }
        }
        const std::size_t w = src.output_port_width(from_port);
        synth_out.port_types.push_back(w > 1 ? "vector_number" : "number");
        synth_out.port_widths.push_back(w);

        Connection conn;
        conn.from_id   = term_op_id;
        conn.from_port = from_port;
        conn.to_id     = synth_out.id;
        conn.to_port   = to_port_cursor;
        conn.from_kind = PortKind::Data;
        conn.to_kind   = PortKind::Data;
        graph.connections.push_back(conn);

        // The synthesized Output port name is "o{cursor+1}" so it lines up
        // with the standard 1-based port-name convention used by the FE.
        new_outputs[synth_out.id].push_back(
            "o" + std::to_string(to_port_cursor + 1));
        ++to_port_cursor;
      }
    }

    id_to_idx[synth_out.id] = graph.nodes.size();
    graph.nodes.push_back(std::move(synth_out));
    graph.outputs = std::move(new_outputs);
  }

  // Stamp the outer Output op's port_widths for any port fed by a
  // vector-producing op (KeyedPipeline / FusedExpressionVector /
  // BurstAggregate). The Output op declares a single VECTOR_NUMBER port whose
  // width is computed by the upstream op (key-prepended for KeyedPipeline,
  // numOutputs for FEV, key_columns + num_agg_outputs for BurstAggregate).
  for (auto& node : graph.nodes) {
    if (node.kind != OpKind::Output) continue;
    for (const auto& conn : graph.connections) {
      if (conn.to_id != node.id || conn.to_kind != PortKind::Data) continue;
      auto it = id_to_idx.find(conn.from_id);
      if (it == id_to_idx.end()) continue;
      const OpNode& up = graph.nodes[it->second];
      if (up.kind != OpKind::KeyedPipeline &&
          up.kind != OpKind::FusedExpressionVector &&
          up.kind != OpKind::BurstAggregate) {
        continue;
      }
      const std::size_t w = up.output_port_width(conn.from_port);
      if (node.port_widths.size() <= conn.to_port) {
        node.port_widths.resize(conn.to_port + 1, 0);
      }
      // Only stamp when the user has not supplied an explicit width.
      if (node.port_widths[conn.to_port] == 0) {
        node.port_widths[conn.to_port] = w;
      }
    }
  }

  return graph;
}

}  // namespace

CompiledGraph parse_program_json(const std::string& json_str) {
  return parse_program_json_obj(nlohmann::json::parse(json_str));
}

}  // namespace rtbot::jit
