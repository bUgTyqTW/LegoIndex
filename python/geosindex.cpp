#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <Python.h>

#include <chrono>
#include <geosindex/rtreebuild.h>
#include <geosindex/rtreequery.h>

#include <geosindex/minmaxquery.h>
#include <geosindex/minmaxbuild.h>

namespace py = pybind11;

PYBIND11_MODULE(geosindex, m) {
    py::class_<geosindex::RTreeQuery>(m, "RTreeQuery")
            .def(py::init<const std::string&, const std::string&, const std::string&>()) // Constructor binding
            .def("queryRTreeXYZ", &geosindex::RTreeQuery::queryRTreeXYZ)
            .def("queryRTreeMetaData", &geosindex::RTreeQuery::queryRTreeMetaData)
            .def("queryRTreeMetaDataRoot", &geosindex::RTreeQuery::queryRTreeMetaDataRoot)
            .def("queryRTreeTracing", &geosindex::RTreeQuery::queryRTreeTracing)
            .def("queryRTreeTracingInteracted", &geosindex::RTreeQuery::queryRTreeTracingInteracted);

    py::class_<geosindex::QueryResult>(m, "QueryResult")
            .def(py::init<>())
            .def_readwrite("start", &geosindex::QueryResult::start)
            .def_readwrite("end", &geosindex::QueryResult::end);

    py::class_<geosindex::QueryBlockResult>(m, "QueryBlockResult")
            .def(py::init<>())
            .def_readwrite("start", &geosindex::QueryBlockResult::start)
            .def_readwrite("end", &geosindex::QueryBlockResult::end)
            .def_readwrite("q", &geosindex::QueryBlockResult::q);

    py::class_<geosindex::TracingResult>(m, "TracingResult")
            .def(py::init<>())
            .def_readwrite("start", &geosindex::TracingResult::start)
            .def_readwrite("end", &geosindex::TracingResult::end)
            .def_readwrite("id_data", &geosindex::TracingResult::id_data);

    py::class_<geos::geom::Envelope3d>(m, "Envelope3d")
            .def(py::init<double, double, double, double, double, double, int, int>())
            .def_property_readonly("minx", &geos::geom::Envelope3d::getMinX)
            .def_property_readonly("maxx", &geos::geom::Envelope3d::getMaxX)
            .def_property_readonly("miny", &geos::geom::Envelope3d::getMinY)
            .def_property_readonly("maxy", &geos::geom::Envelope3d::getMaxY)
            .def_property_readonly("minz", &geos::geom::Envelope3d::getMinZ)
            .def_property_readonly("maxz", &geos::geom::Envelope3d::getMaxZ)
            .def_property_readonly("start", &geos::geom::Envelope3d::getStart)
            .def_property_readonly("end", &geos::geom::Envelope3d::getEnd);

    py::class_<geosindex::MinMaxQuery>(m, "MinMaxQuery")
            .def(py::init<const std::string&, const std::string&, const std::string&>())
            .def("queryMinMaxData", &geosindex::MinMaxQuery::queryMinMaxData);

}

