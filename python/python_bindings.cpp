
#include <pybind11/numpy.h>
#include <pybind11/pytypes.h>
#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <pybind11/cast.h>

#include "rtbot/Buffer.h"

using namespace std;
using namespace rtbot;

namespace py = pybind11;
using namespace pybind11::literals;


PYBIND11_MODULE(rtbotpy, m) {
    m.doc() = "Python interface for real time bot library (rtbot)";
    

    using Bufferd=Buffer<double>;
    py::class_<Bufferd>(m,"Buffer")
            .def(py::init<int,int>(), "channelSize"_a=1, "windowSize"_a=1)
            .def_readonly("channelSize",&Bufferd::channelSize)
            .def_readonly("windowSize",&Bufferd::windowSize)
            .def("add",&Bufferd::add)
            .def("to_matrix",&Bufferd::to_matrix)
            ;

}

