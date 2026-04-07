#ifndef RTBOT_OPERATOR_JSON_H
#define RTBOT_OPERATOR_JSON_H

#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "rtbot/Demultiplexer.h"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Message.h"
#include "rtbot/Multiplexer.h"
#include "rtbot/Operator.h"
#include "rtbot/Output.h"
#include "rtbot/Pipeline.h"
#include "rtbot/TriggerSet.h"
#include "rtbot/std/ArithmeticScalar.h"
#include "rtbot/std/ArithmeticSync.h"
#include "rtbot/std/BooleanSync.h"
#include "rtbot/std/Constant.h"
#include "rtbot/std/Count.h"
#include "rtbot/std/CumulativeSum.h"
#include "rtbot/std/Difference.h"
#include "rtbot/std/FilterScalar.h"
#include "rtbot/std/FilterSync.h"
#include "rtbot/std/FiniteImpulseResponse.h"
#include "rtbot/std/Function.h"
#include "rtbot/std/Identity.h"
#include "rtbot/std/InfiniteImpulseResponse.h"
#include "rtbot/std/Linear.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/MovingSum.h"
#include "rtbot/std/PeakDetector.h"
#include "rtbot/std/Replace.h"
#include "rtbot/std/ResamplerConstant.h"
#include "rtbot/std/ResamplerHermite.h"
#include "rtbot/std/StandardDeviation.h"
#include "rtbot/std/TimeShift.h"
#include "rtbot/std/CompareScalar.h"
#include "rtbot/std/Variable.h"
#include "rtbot/std/KeyedPipeline.h"
#include "rtbot/std/KeyedVariable.h"
#include "rtbot/std/VectorCompose.h"
#include "rtbot/std/VectorExtract.h"
#include "rtbot/std/VectorProject.h"
#include "rtbot/std/CompareSync.h"
#include "rtbot/std/MovingKeyCount.h"
#include "rtbot/std/BooleanToNumber.h"
#include "rtbot/std/MinMaxTracker.h"
#include "rtbot/std/TopK.h"
#include "rtbot/std/WindowMinMax.h"

using json = nlohmann::json;

namespace rtbot {

struct ParsedPort {
  size_t index;
  PortKind kind;
};

class OperatorJson {
 public:
  static ParsedPort parse_port_name(const std::string& port_name) {
    if (port_name.empty()) return {0, PortKind::DATA};

    char type = port_name[0];
    try {
      size_t index = stoul(port_name.substr(1)) - 1;
      return {index, (type == 'c' ? PortKind::CONTROL : PortKind::DATA)};
    } catch (...) {
      throw std::runtime_error("Invalid port name format: " + port_name);
    }
  }

