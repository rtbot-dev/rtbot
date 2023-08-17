
#include <pybind11/cast.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>

#include "rtbot/bindings.h"

using namespace std;
using namespace rtbot;

namespace py = pybind11;
using namespace pybind11::literals;

PYBIND11_MODULE(rtbotapi, m) {
  m.doc() = "Python interface for rtbot";

  m.def("validate", &validate, "Validates json representation of an rtbot program");
  m.def("validateOperator", &validateOperator, "Validates a json representation of an rtbot operator");
  m.def("createProgram", &createProgram, "Register an rtbot program in memory");
  m.def("deleteProgram", &deleteProgram, "Deletes and frees the memory of a previsouly created rtbot program");
  m.def("sendMessage", &processMessageDebug, "Sends a message to the specified program, previously registered");
}
