#include "depth_camera_provider.h"

#include "training_scene.h"
#include "usd_scene_loader.h"

#include <pybind11/pybind11.h>

#include <glm/ext/quaternion_float.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace py = pybind11;

namespace {
constexpr float kQuaternionEpsilon = 1.0e-6f;

std::vector<CameraSpec> BuildCameraTemplates(
    const headless_training::TrainingSceneDescription &scene,
    uint32_t cameraCount, const std::string &cameraPath) {
  if (cameraCount == 0) {
    throw std::invalid_argument("camera_count must be positive");
  }

  if (cameraPath.empty() && scene.cameras.size() == cameraCount) {
    return scene.cameras;
  }

  CameraSpec cameraTemplate = headless_training::MakeDefaultCamera();
  if (!cameraPath.empty()) {
    const auto cameraIt =
        std::find_if(scene.cameras.begin(), scene.cameras.end(),
                     [&cameraPath](const CameraSpec &camera) {
                       return camera.name == cameraPath;
                     });
    if (cameraIt == scene.cameras.end()) {
      throw std::runtime_error("requested camera path was not loaded: " +
                               cameraPath);
    }
    cameraTemplate = *cameraIt;
  } else if (!scene.cameras.empty()) {
    cameraTemplate = scene.cameras.front();
  }

  std::vector<CameraSpec> cameras(cameraCount, cameraTemplate);
  for (uint32_t cameraIndex = 0; cameraIndex < cameraCount; ++cameraIndex) {
    cameras[cameraIndex].name =
        "/PythonDepthCamera/camera_" + std::to_string(cameraIndex);
  }
  return cameras;
}

TileAtlasConfig BuildTileAtlasConfig(uint32_t cameraCount, uint32_t width,
                                     uint32_t height) {
  uint32_t columns =
      static_cast<uint32_t>(std::ceil(std::sqrt(cameraCount)));
  columns = std::max(columns, 1u);
  while (static_cast<uint64_t>(columns) * columns < cameraCount) {
    ++columns;
  }
  const uint32_t rows = cameraCount / columns +
                        static_cast<uint32_t>(cameraCount % columns != 0);

  const uint64_t atlasWidth = static_cast<uint64_t>(columns) * width;
  const uint64_t atlasHeight = static_cast<uint64_t>(rows) * height;
  if (atlasWidth > std::numeric_limits<uint32_t>::max() ||
      atlasHeight > std::numeric_limits<uint32_t>::max()) {
    throw std::invalid_argument(
        "camera_count and image dimensions overflow the tile atlas extent");
  }

  TileAtlasConfig config;
  config.enabled = true;
  config.colorEnabled = false;
  config.depthEnabled = true;
  config.tilePixelWidth = width;
  config.tilePixelHeight = height;
  config.gridColumns = columns;
  config.gridRows = rows;
  return config;
}

bool IsFinite(float value) { return std::isfinite(value); }

std::vector<CameraSpec> BuildCamerasFromPoses(
    const std::vector<CameraSpec> &cameraTemplates,
    const py::array_t<float,
                      py::array::c_style | py::array::forcecast> &positions,
    const py::array_t<float,
                      py::array::c_style | py::array::forcecast>
        &quaternionsWxyz) {
  const py::buffer_info positionsInfo = positions.request();
  const py::buffer_info quaternionsInfo = quaternionsWxyz.request();
  if (positionsInfo.ndim != 2 || positionsInfo.shape[1] != 3) {
    throw std::invalid_argument("positions must have shape (camera_count, 3)");
  }
  if (quaternionsInfo.ndim != 2 || quaternionsInfo.shape[1] != 4) {
    throw std::invalid_argument(
        "quaternions_wxyz must have shape (camera_count, 4)");
  }
  if (positionsInfo.shape[0] != quaternionsInfo.shape[0]) {
    throw std::invalid_argument(
        "positions and quaternions_wxyz must have the same camera_count");
  }
  if (positionsInfo.shape[0] !=
      static_cast<py::ssize_t>(cameraTemplates.size())) {
    throw std::invalid_argument(
        "pose batch camera_count does not match the provider camera_count");
  }

  const auto *positionData = static_cast<const float *>(positionsInfo.ptr);
  const auto *quaternionData =
      static_cast<const float *>(quaternionsInfo.ptr);
  std::vector<CameraSpec> cameras = cameraTemplates;
  for (size_t cameraIndex = 0; cameraIndex < cameras.size(); ++cameraIndex) {
    const float *position = positionData + cameraIndex * 3;
    const float *quaternion = quaternionData + cameraIndex * 4;
    if (!IsFinite(position[0]) || !IsFinite(position[1]) ||
        !IsFinite(position[2])) {
      throw std::invalid_argument("positions must contain only finite values");
    }
    if (!IsFinite(quaternion[0]) || !IsFinite(quaternion[1]) ||
        !IsFinite(quaternion[2]) || !IsFinite(quaternion[3])) {
      throw std::invalid_argument(
          "quaternions_wxyz must contain only finite values");
    }

    glm::quat rotation(quaternion[0], quaternion[1], quaternion[2],
                       quaternion[3]);
    const float quaternionLength = glm::length(rotation);
    if (!std::isfinite(quaternionLength) ||
        quaternionLength < kQuaternionEpsilon) {
      throw std::invalid_argument(
          "quaternions_wxyz must contain non-zero quaternions");
    }
    rotation = glm::normalize(rotation);

    CameraSpec &camera = cameras[cameraIndex];
    camera.position = glm::vec3(position[0], position[1], position[2]);
    camera.forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    camera.up = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
  }
  return cameras;
}

void LinearizeDepth(TileDepthFrame &frame,
                    const std::vector<CameraSpec> &cameras) {
  const size_t pixelsPerCamera =
      static_cast<size_t>(frame.width) * frame.height;
  for (uint32_t cameraIndex = 0; cameraIndex < frame.cameraCount;
       ++cameraIndex) {
    float *cameraDepth = frame.values.data() + cameraIndex * pixelsPerCamera;
    for (size_t pixelIndex = 0; pixelIndex < pixelsPerCamera; ++pixelIndex) {
      cameraDepth[pixelIndex] = LinearizePerspectiveDepth(
          cameraDepth[pixelIndex], cameras[cameraIndex].clipStart,
          cameras[cameraIndex].clipEnd);
    }
  }
}

py::array_t<float> MakeDepthArray(TileDepthFrame frame) {
  auto values =
      std::make_unique<std::vector<float>>(std::move(frame.values));
  py::capsule owner(values.get(), [](void *pointer) {
    delete static_cast<std::vector<float> *>(pointer);
  });
  float *data = values->data();
  values.release();

  const std::vector<py::ssize_t> shape{
      static_cast<py::ssize_t>(frame.cameraCount),
      static_cast<py::ssize_t>(frame.height),
      static_cast<py::ssize_t>(frame.width)};
  const std::vector<py::ssize_t> strides{
      static_cast<py::ssize_t>(frame.height) * frame.width *
          static_cast<py::ssize_t>(sizeof(float)),
      static_cast<py::ssize_t>(frame.width) *
          static_cast<py::ssize_t>(sizeof(float)),
      static_cast<py::ssize_t>(sizeof(float))};
  return py::array_t<float>(shape, strides, data, owner);
}
} // namespace

