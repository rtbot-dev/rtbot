
#include <pybind11/cast.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>

#include "rtbot/std/Matching.h"
#include "rtbot/Pipeline.h"
#include "rtbot/CostFunction.h"

using namespace std;
using namespace rtbot;

namespace py = pybind11;
using namespace pybind11::literals;

template<class T>
void defineInModule(py::module& m)
{
  py::class_<Matching<T>>(m, "Matching")
      .def(py::init<vector<T>, vector<T>, function<bool(T,T)>>(), "A"_a, "B"_a, "match_condition"_a)
      .def(py::init<vector<T>, vector<T>>(), "A"_a, "B"_a)
      .def_readonly("matchA", &Matching<T>::matchA)
      .def_readonly("matchB", &Matching<T>::matchB)
      .def_readonly("extraA", &Matching<T>::extraA)
      .def_readonly("extraB", &Matching<T>::extraB)
      .def("fScore", &Matching<T>::fScore)
      .def("averageDistanceOfMatched", &Matching<T>::averageDistanceOfMatched)
      .def("cost", &Matching<T>::cost)
      ;

  using Msg=Message<std::uint64_t,double>;
  py::class_<Msg>(m,"Message")
      .def(py::init<std::int64_t,double>(),"time"_a=0, "value"_a=0)
      .def_readwrite("time",&Msg::time)
      .def_readwrite("value",&Msg::value)
      ;

  py::class_<Pipeline>(m,"Pipeline")
      .def(py::init<string>(),"json_prog"_a)
      .def("receiveDebug", &Pipeline::receiveDebug);
      ;


  py::class_<Sample>(m,"Sample")
          .def(py::init<vector<Message<>>, vector<std::uint64_t>>(),"messages"_a, "expected_times"_a)
          .def_readwrite("msgs",&Sample::msgs)
          .def_readwrite("value",&Sample::expected_times)
          ;

  py::class_<CostFunction>(m, "CostFuncion")
          .def(py::init<string, string, vector<Sample>>(), "prog_json"_a, "params_json"_a, "samples"_a)
          .def("__eval__", &CostFunction::operator())
          .def("get_prog_json", &CostFunction::get_prog_json)
          ;
}




PYBIND11_MODULE(rtbotpy, m) {
  m.doc() = "Python interface for real time bot library (rtbot)";
  defineInModule<double>(m);




}
