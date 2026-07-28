#include "depth_camera_provider.h"
#include "height_scan_provider.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(robot_raster_py, m) {
  m.doc() = "Batched robot raster sensors backed by USD and Vulkan";

  py::class_<DepthCameraProvider>(m, "DepthCameraProvider")
      .def(py::init<const std::string &, uint32_t, int, int,
                    const std::string &, const std::string &>(),
           py::arg("usd_path"), py::arg("camera_count"),
           py::arg("width") = 64, py::arg("height") = 64,
           py::arg("camera_path") = "", py::arg("plugin_search_root") = "",
           R"doc(Load a USD scene and create a fixed batch of depth cameras.

If camera_path is provided, that USD camera supplies the projection settings
for every camera. Otherwise all authored USD cameras are preserved when their
count matches camera_count; in other cases the first authored camera (or a
default camera) is cloned.)doc")
      .def_property_readonly("camera_count",
                             &DepthCameraProvider::cameraCount)
      .def_property_readonly("width", &DepthCameraProvider::width)
      .def_property_readonly("height", &DepthCameraProvider::height)
      .def_property_readonly("camera_names",
                             &DepthCameraProvider::cameraNames)
      .def("update_camera_poses", &DepthCameraProvider::updateCameraPoses,
           py::arg("positions"), py::arg("quaternions_wxyz"),
           R"doc(Update every camera pose.

positions has shape (n, 3), quaternions_wxyz has shape (n, 4), and rotations
map camera-local forward -Z and up +Y into world space.)doc")
      .def("render_depth", &DepthCameraProvider::renderDepth,
           py::arg("linearize") = true,
           R"doc(Render float32 C-contiguous depth with shape (n, h, w).

With linearize=True, values are view-axis depth in USD scene units and the
background equals each camera's far clip. With False, values are Vulkan
normalized device depth in [0, 1], with background 1. Rows use a top-left
image origin.)doc")
      .def("compute_from_camera_poses",
           &DepthCameraProvider::computeFromCameraPoses,
           py::arg("positions"), py::arg("quaternions_wxyz"),
           py::arg("linearize") = true,
           "Update all poses and render one depth batch atomically.");

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