DepthCameraProvider::DepthCameraProvider(const std::string &usdPath,
                                         uint32_t cameraCount, int width,
                                         int height,
                                         const std::string &cameraPath,
                                         const std::string &pluginSearchRoot) {
  if (cameraCount == 0) {
    throw std::invalid_argument("camera_count must be positive");
  }
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("width and height must be positive");
  }

  headless_training::UsdSceneLoadOptions loadOptions;
  loadOptions.cameraPath = cameraPath;
  headless_training::TrainingSceneDescription scene =
      headless_training::LoadUsdTrainingScene(std::filesystem::path(usdPath),
                                              loadOptions);
  m_cameraTemplates = BuildCameraTemplates(scene, cameraCount, cameraPath);
  m_cameras = m_cameraTemplates;
  m_width = static_cast<uint32_t>(width);
  m_height = static_cast<uint32_t>(height);
  m_runtime = std::make_unique<headless_training::TrainingSceneRuntime>(
      std::move(scene));

  if (!pluginSearchRoot.empty()) {
    m_engine.setPluginSearchRoot(pluginSearchRoot);
  }
  m_engine.setup(width, height);
  if (!m_engine.isTileMultiviewSupported()) {
    throw std::runtime_error(
        "the selected Vulkan device does not support multiview depth cameras");
  }

  m_runtime->uploadToEngine(m_engine);
  m_engine.setCameras(m_cameras);
  m_engine.setMainCamera(m_cameras.front());

  RendererOutputConfig outputConfig;
  outputConfig.tile.atlas =
      BuildTileAtlasConfig(cameraCount, m_width, m_height);
  outputConfig.tile.requestedChannels =
      TileAovChannelMask::FromChannel(TileAovChannel::Depth);
  m_engine.configureOutputs(std::move(outputConfig));
  m_engine.createRenderResources();
}

