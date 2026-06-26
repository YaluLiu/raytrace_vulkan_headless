#include "height_scan_provider.h"

#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(robot_raster_py, m) {
  py::class_<HeightScanProvider>(m, "HeightScanProvider")
      .def(py::init<const std::string &, int, int, const std::string &>(),
           py::arg("usd_path"), py::arg("width") = 64, py::arg("height") = 64,
           py::arg("plugin_search_root") = "")
      .def("set_height_scan_params", &HeightScanProvider::setHeightScanParams,
           py::arg("u_start"), py::arg("u_end"), py::arg("u_step"),
           py::arg("v_start"), py::arg("v_end"), py::arg("v_step"),
           py::arg("gravity_direction_ws") = py::make_tuple(0.0f, 0.0f, -1.0f),
           py::arg("max_range") = 5.0f)
      .def("compute_from_sensor_poses",
           &HeightScanProvider::computeFromSensorPoses, py::arg("positions"),
           py::arg("quaternions_wxyz"));
}
