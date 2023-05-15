
#include <pybind11/cast.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>

#include "rtbot/std/Matching.h"

using namespace std;
using namespace rtbot;

namespace py = pybind11;
using namespace pybind11::literals;

template<class T>
void defineInModule(py::module& m)
{
  using Matchingd = Matching<T>;
  py::class_<Matchingd>(m, "Matching")
      .def(py::init<vector<T>, vector<T>, function<bool(T,T)>>(), "A"_a, "B"_a, "match_condition"_a)
      .def(py::init<vector<T>, vector<T>>(), "A"_a, "B"_a)
      .def_readonly("matchA", &Matchingd::matchA)
      .def_readonly("matchB", &Matchingd::matchB)
      .def_readonly("extraA", &Matchingd::extraA)
      .def_readonly("extraB", &Matchingd::extraB)
      .def("fScore", &Matchingd::fScore)
      .def("averageDistanceOfMatched", &Matchingd::averageDistanceOfMatched)
      ;
}




PYBIND11_MODULE(rtbotpy, m) {
  m.doc() = "Python interface for real time bot library (rtbot)";
  defineInModule<double>(m);




}