DepthCameraProvider::~DepthCameraProvider() { m_engine.cleanup(); }

void DepthCameraProvider::updateCameraPoses(
    py::array_t<float, py::array::c_style | py::array::forcecast> positions,
    py::array_t<float, py::array::c_style | py::array::forcecast>
        quaternionsWxyz) {
  std::vector<CameraSpec> cameras = BuildCamerasFromPoses(
      m_cameraTemplates, positions, quaternionsWxyz);
  py::gil_scoped_release release;
  std::scoped_lock lock(m_mutex);
  m_cameras = std::move(cameras);
  m_engine.setCameras(m_cameras);
  m_engine.setMainCamera(m_cameras.front());
}

py::array_t<float> DepthCameraProvider::renderDepth(bool linearize) {
  TileDepthFrame frame;
  {
    py::gil_scoped_release release;
    std::scoped_lock lock(m_mutex);
    frame = renderDepthFrame(linearize);
  }
  return MakeDepthArray(std::move(frame));
}

py::array_t<float> DepthCameraProvider::computeFromCameraPoses(
    py::array_t<float, py::array::c_style | py::array::forcecast> positions,
    py::array_t<float, py::array::c_style | py::array::forcecast>
        quaternionsWxyz,
    bool linearize) {
  std::vector<CameraSpec> cameras = BuildCamerasFromPoses(
      m_cameraTemplates, positions, quaternionsWxyz);
  TileDepthFrame frame;
  {
    py::gil_scoped_release release;
    std::scoped_lock lock(m_mutex);
    m_cameras = std::move(cameras);
    m_engine.setCameras(m_cameras);
    m_engine.setMainCamera(m_cameras.front());
    frame = renderDepthFrame(linearize);
  }
  return MakeDepthArray(std::move(frame));
}

std::vector<std::string> DepthCameraProvider::cameraNames() const {
  std::vector<std::string> names;
  names.reserve(m_cameraTemplates.size());
  for (const CameraSpec &camera : m_cameraTemplates) {
    names.push_back(camera.name);
  }
  return names;
}

TileDepthFrame DepthCameraProvider::renderDepthFrame(bool linearize) {
  m_engine.render();
  TileDepthFrame frame = m_engine.readTileDepthFrame();
  const uint64_t expectedValueCount =
      static_cast<uint64_t>(m_cameras.size()) * m_width * m_height;
  if (frame.cameraCount != m_cameras.size() || frame.width != m_width ||
      frame.height != m_height || frame.values.size() != expectedValueCount) {
    throw std::runtime_error(
        "tile depth render/readback did not return the requested [n,h,w] "
        "batch; check Vulkan multiview, atlas size, and device limits");
  }
  if (linearize) {
    LinearizeDepth(frame, m_cameras);
  }
  return frame;
}
