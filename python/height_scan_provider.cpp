#include "height_scan_provider.h"

#include "training_scene.h"
#include "usd_scene_loader.h"

#include <pybind11/pybind11.h>

#include <glm/ext/quaternion_float.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace py = pybind11;

namespace {
constexpr float kDirectionEpsilon = 1.0e-6f;

glm::vec3 TupleToVec3(const py::tuple &value, const char *name) {
  if (value.size() != 3) {
    throw std::invalid_argument(std::string(name) +
                                " must contain exactly 3 floats");
  }
  return glm::vec3(value[0].cast<float>(), value[1].cast<float>(),
                   value[2].cast<float>());
}

bool IsFinite(const glm::vec3 &value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

glm::vec3 NormalizeOr(glm::vec3 value, glm::vec3 fallback) {
  if (!IsFinite(value) ||
      glm::dot(value, value) < kDirectionEpsilon * kDirectionEpsilon) {
    value = fallback;
  }
  if (!IsFinite(value) ||
      glm::dot(value, value) < kDirectionEpsilon * kDirectionEpsilon) {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }
  return glm::normalize(value);
}

glm::quat QuaternionFromWxyz(float w, float x, float y, float z) {
  glm::quat quat(w, x, y, z);
  const float length = glm::length(quat);
  if (!std::isfinite(length) || length < kDirectionEpsilon) {
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  }
  return glm::normalize(quat);
}

std::string SensorName(size_t index) {
  return "/PythonHeightScan/env_" + std::to_string(index);
}

size_t CheckedSampleCount(const HeightScanSensorGrid &grid) {
  const uint64_t count =
      static_cast<uint64_t>(grid.width) * static_cast<uint64_t>(grid.height);
  if (count > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
    throw std::runtime_error(
        "height scan sample count exceeds host size_t range");
  }
  return static_cast<size_t>(count);
}
} // namespace

HeightScanProvider::HeightScanProvider(const std::string &usdPath, int width,
                                       int height,
                                       const std::string &pluginSearchRoot) {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("width and height must be positive");
  }

  headless_training::TrainingSceneDescription scene =
      headless_training::LoadUsdTrainingScene(std::filesystem::path(usdPath));
  m_runtime = std::make_unique<headless_training::TrainingSceneRuntime>(
      std::move(scene));

  if (!pluginSearchRoot.empty()) {
    m_engine.setPluginSearchRoot(pluginSearchRoot);
  }
  m_engine.setup(width, height);
  m_runtime->uploadToEngine(m_engine);
  m_runtime->configureEngineOutputs(m_engine, false, false);
  m_engine.createRenderResources();
}

HeightScanProvider::~HeightScanProvider() { m_engine.cleanup(); }

void HeightScanProvider::setHeightScanParams(float uStart, float uEnd,
                                             float uStep, float vStart,
                                             float vEnd, float vStep,
                                             py::tuple gravityDirectionWs,
                                             float maxRange) {
  std::scoped_lock lock(m_mutex);
  m_params.uStart = uStart;
  m_params.uEnd = uEnd;
  m_params.uStep = uStep;
  m_params.vStart = vStart;
  m_params.vEnd = vEnd;
  m_params.vStep = vStep;
  m_params.gravityDirectionWs =
      TupleToVec3(gravityDirectionWs, "gravity_direction_ws");
  m_params.maxRange = maxRange;
}

py::array_t<float> HeightScanProvider::computeFromSensorPoses(
    py::array_t<float, py::array::c_style | py::array::forcecast> positions,
    py::array_t<float, py::array::c_style | py::array::forcecast>
        quaternionsWxyz) {
  std::scoped_lock lock(m_mutex);
  const py::buffer_info positionsInfo = positions.request();
  const py::buffer_info quaternionsInfo = quaternionsWxyz.request();
  if (positionsInfo.ndim != 2 || positionsInfo.shape[1] != 3) {
    throw std::invalid_argument("positions must have shape (num_envs, 3)");
  }
  if (quaternionsInfo.ndim != 2 || quaternionsInfo.shape[1] != 4) {
    throw std::invalid_argument(
        "quaternions_wxyz must have shape (num_envs, 4)");
  }
  if (positionsInfo.shape[0] != quaternionsInfo.shape[0]) {
    throw std::invalid_argument(
        "positions and quaternions_wxyz must have the same num_envs");
  }

  const size_t sensorCount = static_cast<size_t>(positionsInfo.shape[0]);
  const float *positionData = static_cast<const float *>(positionsInfo.ptr);
  const float *quaternionData = static_cast<const float *>(quaternionsInfo.ptr);

  std::vector<HeightScanSensorSpec> sensors;
  sensors.reserve(sensorCount);
  m_sensorNames.resize(sensorCount);
  for (size_t i = 0; i < sensorCount; ++i) {
    m_sensorNames[i] = SensorName(i);
    const float *position = positionData + i * 3;
    const float *quaternion = quaternionData + i * 4;
    const glm::quat rotation = QuaternionFromWxyz(quaternion[0], quaternion[1],
                                                  quaternion[2], quaternion[3]);

    HeightScanSensorSpec sensor;
    sensor.name = m_sensorNames[i];
    sensor.position = glm::vec3(position[0], position[1], position[2]);
    sensor.forward = NormalizeOr(rotation * glm::vec3(0.0f, 1.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    sensor.up = NormalizeOr(rotation * glm::vec3(0.0f, 0.0f, 1.0f),
                            glm::vec3(0.0f, 0.0f, 1.0f));
    sensor.params = m_params;
    sensors.push_back(std::move(sensor));
  }

  m_runtime->setHeightScanSensors(std::move(sensors));
  m_runtime->configureEngineOutputs(m_engine, false, true);
  m_engine.render();
  const HeightScanFrame frame = m_engine.readHeightScanFrame();
  if (frame.sensors.size() != sensorCount) {
    throw std::runtime_error(
        "height scan readback did not return the requested sensor count");
  }

  size_t samplesPerSensor = 0;
  if (sensorCount > 0) {
    samplesPerSensor = CheckedSampleCount(frame.sensors.front());
    if (frame.sensors.front().samples.size() != samplesPerSensor) {
      throw std::runtime_error(
          "height scan readback sample count does not match grid dimensions");
    }
  }

  const std::vector<py::ssize_t> resultShape{
      static_cast<py::ssize_t>(sensorCount),
      static_cast<py::ssize_t>(samplesPerSensor), 3};
  py::array_t<float> result(resultShape);
  py::buffer_info resultInfo = result.request();
  float *output = static_cast<float *>(resultInfo.ptr);

  for (size_t sensorIndex = 0; sensorIndex < sensorCount; ++sensorIndex) {
    const HeightScanSensorGrid &grid = frame.sensors[sensorIndex];
    const size_t gridSamples = CheckedSampleCount(grid);
    if (gridSamples != samplesPerSensor ||
        grid.samples.size() != samplesPerSensor) {
      throw std::runtime_error(
          "height scan sensors returned inconsistent grid sizes");
    }

    for (size_t sampleIndex = 0; sampleIndex < samplesPerSensor;
         ++sampleIndex) {
      const glm::vec3 positionWs = grid.samples[sampleIndex].positionWs;
      const size_t offset = (sensorIndex * samplesPerSensor + sampleIndex) * 3;
      output[offset + 0] = positionWs.x;
      output[offset + 1] = positionWs.y;
      output[offset + 2] = positionWs.z;
    }
  }

  return result;
}