  static std::shared_ptr<Operator> read_op(std::string const& json_string) {
    auto parsed = json::parse(json_string);
    auto type = parsed["type"].get<std::string>();
    auto id = parsed["id"].get<std::string>();
    auto num_ports = parsed.value("numPorts", 2);
    auto max_size_per_port = parsed.value("maxSizePerPort", MAX_SIZE_PER_PORT);

    if (type == "Input") {
      return make_input(id, parsed["portTypes"].get<std::vector<std::string>>());
    } else if (type == "Output") {
      return make_output(id, parsed["portTypes"].get<std::vector<std::string>>());
    } else if (type == "MovingAverage") {
      return make_moving_average(id, parsed["window_size"].get<size_t>());
    } else if (type == "MovingSum") {
      return make_moving_sum(id, parsed["window_size"].get<size_t>());
    } else if (type == "StandardDeviation") {
      return make_std_dev(id, parsed["window_size"].get<size_t>());
    } else if (type == "FiniteImpulseResponse") {
      return make_fir(id, parsed["coeff"].get<std::vector<double>>());
    } else if (type == "InfiniteImpulseResponse") {
      return make_iir(id, parsed["b_coeffs"].get<std::vector<double>>(), parsed["a_coeffs"].get<std::vector<double>>());
    } else if (type == "Join") {
      return make_join(id, parsed["portTypes"].get<std::vector<std::string>>(), max_size_per_port);
    } else if (type == "Difference") {
      return make_difference(id);
    } else if (type == "PeakDetector") {
      return make_peak_detector(id, parsed["window_size"].get<size_t>());
    } else if (type == "Linear") {
      return make_linear(id, parsed["coefficients"].get<std::vector<double>>(), max_size_per_port);
    } else if (type == "Subtraction") {
      return make_subtraction(id, num_ports, max_size_per_port);
    } else if (type == "LogicalAnd") {
      return make_logical_and(id, num_ports, max_size_per_port);
    } else if (type == "LogicalOr") {
      return make_logical_or(id, num_ports, max_size_per_port);
    } else if (type == "LogicalXor") {
      return make_logical_xor(id, num_ports, max_size_per_port);
    } else if (type == "LogicalNand") {
      return make_logical_nand(id, num_ports, max_size_per_port);
    } else if (type == "LogicalNor") {
      return make_logical_nor(id, num_ports, max_size_per_port);
    } else if (type == "LogicalImplication") {
      return make_logical_implication(id, num_ports, max_size_per_port);
    } else if (type == "SyncGreaterThan") {
      return make_sync_greater_than(id, num_ports, max_size_per_port);
    } else if (type == "SyncLessThan") {
      return make_sync_less_than(id, num_ports, max_size_per_port);
    } else if (type == "Scale") {
      return make_scale(id, parsed["value"].get<double>());
    } else if (type == "Power") {
      return make_power(id, parsed["value"].get<double>());
    } else if (type == "Add") {
      return make_add(id, parsed["value"].get<double>());
    } else if (type == "Division") {
      return make_division(id, num_ports, max_size_per_port);
    } else if (type == "Multiplication") {
      return make_multiplication(id, num_ports, max_size_per_port);
    } else if (type == "Addition") {
      return make_addition(id, num_ports, max_size_per_port);
    } else if (type == "GreaterThan") {
      return make_greater_than(id, parsed["value"].get<double>());
    } else if (type == "Function") {
      return make_function(id, parsed["points"].get<std::vector<std::pair<double, double>>>());
    } else if (type == "ConstantNumber") {
      return make_constant_number(id, parsed["value"].get<double>());
    } else if (type == "ConstantBoolean") {
      return make_constant_boolean(id, parsed["value"].get<bool>());
    } else if (type == "ConstantNumberToBoolean") {
      return make_constant_number_to_boolean(id, parsed["value"].get<bool>());
    } else if (type == "ConstantBooleanToNumber") {
      return make_constant_boolean_to_number(id, parsed["value"].get<double>());
    } else if (type == "LessThan") {
      return make_less_than(id, parsed["value"].get<double>());
    } else if (type == "LessThanOrEqualToReplace") {
      return make_less_than_or_equal_to_replace(id, parsed["value"].get<double>(), parsed["replaceBy"].get<double>());
    } else if (type == "EqualTo") {
      return make_equal_to(id, parsed["value"].get<double>(), parsed.value("epsilon", 1e-10));
    } else if (type == "NotEqualTo") {
      return make_not_equal_to(id, parsed["value"].get<double>(), parsed.value("epsilon", 1e-10));
    } else if (type == "SyncEqual") {
      return make_sync_equal(id, num_ports, parsed.value("epsilon", 1e-10), max_size_per_port);
    } else if (type == "SyncNotEqual") {
      return make_sync_not_equal(id, num_ports, parsed.value("epsilon", 1e-10), max_size_per_port);
    } else if (type == "Sin") {
      return make_sin(id);
    } else if (type == "Cos") {
      return make_cos(id);
    } else if (type == "Tan") {
      return make_tan(id);
    } else if (type == "Exp") {
      return make_exp(id);
    } else if (type == "Log") {
      return make_log(id);
    } else if (type == "Log10") {
      return make_log10(id);
    } else if (type == "Abs") {
      return make_abs(id);
    } else if (type == "Sign") {
      return make_sign(id);
    } else if (type == "Floor") {
      return make_floor(id);
    } else if (type == "Ceil") {
      return make_ceil(id);
    } else if (type == "Round") {
      return make_round(id);
    } else if (type == "Variable") {
      return make_variable(id, parsed.value("default_value", 0.0), max_size_per_port);
    } else if (type == "KeyedVariable") {
      return make_keyed_variable(id, parsed.value("mode", "exists"), parsed.value("default_value", 0.0));
    } else if (type == "TimeShift") {
      return make_time_shift(id, parsed["shift"].get<int>());
    } else if (type == "CumulativeSum") {
      return make_cumulative_sum(id);
    } else if (type == "CountNumber") {
      return make_count_number(id);
    } else if (type == "Demultiplexer") {
      auto port_type = parsed.value("portType", "number");
      auto num = parsed.value("numPorts", 1);
      if (port_type == "vector_number") return make_demultiplexer_vector_number(id, num, max_size_per_port);
      if (port_type == "boolean") return make_demultiplexer_boolean(id, num, max_size_per_port);
      if (port_type == "vector_boolean") return make_demultiplexer_vector_boolean(id, num, max_size_per_port);
      return make_demultiplexer_number(id, num, max_size_per_port);
    } else if (type == "Multiplexer") {
      return make_multiplexer_number(id, parsed.value("numPorts", 2));
    } else if (type == "ResamplerConstant") {
      if (parsed.contains("t0")) {
        return make_resampler_constant(id, parsed["interval"].get<int>(), parsed["t0"].get<double>());
      }
      return make_resampler_constant(id, parsed["interval"].get<int>());
    } else if (type == "ResamplerHermite") {
      return make_resampler_hermite(id, parsed["interval"].get<int>());
    } else if (type == "VectorExtract") {
      return make_vector_extract(id, parsed["index"].get<int>());
    } else if (type == "VectorProject") {
      return make_vector_project(id, parsed["indices"].get<std::vector<int>>());
    } else if (type == "VectorCompose") {
      return make_vector_compose(id, parsed["numPorts"].get<size_t>());
    } else if (type == "CompareGT") {
      return make_compare_gt(id, parsed["value"].get<double>());
    } else if (type == "CompareLT") {
      return make_compare_lt(id, parsed["value"].get<double>());
    } else if (type == "CompareGTE") {
      return make_compare_gte(id, parsed["value"].get<double>());
    } else if (type == "CompareLTE") {
      return make_compare_lte(id, parsed["value"].get<double>());
    } else if (type == "CompareEQ") {
      return make_compare_eq(id, parsed["value"].get<double>(), parsed.value("tolerance", 0.0));
    } else if (type == "CompareNEQ") {
      return make_compare_neq(id, parsed["value"].get<double>(), parsed.value("tolerance", 0.0));
    } else if (type == "CompareSyncGT") {
      return make_compare_sync_gt(id, max_size_per_port);
    } else if (type == "CompareSyncLT") {
      return make_compare_sync_lt(id, max_size_per_port);
    } else if (type == "CompareSyncGTE") {
      return make_compare_sync_gte(id, max_size_per_port);
    } else if (type == "CompareSyncLTE") {
      return make_compare_sync_lte(id, max_size_per_port);
    } else if (type == "CompareSyncEQ") {
      return make_compare_sync_eq(id, parsed.value("tolerance", 0.0), max_size_per_port);
    } else if (type == "CompareSyncNEQ") {
      return make_compare_sync_neq(id, parsed.value("tolerance", 0.0), max_size_per_port);
    } else if (type == "MovingKeyCount") {
      return make_moving_key_count(id, parsed["window_size"].get<size_t>());
    } else if (type == "MinTracker") {
      return make_min_tracker(id);
    } else if (type == "MaxTracker") {
      return make_max_tracker(id);
    } else if (type == "TopK") {
      return make_topk(id, parsed["k"].get<int>(),
                       parsed["score_index"].get<int>(),
                       parsed.value("descending", "true") == "true");
    } else if (type == "WindowMinMax") {
      return make_window_min_max(id, parsed["window_size"].get<size_t>(),
                                 parsed.value("mode", "min"));
    } else if (type == "BooleanToNumber") {
      return make_boolean_to_number(id);
    } else if (type == "KeyedPipeline") {
      auto key_index = parsed["key_index"].get<int>();

      // Build factory from embedded prototype definition
      const auto& proto = parsed["prototype"];
      json proto_operators = proto["operators"];
      json proto_connections = proto["connections"];
      std::string proto_entry = proto["entry"]["operator"].get<std::string>();
      std::string proto_output = proto["output"]["operator"].get<std::string>();

      auto factory = [proto_operators, proto_connections, proto_entry, proto_output]() -> SubGraph {
        SubGraph sg;
        for (const auto& op_json : proto_operators) {
          auto op = OperatorJson::read_op(op_json.dump());
          sg.operators[op->id()] = op;
        }
        for (const auto& conn : proto_connections) {
          std::string from = conn["from"].get<std::string>();
          std::string to = conn["to"].get<std::string>();
          auto from_parsed = conn.contains("fromPort") ? parse_port_name(conn["fromPort"].get<std::string>()) : ParsedPort{0, PortKind::DATA};
          auto to_parsed = conn.contains("toPort") ? parse_port_name(conn["toPort"].get<std::string>()) : ParsedPort{0, PortKind::DATA};
          sg.operators[from]->connect(sg.operators[to], from_parsed.index, to_parsed.index, to_parsed.kind);
        }
        sg.entry = sg.operators[proto_entry];
        sg.output = sg.operators[proto_output];
        return sg;
      };

      return make_keyed_pipeline(id, key_index, factory);
    } else if (type == "Pipeline") {
      // Validate port types
      auto input_types = parsed["input_port_types"].get<std::vector<std::string>>();
      auto output_types = parsed["output_port_types"].get<std::vector<std::string>>();

      for (const auto& t : input_types) {
        if (t != "number" && t != "boolean" && t != "vector_number" && t != "vector_boolean") {
          throw std::runtime_error("Invalid input port type: " + t);
        }
      }

      for (const auto& t : output_types) {
        if (t != "number" && t != "boolean" && t != "vector_number" && t != "vector_boolean") {
          throw std::runtime_error("Invalid output port type: " + t);
        }
      }

      if (input_types.empty() || output_types.empty()) {
        throw std::runtime_error("Pipeline must have at least one input and output port");
      }

      if (!parsed.contains("operators") || !parsed["operators"].is_array() || parsed["operators"].size() < 2) {
        throw std::runtime_error("Pipeline must contain at least two operators");
      }

      if (!parsed.contains("connections") || !parsed["connections"].is_array() || parsed["connections"].empty()) {
        throw std::runtime_error("Pipeline must contain at least one connection between operators");
      }

      auto pipeline = std::make_shared<Pipeline>(id, input_types, output_types);

      // Configure internal operators
      for (const auto& op_json : parsed["operators"]) {
        if (!op_json.contains("id") || !op_json.contains("type")) {
          throw std::runtime_error("Pipeline operators must have id and type fields");
        }
        pipeline->register_operator(read_op(op_json.dump()));
      }

      // Configure connections
      for (const auto& conn : parsed["connections"]) {
        if (!conn.contains("from") || !conn.contains("to")) {
          throw std::runtime_error("Pipeline connections must specify from and to operators");
        }
        std::string from = conn["from"].get<std::string>();
        std::string to = conn["to"].get<std::string>();
        size_t from_port = conn.contains("fromPort") ? parse_port_name(conn["fromPort"]).index : 0;
        size_t to_port = conn.contains("toPort") ? parse_port_name(conn["toPort"]).index : 0;
        pipeline->connect(from, to, from_port, to_port);
      }

      // Set entry operator
      std::string entry_id = parsed["entryOperator"].get<std::string>();
      pipeline->set_entry(entry_id);

      // Configure output mappings
      for (const auto& [op_id, mappings] : parsed["outputMappings"].items()) {
        for (const auto& [op_port, pipeline_port] : mappings.items()) {
          pipeline->add_output_mapping(op_id, parse_port_name(op_port).index, parse_port_name(pipeline_port).index);
        }
      }

      return pipeline;
    } else if (type == "TriggerSet") {
      // Validate single input/output port types
      if (!parsed.contains("input_port_type") || !parsed["input_port_type"].is_string()) {
        throw std::runtime_error("TriggerSet requires a string 'input_port_type' field");
      }
      if (!parsed.contains("output_port_type") || !parsed["output_port_type"].is_string()) {
        throw std::runtime_error("TriggerSet requires a string 'output_port_type' field");
      }

      auto input_type = parsed["input_port_type"].get<std::string>();
      auto output_type = parsed["output_port_type"].get<std::string>();

      auto valid_type = [](const std::string& t) {
        return t == "number" || t == "boolean" || t == "vector_number" || t == "vector_boolean";
      };
      if (!valid_type(input_type)) {
        throw std::runtime_error("Invalid input port type: " + input_type);
      }
      if (!valid_type(output_type)) {
        throw std::runtime_error("Invalid output port type: " + output_type);
      }

      if (!parsed.contains("operators") || !parsed["operators"].is_array() || parsed["operators"].empty()) {
        throw std::runtime_error("TriggerSet must contain at least one operator");
      }

      auto trigger_set = std::make_shared<TriggerSet>(id, input_type, output_type);

      // Configure internal operators
      for (const auto& op_json : parsed["operators"]) {
        if (!op_json.contains("id") || !op_json.contains("type")) {
          throw std::runtime_error("TriggerSet operators must have id and type fields");
        }
        trigger_set->register_operator(read_op(op_json.dump()));
      }

      // Configure connections (optional — a trigger set may consist of a single operator)
      if (parsed.contains("connections")) {
        if (!parsed["connections"].is_array()) {
          throw std::runtime_error("TriggerSet 'connections' must be an array");
        }
        for (const auto& conn : parsed["connections"]) {
          if (!conn.contains("from") || !conn.contains("to")) {
            throw std::runtime_error("TriggerSet connections must specify from and to operators");
          }
          std::string from = conn["from"].get<std::string>();
          std::string to = conn["to"].get<std::string>();
          size_t from_port = conn.contains("fromPort") ? parse_port_name(conn["fromPort"]).index : 0;
          size_t to_port = conn.contains("toPort") ? parse_port_name(conn["toPort"]).index : 0;
          trigger_set->connect(from, to, from_port, to_port);
        }
      }

      // Set entry operator
      if (!parsed.contains("entryOperator") || !parsed["entryOperator"].is_string()) {
        throw std::runtime_error("TriggerSet requires a string 'entryOperator' field");
      }
      trigger_set->set_entry(parsed["entryOperator"].get<std::string>());

      // Set output operator (single op + optional port; defaults to port o1)
      if (!parsed.contains("outputOperator")) {
        throw std::runtime_error("TriggerSet requires an 'outputOperator' field");
      }
      const auto& output_op_json = parsed["outputOperator"];
      if (output_op_json.is_string()) {
        trigger_set->set_output(output_op_json.get<std::string>(), 0);
      } else if (output_op_json.is_object()) {
        if (!output_op_json.contains("id") || !output_op_json["id"].is_string()) {
          throw std::runtime_error("TriggerSet 'outputOperator' object must contain a string 'id' field");
        }
        std::string out_id = output_op_json["id"].get<std::string>();
        size_t out_port = output_op_json.contains("port") ? parse_port_name(output_op_json["port"]).index : 0;
        trigger_set->set_output(out_id, out_port);
      } else {
        throw std::runtime_error("TriggerSet 'outputOperator' must be a string or an object");
      }

      return trigger_set;
    } else {
      throw std::runtime_error("Unknown operator type: " + type);
    }
  }

