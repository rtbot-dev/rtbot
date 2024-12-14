// python/rtbot_py.cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "rtbot/bindings.h"

namespace py = pybind11;

PYBIND11_MODULE(rtbotapi, m) {
  m.doc() = "Python bindings for RTBot framework";

  // Program lifecycle functions
  m.def("create_program", &rtbot::create_program, "Create a new program", py::arg("program_id"),
        py::arg("json_program"));
  m.def("delete_program", &rtbot::delete_program, "Delete a program", py::arg("program_id"));
  m.def("validate_program", &rtbot::validate_program, "Validate a program JSON", py::arg("json_program"));
  m.def("validate_operator", &rtbot::validate_operator, "Validate an operator", py::arg("type"), py::arg("json_op"));

  // Message handling
  m.def("add_to_message_buffer", &rtbot::add_to_message_buffer, "Add message to buffer", py::arg("program_id"),
        py::arg("port_id"), py::arg("time"), py::arg("value"));
  m.def("process_message_buffer", &rtbot::process_message_buffer, "Process message buffer", py::arg("program_id"));
  m.def("process_message_buffer_debug", &rtbot::process_message_buffer_debug, "Process message buffer in debug mode",
        py::arg("program_id"));

  // Batch operations
  m.def("process_batch", &rtbot::process_batch, "Process batch of messages", py::arg("program_id"), py::arg("times"),
        py::arg("values"), py::arg("ports"));
  m.def("process_batch_debug", &rtbot::process_batch_debug, "Process batch in debug mode", py::arg("program_id"),
        py::arg("times"), py::arg("values"), py::arg("ports"));

  // Pretty printing
  m.def("pretty_print", py::overload_cast<const std::string&>(&rtbot::pretty_print), "Pretty print JSON output",
        py::arg("json_output"));
}