  static std::string write_op(std::shared_ptr<Operator> const& op) {
    auto type = op->type_name();
    json j;
    j["type"] = type;
    j["id"] = op->id();

    if (type == "Input") {
      j["portTypes"] = std::dynamic_pointer_cast<Input>(op)->get_port_types();
    } else if (type == "Output") {
      j["portTypes"] = std::dynamic_pointer_cast<Output>(op)->get_port_types();
    } else if (type == "MovingAverage") {
      j["window_size"] = std::dynamic_pointer_cast<MovingAverage>(op)->window_size();
    } else if (type == "MovingSum") {
      j["window_size"] = std::dynamic_pointer_cast<MovingSum>(op)->window_size();
    } else if (type == "StandardDeviation") {
      j["window_size"] = std::dynamic_pointer_cast<StandardDeviation>(op)->window_size();
    } else if (type == "FiniteImpulseResponse") {
      j["coeff"] = std::dynamic_pointer_cast<FiniteImpulseResponse>(op)->get_coefficients();
    } else if (type == "InfiniteImpulseResponse") {
      j["b_coeffs"] = std::dynamic_pointer_cast<InfiniteImpulseResponse>(op)->get_b_coeffs();
      j["a_coeffs"] = std::dynamic_pointer_cast<InfiniteImpulseResponse>(op)->get_a_coeffs();
    } else if (type == "Join") {
      j["portTypes"] = std::dynamic_pointer_cast<Join>(op)->get_port_types();
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "Scale") {
      j["value"] = std::dynamic_pointer_cast<Scale>(op)->get_value();
    } else if (type == "Power") {
      j["value"] = std::dynamic_pointer_cast<Power>(op)->get_value();
    } else if (type == "Add") {
      j["value"] = std::dynamic_pointer_cast<Add>(op)->get_value();
    } else if (type == "Linear") {
      j["coefficients"] = std::dynamic_pointer_cast<Linear>(op)->get_coefficients();
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "PeakDetector") {
      j["window_size"] = std::dynamic_pointer_cast<PeakDetector>(op)->window_size();
    } else if (type == "GreaterThan") {
      j["value"] = std::dynamic_pointer_cast<GreaterThan>(op)->get_threshold();
    } else if (type == "Function") {
      j["points"] = std::dynamic_pointer_cast<Function>(op)->get_points();
    } else if (type == "ConstantNumber" || type == "ConstantNumberToBoolean") {
      j["value"] = std::dynamic_pointer_cast<ConstantNumber>(op)->get_value().value;
    } else if (type == "ConstantBoolean" || type == "ConstantBooleanToNumber") {
      j["value"] = std::dynamic_pointer_cast<ConstantBoolean>(op)->get_value().value;
    } else if (type == "LessThan") {
      j["value"] = std::dynamic_pointer_cast<LessThan>(op)->get_threshold();
    } else if (type == "EqualTo") {
      j["value"] = std::dynamic_pointer_cast<EqualTo>(op)->get_value();
      j["epsilon"] = std::dynamic_pointer_cast<EqualTo>(op)->get_epsilon();
    } else if (type == "NotEqualTo") {
      j["value"] = std::dynamic_pointer_cast<NotEqualTo>(op)->get_value();
      j["epsilon"] = std::dynamic_pointer_cast<NotEqualTo>(op)->get_epsilon();
    } else if (type == "SyncEqual") {
      j["epsilon"] = std::dynamic_pointer_cast<SyncEqual>(op)->get_epsilon();
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "SyncNotEqual") {
      j["epsilon"] = std::dynamic_pointer_cast<SyncNotEqual>(op)->get_epsilon();
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "GreaterThan") {
      j["value"] = std::dynamic_pointer_cast<GreaterThan>(op)->get_threshold();
    } else if (type == "LessThanOrEqualToReplace") {
      j["value"] = std::dynamic_pointer_cast<LessThanOrEqualToReplace>(op)->get_threshold();
      j["replaceBy"] = std::dynamic_pointer_cast<LessThanOrEqualToReplace>(op)->get_replace_by();
    } else if (type == "Addition" || type == "Subtraction" || type == "Multiplication" || type == "Division") {
      j["numPorts"] = op->num_data_ports();
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "SyncGreaterThan" || type == "SyncLessThan") {
      j["numPorts"] = op->num_data_ports();
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "LogicalAnd" || type == "LogicalOr" || type == "LogicalXor" || type == "LogicalNand" ||
               type == "LogicalNor" || type == "LogicalImplication") {
      j["numPorts"] = std::dynamic_pointer_cast<BooleanSync>(op)->get_num_ports();
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "Sin" || type == "Cos" || type == "Tan" || type == "Exp" || type == "Log" || type == "Log10" ||
               type == "Abs" || type == "Sign" || type == "Floor" || type == "Ceil" || type == "Round") {
    } else if (type == "Variable") {
      j["default_value"] = std::dynamic_pointer_cast<Variable>(op)->get_default_value();
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "KeyedVariable") {
      auto kv = std::dynamic_pointer_cast<KeyedVariable>(op);
      j["mode"] = kv->get_mode();
      j["default_value"] = kv->get_default_value();
    } else if (type == "TimeShift") {
      j["shift"] = std::dynamic_pointer_cast<TimeShift>(op)->get_shift();
    } else if (type == "Demultiplexer") {
      if (auto d = std::dynamic_pointer_cast<Demultiplexer<VectorNumberData>>(op)) {
        j["numPorts"] = d->get_num_ports();
        j["portType"] = "vector_number";
      } else if (auto d = std::dynamic_pointer_cast<Demultiplexer<BooleanData>>(op)) {
        j["numPorts"] = d->get_num_ports();
        j["portType"] = "boolean";
      } else if (auto d = std::dynamic_pointer_cast<Demultiplexer<VectorBooleanData>>(op)) {
        j["numPorts"] = d->get_num_ports();
        j["portType"] = "vector_boolean";
      } else {
        j["numPorts"] = std::dynamic_pointer_cast<Demultiplexer<NumberData>>(op)->get_num_ports();
      }
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "Multiplexer") {
      j["numPorts"] = std::dynamic_pointer_cast<Multiplexer<NumberData>>(op)->get_num_ports();
    } else if (type == "ResamplerConstant") {
      auto resampler = std::dynamic_pointer_cast<ResamplerConstant<NumberData>>(op);
      j["interval"] = resampler->get_interval();
      if (resampler->get_t0().has_value()) {
        j["t0"] = resampler->get_t0().value();
      }
    } else if (type == "ResamplerHermite") {
      j["interval"] = std::dynamic_pointer_cast<ResamplerHermite>(op)->get_interval();
    } else if (type == "VectorExtract") {
      j["index"] = std::dynamic_pointer_cast<VectorExtract>(op)->get_index();
    } else if (type == "VectorProject") {
      j["indices"] = std::dynamic_pointer_cast<VectorProject>(op)->get_indices();
    } else if (type == "VectorCompose") {
      j["numPorts"] = std::dynamic_pointer_cast<VectorCompose>(op)->get_num_ports();
    } else if (type == "CompareGT") {
      j["value"] = std::dynamic_pointer_cast<CompareGT>(op)->get_value();
    } else if (type == "CompareLT") {
      j["value"] = std::dynamic_pointer_cast<CompareLT>(op)->get_value();
    } else if (type == "CompareGTE") {
      j["value"] = std::dynamic_pointer_cast<CompareGTE>(op)->get_value();
    } else if (type == "CompareLTE") {
      j["value"] = std::dynamic_pointer_cast<CompareLTE>(op)->get_value();
    } else if (type == "CompareEQ") {
      j["value"] = std::dynamic_pointer_cast<CompareEQ>(op)->get_value();
      j["tolerance"] = std::dynamic_pointer_cast<CompareEQ>(op)->get_tolerance();
    } else if (type == "CompareNEQ") {
      j["value"] = std::dynamic_pointer_cast<CompareNEQ>(op)->get_value();
      j["tolerance"] = std::dynamic_pointer_cast<CompareNEQ>(op)->get_tolerance();
    } else if (type == "CompareSyncGT" || type == "CompareSyncLT" ||
               type == "CompareSyncGTE" || type == "CompareSyncLTE") {
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "CompareSyncEQ") {
      j["tolerance"] = std::dynamic_pointer_cast<CompareSyncEQ>(op)->get_tolerance();
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "CompareSyncNEQ") {
      j["tolerance"] = std::dynamic_pointer_cast<CompareSyncNEQ>(op)->get_tolerance();
      j["maxSizePerPort"] = op->max_size_per_port();
    } else if (type == "MovingKeyCount") {
      j["window_size"] = std::dynamic_pointer_cast<MovingKeyCount>(op)->get_window_size();
    } else if (type == "MinTracker" || type == "MaxTracker") {
      // No parameters — stateful but no configuration
    } else if (type == "TopK") {
      auto tk = std::dynamic_pointer_cast<TopK>(op);
      j["k"] = tk->k();
      j["score_index"] = tk->score_index();
      j["descending"] = tk->descending() ? "true" : "false";
    } else if (type == "WindowMinMax") {
      auto wmm = std::dynamic_pointer_cast<WindowMinMax>(op);
      j["window_size"] = wmm->window_size();
      j["mode"] = wmm->is_min() ? "min" : "max";
    } else if (type == "BooleanToNumber") {
      // No parameters beyond id
    } else if (type == "KeyedPipeline") {
      auto kp = std::dynamic_pointer_cast<KeyedPipeline>(op);
      j["key_index"] = kp->get_key_index();
    } else if (type == "Pipeline") {
      auto pipeline = std::dynamic_pointer_cast<Pipeline>(op);
      j["type"] = "Pipeline";
      j["id"] = op->id();
      j["input_port_types"] = pipeline->get_input_port_types();
      j["output_port_types"] = pipeline->get_output_port_types();
      // Store operators
      j["operators"] = json::array();
      for (const auto& [op_id, internal_op] : pipeline->get_operators()) {
        json op_json = json::parse(write_op(internal_op));
        op_json["id"] = op_id;
        j["operators"].push_back(op_json);
      }

      // Store connections
      j["connections"] = json::array();
      for (const auto& conn : pipeline->get_connections()) {
        json conn_json;
        conn_json["from"] = conn.from_id;
        conn_json["to"] = conn.to_id;
        conn_json["fromPort"] = "o" + std::to_string(conn.from_port + 1);
        conn_json["toPort"] = "i" + std::to_string(conn.to_port + 1);
        j["connections"].push_back(conn_json);
      }

      // Store entry operator
      j["entryOperator"] = pipeline->get_entry_operator_id();

      // Store output mappings
      j["outputMappings"] = json::object();
      for (const auto& [op_id, mappings] : pipeline->get_output_mappings()) {
        json mapping_json;
        for (const auto& [op_port, pipeline_port] : mappings) {
          mapping_json["o" + std::to_string(op_port + 1)] = "o" + std::to_string(pipeline_port + 1);
        }
        j["outputMappings"][op_id] = mapping_json;
      }
    } else if (type == "TriggerSet") {
      auto trigger_set = std::dynamic_pointer_cast<TriggerSet>(op);
      j["type"] = "TriggerSet";
      j["id"] = op->id();
      j["input_port_type"] = trigger_set->get_input_port_type();
      j["output_port_type"] = trigger_set->get_output_port_type();

      // Store internal operators
      j["operators"] = json::array();
      for (const auto& [op_id, internal_op] : trigger_set->get_operators()) {
        json op_json = json::parse(write_op(internal_op));
        op_json["id"] = op_id;
        j["operators"].push_back(op_json);
      }

      // Store internal connections
      j["connections"] = json::array();
      for (const auto& conn : trigger_set->get_connections()) {
        json conn_json;
        conn_json["from"] = conn.from_id;
        conn_json["to"] = conn.to_id;
        conn_json["fromPort"] = "o" + std::to_string(conn.from_port + 1);
        conn_json["toPort"] = "i" + std::to_string(conn.to_port + 1);
        j["connections"].push_back(conn_json);
      }

      // Store entry and output operators
      j["entryOperator"] = trigger_set->get_entry_operator_id();
      json output_op_json;
      output_op_json["id"] = trigger_set->get_output_operator_id();
      output_op_json["port"] = "o" + std::to_string(trigger_set->get_output_operator_port() + 1);
      j["outputOperator"] = output_op_json;
    } else {
      throw std::runtime_error("Unknown operator type: " + type);
    }
    return j.dump();
  }
};

}  // namespace rtbot

#endif  // RTBOT_OPERATOR_JSON_H